"""
/* Copyright (c) 2022 Amazon
   Written by Jan Buethe */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import os
import argparse

import torch
import tqdm

from rdovae import RDOVAE, RDOVAEDataset, distortion_loss, hard_rate_estimate, soft_rate_estimate


parser = argparse.ArgumentParser()

parser.add_argument('features', type=str, help='path to feature file in .f32 format')
parser.add_argument('output', type=str, help='path to output folder')

parser.add_argument('--cuda-visible-devices', type=str, help="comma separates list of cuda visible device indices, default: ''", default="")


model_group = parser.add_argument_group(title="model parameters")
model_group.add_argument('--latent-dim', type=int, help="number of symbols produces by encoder, default: 80", default=80)
model_group.add_argument('--cond-size', type=int, help="first conditioning size, default: 256", default=256)
model_group.add_argument('--cond-size2', type=int, help="second conditioning size, default: 256", default=256)
model_group.add_argument('--state-dim', type=int, help="dimensionality of transfered state, default: 24", default=24)
model_group.add_argument('--quant-levels', type=int, help="number of quantization levels, default: 16", default=16)
model_group.add_argument('--lambda-min', type=float, help="minimal value for rate lambda, default: 0.0002", default=2e-4)
model_group.add_argument('--lambda-max', type=float, help="maximal value for rate lambda, default: 0.0104", default=0.0104)
model_group.add_argument('--pvq-num-pulses', type=int, help="number of pulses for PVQ, default: 82", default=82)
model_group.add_argument('--state-dropout-rate', type=float, help="state dropout rate, default: 0", default=0.0)

training_group = parser.add_argument_group(title="training parameters")
training_group.add_argument('--batch-size', type=int, help="batch size, default: 32", default=32)
training_group.add_argument('--lr', type=float, help='learning rate, default: 3e-4', default=3e-4)
training_group.add_argument('--epochs', type=int, help='number of training epochs, default: 100', default=100)
training_group.add_argument('--sequence-length', type=int, help='sequence length, needs to be divisible by 4, default: 256', default=256)
training_group.add_argument('--lr-decay-factor', type=float, help='learning rate decay factor, default: 2.5e-5', default=2.5e-5)
training_group.add_argument('--split-mode', type=str, choices=['split', 'random_split'], help='splitting mode for decoder input, default: split', default='split')
training_group.add_argument('--enable-first-frame-loss', action='store_true', default=False, help='enables dedicated distortion loss on first 4 decoder frames')
training_group.add_argument('--initial-checkpoint', type=str, help='initial checkpoint to start training from, default: None', default=None)
training_group.add_argument('--train-decoder-only', action='store_true', help='freeze encoder and statistical model and train decoder only')

args = parser.parse_args()

# set visible devices
os.environ['CUDA_VISIBLE_DEVICES'] = args.cuda_visible_devices

# checkpoints
checkpoint_dir = os.path.join(args.output, 'checkpoints')
checkpoint = dict()
os.makedirs(checkpoint_dir, exist_ok=True)

# training parameters
batch_size = args.batch_size
lr = args.lr
epochs = args.epochs
sequence_length = args.sequence_length
lr_decay_factor = args.lr_decay_factor
split_mode = args.split_mode
# not exposed
adam_betas = [0.8, 0.95]
adam_eps = 1e-8

checkpoint['batch_size'] = batch_size
checkpoint['lr'] = lr
checkpoint['lr_decay_factor'] = lr_decay_factor
checkpoint['split_mode'] = split_mode
checkpoint['epochs'] = epochs
checkpoint['sequence_length'] = sequence_length
checkpoint['adam_betas'] = adam_betas

# logging
log_interval = 10

# device
device = torch.device("cuda") if torch.cuda.is_available() else torch.device("cpu")

# model parameters
cond_size  = args.cond_size
cond_size2 = args.cond_size2
latent_dim = args.latent_dim
quant_levels = args.quant_levels
lambda_min = args.lambda_min
lambda_max = args.lambda_max
state_dim = args.state_dim
# not expsed
num_features = 20


# training data
feature_file = args.features

# model
checkpoint['model_args']    = (num_features, latent_dim, quant_levels, cond_size, cond_size2)
checkpoint['model_kwargs']  = {'state_dim': state_dim, 'split_mode' : split_mode, 'pvq_num_pulses': args.pvq_num_pulses, 'state_dropout_rate': args.state_dropout_rate}
model = RDOVAE(*checkpoint['model_args'], **checkpoint['model_kwargs'])

if type(args.initial_checkpoint) != type(None):
    checkpoint = torch.load(args.initial_checkpoint, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=False)

checkpoint['state_dict']    = model.state_dict()

if args.train_decoder_only:
    if args.initial_checkpoint is None:
        print("warning: training decoder only without providing initial checkpoint")

    for p in model.core_encoder.module.parameters():
        p.requires_grad = False

    for p in model.statistical_model.parameters():
        p.requires_grad = False

# dataloader
checkpoint['dataset_args'] = (feature_file, sequence_length, num_features, 36)
checkpoint['dataset_kwargs'] = {'lambda_min': lambda_min, 'lambda_max': lambda_max, 'enc_stride': model.enc_stride, 'quant_levels': quant_levels}
dataset = RDOVAEDataset(*checkpoint['dataset_args'], **checkpoint['dataset_kwargs'])
dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True, num_workers=4)



# optimizer
params = [p for p in model.parameters() if p.requires_grad]
optimizer = torch.optim.Adam(params, lr=lr, betas=adam_betas, eps=adam_eps)


# learning rate scheduler
scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay_factor * x))

if __name__ == '__main__':

    # push model to device
    model.to(device)

    # training loop

    for epoch in range(1, epochs + 1):

        print(f"training epoch {epoch}...")

        # running stats
        running_rate_loss       = 0
        running_soft_dist_loss  = 0
        running_hard_dist_loss  = 0
        running_hard_rate_loss  = 0
        running_soft_rate_loss  = 0
        running_total_loss      = 0
        running_rate_metric     = 0
        running_states_rate_metric     = 0
        previous_total_loss     = 0
        running_first_frame_loss = 0

        with tqdm.tqdm(dataloader, unit='batch') as tepoch:
            for i, (features, rate_lambda, q_ids) in enumerate(tepoch):

                # zero out gradients
                optimizer.zero_grad()

                # push inputs to device
                features    = features.to(device)
                q_ids       = q_ids.to(device)
                rate_lambda = rate_lambda.to(device)


                rate_lambda_upsamp = torch.repeat_interleave(rate_lambda, 2, 1)

                # run model
                model_output = model(features, q_ids)

                # collect outputs
                z                   = model_output['z']
                states              = model_output['states']
                outputs_hard_quant  = model_output['outputs_hard_quant']
                outputs_soft_quant  = model_output['outputs_soft_quant']
                statistical_model   = model_output['statistical_model']

                # rate loss
                hard_rate = hard_rate_estimate(z, statistical_model['r_hard'][:,:,:latent_dim], statistical_model['theta_hard'][:,:,:latent_dim], reduce=False)
                soft_rate = soft_rate_estimate(z, statistical_model['r_soft'][:,:,:latent_dim], reduce=False)
                states_hard_rate = hard_rate_estimate(states, statistical_model['r_hard'][:,:,latent_dim:], statistical_model['theta_hard'][:,:,latent_dim:], reduce=False)
                states_soft_rate = soft_rate_estimate(states, statistical_model['r_soft'][:,:,latent_dim:], reduce=False)
                soft_rate_loss = torch.mean(torch.sqrt(rate_lambda) * (soft_rate + .02*states_soft_rate))
                hard_rate_loss = torch.mean(torch.sqrt(rate_lambda) * (hard_rate + .02*states_hard_rate))
                rate_loss = (soft_rate_loss + 0.1 * hard_rate_loss)
                hard_rate_metric = torch.mean(hard_rate)
                states_rate_metric = torch.mean(states_hard_rate)

                ## distortion losses

                # hard quantized decoder input
                distortion_loss_hard_quant = torch.zeros_like(rate_loss)
                for dec_features, start, stop in outputs_hard_quant:
                    distortion_loss_hard_quant += distortion_loss(features[..., start : stop, :], dec_features, rate_lambda_upsamp[..., start : stop]) / len(outputs_hard_quant)

                first_frame_loss = torch.zeros_like(rate_loss)
                for dec_features, start, stop in outputs_hard_quant:
                    first_frame_loss += distortion_loss(features[..., stop-4 : stop, :], dec_features[..., -4:, :], rate_lambda_upsamp[..., stop - 4 : stop]) / len(outputs_hard_quant)

                # soft quantized decoder input
                distortion_loss_soft_quant = torch.zeros_like(rate_loss)
                for dec_features, start, stop in outputs_soft_quant:
                    distortion_loss_soft_quant += distortion_loss(features[..., start : stop, :], dec_features, rate_lambda_upsamp[..., start : stop]) / len(outputs_soft_quant)

                # total loss
                total_loss = rate_loss + (distortion_loss_hard_quant + distortion_loss_soft_quant) / 2

                if args.enable_first_frame_loss:
                    total_loss = .97*total_loss + 0.03 * first_frame_loss


                total_loss.backward()

                optimizer.step()

                model.clip_weights()
                model.sparsify()

                scheduler.step()

                # collect running stats
                running_hard_dist_loss  += float(distortion_loss_hard_quant.detach().cpu())
                running_soft_dist_loss  += float(distortion_loss_soft_quant.detach().cpu())
                running_rate_loss       += float(rate_loss.detach().cpu())
                running_rate_metric     += float(hard_rate_metric.detach().cpu())
                running_states_rate_metric     += float(states_rate_metric.detach().cpu())
                running_total_loss      += float(total_loss.detach().cpu())
                running_first_frame_loss += float(first_frame_loss.detach().cpu())
                running_soft_rate_loss += float(soft_rate_loss.detach().cpu())
                running_hard_rate_loss += float(hard_rate_loss.detach().cpu())

                if (i + 1) % log_interval == 0:
                    current_loss = (running_total_loss - previous_total_loss) / log_interval
                    tepoch.set_postfix(
                        current_loss=current_loss,
                        total_loss=running_total_loss / (i + 1),
                        dist_hq=running_hard_dist_loss / (i + 1),
                        dist_sq=running_soft_dist_loss / (i + 1),
                        rate_loss=running_rate_loss / (i + 1),
                        rate=running_rate_metric / (i + 1),
                        states_rate=running_states_rate_metric / (i + 1),
                        ffloss=running_first_frame_loss / (i + 1),
                        rateloss_hard=running_hard_rate_loss / (i + 1),
                        rateloss_soft=running_soft_rate_loss / (i + 1)
                    )
                    previous_total_loss = running_total_loss

        # save checkpoint
        checkpoint_path = os.path.join(checkpoint_dir, f'checkpoint_epoch_{epoch}.pth')
        checkpoint['state_dict'] = model.state_dict()
        checkpoint['loss'] = running_total_loss / len(dataloader)
        checkpoint['epoch'] = epoch
        torch.save(checkpoint, checkpoint_path)

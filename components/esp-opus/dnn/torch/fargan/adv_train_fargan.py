import os
import argparse
import random
import numpy as np
import sys
import math as m

import torch
from torch import nn
import torch.nn.functional as F
import tqdm

import fargan
from dataset import FARGANDataset
from stft_loss import *

source_dir = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(os.path.join(source_dir, "../osce/"))

import models as osce_models


def fmap_loss(scores_real, scores_gen):
    num_discs = len(scores_real)
    loss_feat = 0
    for k in range(num_discs):
        num_layers = len(scores_gen[k]) - 1
        f = 4 / num_discs / num_layers
        for l in range(num_layers):
            loss_feat += f * F.l1_loss(scores_gen[k][l], scores_real[k][l].detach())

    return loss_feat

parser = argparse.ArgumentParser()

parser.add_argument('features', type=str, help='path to feature file in .f32 format')
parser.add_argument('signal', type=str, help='path to signal file in .s16 format')
parser.add_argument('output', type=str, help='path to output folder')

parser.add_argument('--suffix', type=str, help="model name suffix", default="")
parser.add_argument('--cuda-visible-devices', type=str, help="comma separates list of cuda visible device indices, default: CUDA_VISIBLE_DEVICES", default=None)


model_group = parser.add_argument_group(title="model parameters")
model_group.add_argument('--cond-size', type=int, help="first conditioning size, default: 256", default=256)
model_group.add_argument('--gamma', type=float, help="Use A(z/gamma), default: 0.9", default=0.9)

training_group = parser.add_argument_group(title="training parameters")
training_group.add_argument('--batch-size', type=int, help="batch size, default: 128", default=128)
training_group.add_argument('--lr', type=float, help='learning rate, default: 5e-4', default=5e-4)
training_group.add_argument('--epochs', type=int, help='number of training epochs, default: 50', default=50)
training_group.add_argument('--sequence-length', type=int, help='sequence length, default: 60', default=60)
training_group.add_argument('--lr-decay', type=float, help='learning rate decay factor, default: 0.0', default=0.0)
training_group.add_argument('--initial-checkpoint', type=str, help='initial checkpoint to start training from, default: None', default=None)
training_group.add_argument('--reg-weight', type=float, help='regression loss weight, default: 1.0', default=1.0)
training_group.add_argument('--fmap-weight', type=float, help='feature matchin loss weight, default: 1.0', default=1.)

args = parser.parse_args()

if args.cuda_visible_devices != None:
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
lr_decay = args.lr_decay

adam_betas = [0.8, 0.99]
adam_eps = 1e-8
features_file = args.features
signal_file = args.signal

# model parameters
cond_size  = args.cond_size


checkpoint['batch_size'] = batch_size
checkpoint['lr'] = lr
checkpoint['lr_decay'] = lr_decay
checkpoint['epochs'] = epochs
checkpoint['sequence_length'] = sequence_length
checkpoint['adam_betas'] = adam_betas


device = torch.device("cuda") if torch.cuda.is_available() else torch.device("cpu")

checkpoint['model_args']    = ()
checkpoint['model_kwargs']  = {'cond_size': cond_size, 'gamma': args.gamma}
print(checkpoint['model_kwargs'])
model = fargan.FARGAN(*checkpoint['model_args'], **checkpoint['model_kwargs'])


#discriminator
disc_name = 'fdmresdisc'
disc = osce_models.model_dict[disc_name](
    architecture='free',
    design='f_down',
    fft_sizes_16k=[2**n for n in range(6, 12)],
    freq_roi=[0, 7400],
    max_channels=256,
    noise_gain=0.0
)

if type(args.initial_checkpoint) != type(None):
    checkpoint = torch.load(args.initial_checkpoint, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=False)

checkpoint['state_dict']    = model.state_dict()


dataset = FARGANDataset(features_file, signal_file, sequence_length=sequence_length)
dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True, num_workers=4)


optimizer = torch.optim.AdamW(model.parameters(), lr=lr, betas=adam_betas, eps=adam_eps)
optimizer_disc = torch.optim.AdamW([p for p in disc.parameters() if p.requires_grad], lr=lr, betas=adam_betas, eps=adam_eps)


# learning rate scheduler
scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay * x))
scheduler_disc = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer_disc, lr_lambda=lambda x : 1 / (1 + lr_decay * x))

states = None

spect_loss =  MultiResolutionSTFTLoss(device).to(device)

for param in model.parameters():
    param.requires_grad = False

batch_count = 0
if __name__ == '__main__':
    model.to(device)
    disc.to(device)

    for epoch in range(1, epochs + 1):

        m_r = 0
        m_f = 0
        s_r = 1
        s_f = 1

        running_cont_loss = 0
        running_disc_loss = 0
        running_gen_loss = 0
        running_fmap_loss = 0
        running_reg_loss = 0
        running_wc = 0

        print(f"training epoch {epoch}...")
        with tqdm.tqdm(dataloader, unit='batch') as tepoch:
            for i, (features, periods, target, lpc) in enumerate(tepoch):
                if epoch == 1 and i == 400:
                    for param in model.parameters():
                        param.requires_grad = True
                    for param in model.cond_net.parameters():
                        param.requires_grad = False
                    for param in model.sig_net.cond_gain_dense.parameters():
                        param.requires_grad = False

                optimizer.zero_grad()
                features = features.to(device)
                #lpc = lpc.to(device)
                #lpc = lpc*(args.gamma**torch.arange(1,17, device=device))
                #lpc = fargan.interp_lpc(lpc, 4)
                periods = periods.to(device)
                if True:
                    target = target[:, :sequence_length*160]
                    #lpc = lpc[:,:sequence_length*4,:]
                    features = features[:,:sequence_length+4,:]
                    periods = periods[:,:sequence_length+4]
                else:
                    target=target[::2, :]
                    #lpc=lpc[::2,:]
                    features=features[::2,:]
                    periods=periods[::2,:]
                target = target.to(device)
                #target = fargan.analysis_filter(target, lpc[:,:,:], nb_subframes=1, gamma=args.gamma)

                #nb_pre = random.randrange(1, 6)
                nb_pre = 2
                pre = target[:, :nb_pre*160]
                output, _ = model(features, periods, target.size(1)//160 - nb_pre, pre=pre, states=None)
                output = torch.cat([pre, output], -1)


                # discriminator update
                scores_gen = disc(output.detach().unsqueeze(1))
                scores_real = disc(target.unsqueeze(1))

                disc_loss = 0
                for scale in scores_gen:
                    disc_loss += ((scale[-1]) ** 2).mean()
                    m_f = 0.9 * m_f + 0.1 * scale[-1].detach().mean().cpu().item()
                    s_f = 0.9 * s_f + 0.1 * scale[-1].detach().std().cpu().item()

                for scale in scores_real:
                    disc_loss += ((1 - scale[-1]) ** 2).mean()
                    m_r = 0.9 * m_r + 0.1 * scale[-1].detach().mean().cpu().item()
                    s_r = 0.9 * s_r + 0.1 * scale[-1].detach().std().cpu().item()

                disc_loss = 0.5 * disc_loss / len(scores_gen)
                winning_chance = 0.5 * m.erfc( (m_r - m_f) / m.sqrt(2 * (s_f**2 + s_r**2)) )
                running_wc += winning_chance

                disc.zero_grad()
                disc_loss.backward()
                optimizer_disc.step()

                # model update
                scores_gen = disc(output.unsqueeze(1))
                if False: # todo: check whether that makes a difference
                    with torch.no_grad():
                        scores_real = disc(target.unsqueeze(1))

                cont_loss = fargan.sig_loss(target[:, nb_pre*160:nb_pre*160+80], output[:, nb_pre*160:nb_pre*160+80])
                specc_loss = spect_loss(output, target.detach())
                reg_loss = (.00*cont_loss + specc_loss)

                loss_gen = 0
                for scale in scores_gen:
                    loss_gen += ((1 - scale[-1]) ** 2).mean() / len(scores_gen)

                feat_loss = args.fmap_weight * fmap_loss(scores_real, scores_gen)

                reg_weight = args.reg_weight# + 15./(1 + (batch_count/7600.))
                gen_loss = reg_weight * reg_loss +  feat_loss + loss_gen

                model.zero_grad()


                gen_loss.backward()
                optimizer.step()

                #model.clip_weights()

                scheduler.step()
                scheduler_disc.step()

                running_cont_loss += cont_loss.detach().cpu().item()
                running_gen_loss += loss_gen.detach().cpu().item()
                running_disc_loss += disc_loss.detach().cpu().item()
                running_fmap_loss += feat_loss.detach().cpu().item()
                running_reg_loss += reg_loss.detach().cpu().item()



                tepoch.set_postfix(cont_loss=f"{running_cont_loss/(i+1):8.5f}",
                                   reg_weight=f"{reg_weight:8.5f}",
                                   gen_loss=f"{running_gen_loss/(i+1):8.5f}",
                                   disc_loss=f"{running_disc_loss/(i+1):8.5f}",
                                   fmap_loss=f"{running_fmap_loss/(i+1):8.5f}",
                                   reg_loss=f"{running_reg_loss/(i+1):8.5f}",
                                   wc = f"{running_wc/(i+1):8.5f}",
                                   )
                batch_count = batch_count + 1

        # save checkpoint
        checkpoint_path = os.path.join(checkpoint_dir, f'fargan{args.suffix}_adv_{epoch}.pth')
        checkpoint['state_dict'] = model.state_dict()
        checkpoint['disc_sate_dict'] = disc.state_dict()
        checkpoint['loss'] = {
            'cont': running_cont_loss / len(dataloader),
            'gen': running_gen_loss / len(dataloader),
            'disc': running_disc_loss / len(dataloader),
            'fmap': running_fmap_loss / len(dataloader),
            'reg': running_reg_loss / len(dataloader)
        }
        checkpoint['epoch'] = epoch
        torch.save(checkpoint, checkpoint_path)

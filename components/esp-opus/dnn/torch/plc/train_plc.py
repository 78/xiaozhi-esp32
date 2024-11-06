import os
import argparse
import random
import numpy as np

import torch
from torch import nn
import torch.nn.functional as F
import tqdm

import plc
from plc_dataset import PLCDataset

parser = argparse.ArgumentParser()

parser.add_argument('features', type=str, help='path to feature file in .f32 format')
parser.add_argument('loss', type=str, help='path to signal file in .s8 format')
parser.add_argument('output', type=str, help='path to output folder')

parser.add_argument('--suffix', type=str, help="model name suffix", default="")
parser.add_argument('--cuda-visible-devices', type=str, help="comma separates list of cuda visible device indices, default: CUDA_VISIBLE_DEVICES", default=None)


model_group = parser.add_argument_group(title="model parameters")
model_group.add_argument('--cond-size', type=int, help="first conditioning size, default: 128", default=128)
model_group.add_argument('--gru-size', type=int, help="GRU size, default: 128", default=128)

training_group = parser.add_argument_group(title="training parameters")
training_group.add_argument('--batch-size', type=int, help="batch size, default: 512", default=512)
training_group.add_argument('--lr', type=float, help='learning rate, default: 1e-3', default=1e-3)
training_group.add_argument('--epochs', type=int, help='number of training epochs, default: 20', default=20)
training_group.add_argument('--sequence-length', type=int, help='sequence length, default: 15', default=15)
training_group.add_argument('--lr-decay', type=float, help='learning rate decay factor, default: 1e-4', default=1e-4)
training_group.add_argument('--initial-checkpoint', type=str, help='initial checkpoint to start training from, default: None', default=None)

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

adam_betas = [0.8, 0.95]
adam_eps = 1e-8
features_file = args.features
loss_file = args.loss

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
checkpoint['model_kwargs']  = {'cond_size': cond_size, 'gru_size': args.gru_size}
print(checkpoint['model_kwargs'])
model = plc.PLC(*checkpoint['model_args'], **checkpoint['model_kwargs'])

if type(args.initial_checkpoint) != type(None):
    checkpoint = torch.load(args.initial_checkpoint, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=False)

checkpoint['state_dict']    = model.state_dict()


dataset = PLCDataset(features_file, loss_file, sequence_length=sequence_length)
dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True, num_workers=4)


optimizer = torch.optim.AdamW(model.parameters(), lr=lr, betas=adam_betas, eps=adam_eps)


# learning rate scheduler
scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay * x))

states = None

plc_loss = plc.plc_loss(18, device=device)
if __name__ == '__main__':
    model.to(device)

    for epoch in range(1, epochs + 1):

        running_loss = 0
        running_l1_loss = 0
        running_ceps_loss = 0
        running_band_loss = 0
        running_pitch_loss = 0

        print(f"training epoch {epoch}...")
        with tqdm.tqdm(dataloader, unit='batch') as tepoch:
            for i, (features, lost, target) in enumerate(tepoch):
                optimizer.zero_grad()
                features = features.to(device)
                lost = lost.to(device)
                target = target.to(device)

                out, states = model(features, lost)

                loss, l1_loss, ceps_loss, band_loss, pitch_loss = plc_loss(target, out)

                loss.backward()
                optimizer.step()

                #model.clip_weights()

                scheduler.step()

                running_loss += loss.detach().cpu().item()
                running_l1_loss += l1_loss.detach().cpu().item()
                running_ceps_loss += ceps_loss.detach().cpu().item()
                running_band_loss += band_loss.detach().cpu().item()
                running_pitch_loss += pitch_loss.detach().cpu().item()
                tepoch.set_postfix(loss=f"{running_loss/(i+1):8.5f}",
                                   l1_loss=f"{running_l1_loss/(i+1):8.5f}",
                                   ceps_loss=f"{running_ceps_loss/(i+1):8.5f}",
                                   band_loss=f"{running_band_loss/(i+1):8.5f}",
                                   pitch_loss=f"{running_pitch_loss/(i+1):8.5f}",
                                   )

        # save checkpoint
        checkpoint_path = os.path.join(checkpoint_dir, f'plc{args.suffix}_{epoch}.pth')
        checkpoint['state_dict'] = model.state_dict()
        checkpoint['loss'] = running_loss / len(dataloader)
        checkpoint['epoch'] = epoch
        torch.save(checkpoint, checkpoint_path)

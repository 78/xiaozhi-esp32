import os
import argparse
import random
import numpy as np

import torch
from torch import nn
import torch.nn.functional as F
import tqdm

import fargan
from dataset import FARGANDataset
from stft_loss import *

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

#model = fargan.FARGAN()
#model = nn.DataParallel(model)

if type(args.initial_checkpoint) != type(None):
    checkpoint = torch.load(args.initial_checkpoint, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'], strict=False)

checkpoint['state_dict']    = model.state_dict()


dataset = FARGANDataset(features_file, signal_file, sequence_length=sequence_length)
dataloader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True, num_workers=4)


optimizer = torch.optim.AdamW(model.parameters(), lr=lr, betas=adam_betas, eps=adam_eps)


# learning rate scheduler
scheduler = torch.optim.lr_scheduler.LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay * x))

states = None

spect_loss =  MultiResolutionSTFTLoss(device).to(device)

if __name__ == '__main__':
    model.to(device)

    for epoch in range(1, epochs + 1):

        running_specc = 0
        running_cont_loss = 0
        running_loss = 0

        print(f"training epoch {epoch}...")
        with tqdm.tqdm(dataloader, unit='batch') as tepoch:
            for i, (features, periods, target, lpc) in enumerate(tepoch):
                optimizer.zero_grad()
                features = features.to(device)
                #lpc = torch.tensor(fargan.interp_lpc(lpc.numpy(), 4))
                #print("interp size", lpc.shape)
                #lpc = lpc.to(device)
                #lpc = lpc*(args.gamma**torch.arange(1,17, device=device))
                #lpc = fargan.interp_lpc(lpc, 4)
                periods = periods.to(device)
                if (np.random.rand() > 0.1):
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
                #print(target.shape, lpc.shape)
                #target = fargan.analysis_filter(target, lpc[:,:,:], nb_subframes=1, gamma=args.gamma)

                #nb_pre = random.randrange(1, 6)
                nb_pre = 2
                pre = target[:, :nb_pre*160]
                sig, states = model(features, periods, target.size(1)//160 - nb_pre, pre=pre, states=None)
                sig = torch.cat([pre, sig], -1)

                cont_loss = fargan.sig_loss(target[:, nb_pre*160:nb_pre*160+160], sig[:, nb_pre*160:nb_pre*160+160])
                specc_loss = spect_loss(sig, target.detach())
                loss = .03*cont_loss + specc_loss

                loss.backward()
                optimizer.step()

                #model.clip_weights()

                scheduler.step()

                running_specc += specc_loss.detach().cpu().item()
                running_cont_loss += cont_loss.detach().cpu().item()

                running_loss += loss.detach().cpu().item()
                tepoch.set_postfix(loss=f"{running_loss/(i+1):8.5f}",
                                   cont_loss=f"{running_cont_loss/(i+1):8.5f}",
                                   specc=f"{running_specc/(i+1):8.5f}",
                                   )

        # save checkpoint
        checkpoint_path = os.path.join(checkpoint_dir, f'fargan{args.suffix}_{epoch}.pth')
        checkpoint['state_dict'] = model.state_dict()
        checkpoint['loss'] = running_loss / len(dataloader)
        checkpoint['epoch'] = epoch
        torch.save(checkpoint, checkpoint_path)

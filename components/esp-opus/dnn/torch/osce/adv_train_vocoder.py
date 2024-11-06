"""
/* Copyright (c) 2023 Amazon
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
import sys
import math as m
import random

import yaml

from tqdm import tqdm

try:
    import git
    has_git = True
except:
    has_git = False

import torch
from torch.optim.lr_scheduler import LambdaLR
import torch.nn.functional as F

from scipy.io import wavfile
import numpy as np
import pesq

from data import LPCNetVocodingDataset
from models import model_dict


from utils.lpcnet_features import load_lpcnet_features
from utils.misc import count_parameters

from losses.stft_loss import MRSTFTLoss, MRLogMelLoss


parser = argparse.ArgumentParser()

parser.add_argument('setup', type=str, help='setup yaml file')
parser.add_argument('output', type=str, help='output path')
parser.add_argument('--device', type=str, help='compute device', default=None)
parser.add_argument('--initial-checkpoint', type=str, help='initial checkpoint', default=None)
parser.add_argument('--test-features', type=str, help='path to features for testing', default=None)
parser.add_argument('--no-redirect', action='store_true', help='disables re-direction of stdout')

args = parser.parse_args()


torch.set_num_threads(4)

with open(args.setup, 'r') as f:
    setup = yaml.load(f.read(), yaml.FullLoader)

checkpoint_prefix = 'checkpoint'
output_prefix = 'output'
setup_name = 'setup.yml'
output_file='out.txt'


# check model
if not 'name' in setup['model']:
    print(f'warning: did not find model entry in setup, using default PitchPostFilter')
    model_name = 'pitchpostfilter'
else:
    model_name = setup['model']['name']

# prepare output folder
if os.path.exists(args.output):
    print("warning: output folder exists")

    reply = input('continue? (y/n): ')
    while reply not in {'y', 'n'}:
        reply = input('continue? (y/n): ')

    if reply == 'n':
        os._exit()
else:
    os.makedirs(args.output, exist_ok=True)

checkpoint_dir = os.path.join(args.output, 'checkpoints')
os.makedirs(checkpoint_dir, exist_ok=True)

# add repo info to setup
if has_git:
    working_dir = os.path.split(__file__)[0]
    try:
        repo = git.Repo(working_dir, search_parent_directories=True)
        setup['repo'] = dict()
        hash = repo.head.object.hexsha
        urls = list(repo.remote().urls)
        is_dirty = repo.is_dirty()

        if is_dirty:
            print("warning: repo is dirty")

        setup['repo']['hash'] = hash
        setup['repo']['urls'] = urls
        setup['repo']['dirty'] = is_dirty
    except:
        has_git = False

# dump setup
with open(os.path.join(args.output, setup_name), 'w') as f:
    yaml.dump(setup, f)


ref = None
# prepare inference test if wanted
inference_test = False
if type(args.test_features) != type(None):
    test_features = load_lpcnet_features(args.test_features)
    features = test_features['features']
    periods = test_features['periods']
    inference_folder = os.path.join(args.output, 'inference_test')
    os.makedirs(inference_folder, exist_ok=True)
    inference_test = True


# training parameters
batch_size      = setup['training']['batch_size']
epochs          = setup['training']['epochs']
lr              = setup['training']['lr']
lr_decay_factor = setup['training']['lr_decay_factor']
lr_gen          = lr * setup['training']['gen_lr_reduction']
lambda_feat     =  setup['training']['lambda_feat']
lambda_reg      = setup['training']['lambda_reg']
adv_target      = setup['training'].get('adv_target', 'target')


# load training dataset
data_config = setup['data']
data = LPCNetVocodingDataset(setup['dataset'], **data_config)

# load validation dataset if given
if 'validation_dataset' in setup:
    validation_data = LPCNetVocodingDataset(setup['validation_dataset'], **data_config)

    validation_dataloader = torch.utils.data.DataLoader(validation_data, batch_size=batch_size, drop_last=True, num_workers=4)

    run_validation = True
else:
    run_validation = False

# create model
model = model_dict[model_name](*setup['model']['args'], **setup['model']['kwargs'])


# create discriminator
disc_name = setup['discriminator']['name']
disc = model_dict[disc_name](
    *setup['discriminator']['args'], **setup['discriminator']['kwargs']
)



# set compute device
if type(args.device) == type(None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
else:
    device = torch.device(args.device)



# dataloader
dataloader = torch.utils.data.DataLoader(data, batch_size=batch_size, drop_last=True, shuffle=True, num_workers=4)

# optimizer is introduced to trainable parameters
parameters = [p for p in model.parameters() if p.requires_grad]
optimizer = torch.optim.Adam(parameters, lr=lr_gen)

# disc optimizer
parameters = [p for p in disc.parameters() if p.requires_grad]
optimizer_disc = torch.optim.Adam(parameters, lr=lr, betas=[0.5, 0.9])

# learning rate scheduler
scheduler = LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay_factor * x))

if args.initial_checkpoint is not None:
    print(f"loading state dict from {args.initial_checkpoint}...")
    chkpt = torch.load(args.initial_checkpoint, map_location=device)
    model.load_state_dict(chkpt['state_dict'])

    if 'disc_state_dict' in chkpt:
        print(f"loading discriminator state dict from {args.initial_checkpoint}...")
        disc.load_state_dict(chkpt['disc_state_dict'])

    if 'optimizer_state_dict' in chkpt:
        print(f"loading optimizer state dict from {args.initial_checkpoint}...")
        optimizer.load_state_dict(chkpt['optimizer_state_dict'])

    if 'disc_optimizer_state_dict' in chkpt:
        print(f"loading discriminator optimizer state dict from {args.initial_checkpoint}...")
        optimizer_disc.load_state_dict(chkpt['disc_optimizer_state_dict'])

    if 'scheduler_state_disc' in chkpt:
        print(f"loading scheduler state dict from {args.initial_checkpoint}...")
        scheduler.load_state_dict(chkpt['scheduler_state_dict'])

    # if 'torch_rng_state' in chkpt:
    #     print(f"setting torch RNG state from {args.initial_checkpoint}...")
    #     torch.set_rng_state(chkpt['torch_rng_state'])

    if 'numpy_rng_state' in chkpt:
        print(f"setting numpy RNG state from {args.initial_checkpoint}...")
        np.random.set_state(chkpt['numpy_rng_state'])

    if 'python_rng_state' in chkpt:
        print(f"setting Python RNG state from {args.initial_checkpoint}...")
        random.setstate(chkpt['python_rng_state'])

# loss
w_l1 = setup['training']['loss']['w_l1']
w_lm = setup['training']['loss']['w_lm']
w_slm = setup['training']['loss']['w_slm']
w_sc = setup['training']['loss']['w_sc']
w_logmel = setup['training']['loss']['w_logmel']
w_wsc = setup['training']['loss']['w_wsc']
w_xcorr = setup['training']['loss']['w_xcorr']
w_sxcorr = setup['training']['loss']['w_sxcorr']
w_l2 = setup['training']['loss']['w_l2']

w_sum = w_l1 + w_lm + w_sc + w_logmel + w_wsc + w_slm + w_xcorr + w_sxcorr + w_l2

stftloss = MRSTFTLoss(sc_weight=w_sc, log_mag_weight=w_lm, wsc_weight=w_wsc, smooth_log_mag_weight=w_slm, sxcorr_weight=w_sxcorr).to(device)
logmelloss = MRLogMelLoss().to(device)

def xcorr_loss(y_true, y_pred):
    dims = list(range(1, len(y_true.shape)))

    loss = 1 - torch.sum(y_true * y_pred, dim=dims) / torch.sqrt(torch.sum(y_true ** 2, dim=dims) * torch.sum(y_pred ** 2, dim=dims) + 1e-9)

    return torch.mean(loss)

def td_l2_norm(y_true, y_pred):
    dims = list(range(1, len(y_true.shape)))

    loss = torch.mean((y_true - y_pred) ** 2, dim=dims) / (torch.mean(y_pred ** 2, dim=dims) ** .5 + 1e-6)

    return loss.mean()

def td_l1(y_true, y_pred, pow=0):
    dims = list(range(1, len(y_true.shape)))
    tmp = torch.mean(torch.abs(y_true - y_pred), dim=dims) / ((torch.mean(torch.abs(y_pred), dim=dims) + 1e-9) ** pow)

    return torch.mean(tmp)

def criterion(x, y):

    return (w_l1 * td_l1(x, y, pow=1) +  stftloss(x, y) + w_logmel * logmelloss(x, y)
            + w_xcorr * xcorr_loss(x, y) + w_l2 * td_l2_norm(x, y)) / w_sum


# model checkpoint
checkpoint = {
    'setup'         : setup,
    'state_dict'    : model.state_dict(),
    'loss'          : -1
}


if not args.no_redirect:
    print(f"re-directing output to {os.path.join(args.output, output_file)}")
    sys.stdout = open(os.path.join(args.output, output_file), "w")


print("summary:")

print(f"generator: {count_parameters(model.cpu()) / 1e6:5.3f} M parameters")
if hasattr(model, 'flop_count'):
    print(f"generator: {model.flop_count(16000) / 1e6:5.3f} MFLOPS")
print(f"discriminator: {count_parameters(disc.cpu()) / 1e6:5.3f} M parameters")

if ref is not None:
    noisy = np.fromfile(os.path.join(args.testdata, 'noisy.s16'), dtype=np.int16)
    initial_mos = pesq.pesq(16000, ref, noisy, mode='wb')
    print(f"initial MOS (PESQ): {initial_mos}")

best_loss = 1e9
log_interval = 10


m_r = 0
m_f = 0
s_r = 1
s_f = 1

def optimizer_to(optim, device):
    for param in optim.state.values():
        if isinstance(param, torch.Tensor):
            param.data = param.data.to(device)
            if param._grad is not None:
                param._grad.data = param._grad.data.to(device)
        elif isinstance(param, dict):
            for subparam in param.values():
                if isinstance(subparam, torch.Tensor):
                    subparam.data = subparam.data.to(device)
                    if subparam._grad is not None:
                        subparam._grad.data = subparam._grad.data.to(device)

optimizer_to(optimizer, device)
optimizer_to(optimizer_disc, device)


for ep in range(1, epochs + 1):
    print(f"training epoch {ep}...")

    model.to(device)
    disc.to(device)
    model.train()
    disc.train()

    running_disc_loss = 0
    running_adv_loss = 0
    running_feature_loss = 0
    running_reg_loss = 0

    with tqdm(dataloader, unit='batch', file=sys.stdout) as tepoch:
        for i, batch in enumerate(tepoch):

            # set gradients to zero
            optimizer.zero_grad()

            # push batch to device
            for key in batch:
                batch[key] = batch[key].to(device)

            target = batch['target'].to(device)
            disc_target = batch[adv_target].to(device)

            # calculate model output
            output = model(batch['features'], batch['periods'])

            # discriminator update
            scores_gen = disc(output.detach())
            scores_real = disc(disc_target.unsqueeze(1))

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

            disc.zero_grad()
            disc_loss.backward()
            optimizer_disc.step()

            # generator update
            scores_gen = disc(output)


            # calculate loss
            loss_reg = criterion(output.squeeze(1), target)

            num_discs = len(scores_gen)
            loss_gen = 0
            for scale in scores_gen:
                loss_gen += ((1 - scale[-1]) ** 2).mean() / num_discs

            loss_feat = 0
            for k in range(num_discs):
                num_layers = len(scores_gen[k]) - 1
                f = 4 / num_discs / num_layers
                for l in range(num_layers):
                    loss_feat += f * F.l1_loss(scores_gen[k][l], scores_real[k][l].detach())

            model.zero_grad()

            (loss_gen + lambda_feat * loss_feat + lambda_reg * loss_reg).backward()

            optimizer.step()

            running_adv_loss += loss_gen.detach().cpu().item()
            running_disc_loss += disc_loss.detach().cpu().item()
            running_feature_loss += lambda_feat * loss_feat.detach().cpu().item()
            running_reg_loss += lambda_reg * loss_reg.detach().cpu().item()

            # update status bar
            if i % log_interval == 0:
                tepoch.set_postfix(adv_loss=f"{running_adv_loss/(i + 1):8.7f}",
                                   disc_loss=f"{running_disc_loss/(i + 1):8.7f}",
                                   feat_loss=f"{running_feature_loss/(i + 1):8.7f}",
                                   reg_loss=f"{running_reg_loss/(i + 1):8.7f}",
                                   wc=f"{100*winning_chance:5.2f}%")


    # save checkpoint
    checkpoint['state_dict'] = model.state_dict()
    checkpoint['disc_state_dict'] = disc.state_dict()
    checkpoint['optimizer_state_dict'] = optimizer.state_dict()
    checkpoint['disc_optimizer_state_dict'] = optimizer_disc.state_dict()
    checkpoint['scheduler_state_dict'] = scheduler.state_dict()
    checkpoint['torch_rng_state'] = torch.get_rng_state()
    checkpoint['numpy_rng_state'] = np.random.get_state()
    checkpoint['python_rng_state'] = random.getstate()
    checkpoint['adv_loss']   = running_adv_loss/(i + 1)
    checkpoint['disc_loss']  = running_disc_loss/(i + 1)
    checkpoint['feature_loss'] = running_feature_loss/(i + 1)
    checkpoint['reg_loss'] = running_reg_loss/(i + 1)


    if inference_test:
        print("running inference test...")
        out = model.process(features, periods).cpu().numpy()
        wavfile.write(os.path.join(inference_folder, f'{model_name}_epoch_{ep}.wav'), 16000, out)
        if ref is not None:
            mos = pesq.pesq(16000, ref, out, mode='wb')
            print(f"MOS (PESQ): {mos}")


    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_epoch_{ep}.pth'))
    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_last.pth'))


    print()

print('Done')

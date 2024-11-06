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

seed=1888

import os
import argparse
import sys
import random
random.seed(seed)

import yaml

try:
    import git
    has_git = True
except:
    has_git = False

import torch
torch.manual_seed(seed)
torch.backends.cudnn.benchmark = False
from torch.optim.lr_scheduler import LambdaLR

import numpy as np
np.random.seed(seed)

from scipy.io import wavfile

import pesq

from data import SilkEnhancementSet
from models import model_dict
from engine.engine import train_one_epoch, evaluate


from utils.silk_features import load_inference_data
from utils.misc import count_parameters, count_nonzero_parameters

from losses.stft_loss import MRSTFTLoss, MRLogMelLoss


parser = argparse.ArgumentParser()

parser.add_argument('setup', type=str, help='setup yaml file')
parser.add_argument('output', type=str, help='output path')
parser.add_argument('--device', type=str, help='compute device', default=None)
parser.add_argument('--initial-checkpoint', type=str, help='initial checkpoint', default=None)
parser.add_argument('--testdata', type=str, help='path to features and signal for testing', default=None)
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
        os._exit(0)
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
            with open(os.path.join(args.output, 'repo.diff'), "w") as f:
                f.write(repo.git.execute(["git", "diff"]))

        setup['repo']['hash'] = hash
        setup['repo']['urls'] = urls
        setup['repo']['dirty'] = is_dirty
    except:
        has_git = False

# dump setup
with open(os.path.join(args.output, setup_name), 'w') as f:
    yaml.dump(setup, f)

ref = None
if args.testdata is not None:

    testsignal, features, periods, numbits = load_inference_data(args.testdata, **setup['data'])

    inference_test = True
    inference_folder = os.path.join(args.output, 'inference_test')
    os.makedirs(os.path.join(args.output, 'inference_test'), exist_ok=True)

    try:
        ref = np.fromfile(os.path.join(args.testdata, 'clean.s16'), dtype=np.int16)
    except:
        pass
else:
    inference_test = False

# training parameters
batch_size      = setup['training']['batch_size']
epochs          = setup['training']['epochs']
lr              = setup['training']['lr']
lr_decay_factor = setup['training']['lr_decay_factor']

# load training dataset
data_config = setup['data']
data = SilkEnhancementSet(setup['dataset'], **data_config)

# load validation dataset if given
if 'validation_dataset' in setup:
    validation_data = SilkEnhancementSet(setup['validation_dataset'], **data_config)

    validation_dataloader = torch.utils.data.DataLoader(validation_data, batch_size=batch_size, drop_last=True, num_workers=8)

    run_validation = True
else:
    run_validation = False

# create model
model = model_dict[model_name](*setup['model']['args'], **setup['model']['kwargs'])

if args.initial_checkpoint is not None:
    print(f"loading state dict from {args.initial_checkpoint}...")
    chkpt = torch.load(args.initial_checkpoint, map_location='cpu')
    model.load_state_dict(chkpt['state_dict'])

# set compute device
if type(args.device) == type(None):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
else:
    device = torch.device(args.device)

# push model to device
model.to(device)

# dataloader
dataloader = torch.utils.data.DataLoader(data, batch_size=batch_size, drop_last=True, shuffle=True, num_workers=8)

# optimizer is introduced to trainable parameters
parameters = [p for p in model.parameters() if p.requires_grad]
optimizer = torch.optim.Adam(parameters, lr=lr)

# learning rate scheduler
scheduler = LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay_factor * x))

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

print(f"{count_parameters(model.cpu()) / 1e6:5.3f} M parameters")
if hasattr(model, 'flop_count'):
    print(f"{model.flop_count(16000) / 1e6:5.3f} MFLOPS")

if ref is not None:
    noisy = np.fromfile(os.path.join(args.testdata, 'noisy.s16'), dtype=np.int16)
    initial_mos = pesq.pesq(16000, ref, noisy, mode='wb')
    print(f"initial MOS (PESQ): {initial_mos}")

best_loss = 1e9

for ep in range(1, epochs + 1):
    print(f"training epoch {ep}...")
    new_loss = train_one_epoch(model, criterion, optimizer, dataloader, device, scheduler)


    # save checkpoint
    checkpoint['state_dict'] = model.state_dict()
    checkpoint['loss']       = new_loss

    if run_validation:
        print("running validation...")
        validation_loss = evaluate(model, criterion, validation_dataloader, device)
        checkpoint['validation_loss'] = validation_loss

        if validation_loss < best_loss:
            torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_best.pth'))
            best_loss = validation_loss

    if inference_test:
        print("running inference test...")
        out = model.process(testsignal, features, periods, numbits).cpu().numpy()
        wavfile.write(os.path.join(inference_folder, f'{model_name}_epoch_{ep}.wav'), 16000, out)
        if ref is not None:
            mos = pesq.pesq(16000, ref, out, mode='wb')
            print(f"MOS (PESQ): {mos}")


    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_epoch_{ep}.pth'))
    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_last.pth'))


    print(f"non-zero parameters: {count_nonzero_parameters(model)}\n")

print('Done')

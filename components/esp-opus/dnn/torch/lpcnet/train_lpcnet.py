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

try:
    import git
    has_git = True
except:
    has_git = False

import yaml


import torch
from torch.optim.lr_scheduler import LambdaLR

from data import LPCNetDataset
from models import model_dict
from engine.lpcnet_engine import train_one_epoch, evaluate
from utils.data import load_features
from utils.wav import wavwrite16


debug = False
if debug:
    args = type('dummy', (object,),
    {
        'setup' : 'setup.yml',
        'output' : 'testout',
        'device' : None,
        'test_features' : None,
        'finalize': False,
        'initial_checkpoint': None,
        'no-redirect': False
    })()
else:
    parser = argparse.ArgumentParser("train_lpcnet.py")
    parser.add_argument('setup', type=str, help='setup yaml file')
    parser.add_argument('output', type=str, help='output path')
    parser.add_argument('--device', type=str, help='compute device', default=None)
    parser.add_argument('--test-features', type=str, help='test feature file in v2 format', default=None)
    parser.add_argument('--finalize', action='store_true', help='run single training round with lr=1e-5')
    parser.add_argument('--initial-checkpoint', type=str, help='initial checkpoint', default=None)
    parser.add_argument('--no-redirect', action='store_true', help='disables re-direction of output')

    args = parser.parse_args()


torch.set_num_threads(4)

with open(args.setup, 'r') as f:
    setup = yaml.load(f.read(), yaml.FullLoader)

if args.finalize:
    if args.initial_checkpoint is None:
        raise ValueError('finalization requires initial checkpoint')

    if 'sparsification' in setup['lpcnet']['config']:
        for sp_job in setup['lpcnet']['config']['sparsification'].values():
            sp_job['start'], sp_job['stop'] = 0, 0

    setup['training']['lr'] = 1.0e-5
    setup['training']['lr_decay_factor'] = 0.0
    setup['training']['epochs'] = 1

    checkpoint_prefix = 'checkpoint_finalize'
    output_prefix = 'output_finalize'
    setup_name = 'setup_finalize.yml'
    output_file='out_finalize.txt'
else:
    checkpoint_prefix = 'checkpoint'
    output_prefix = 'output'
    setup_name = 'setup.yml'
    output_file='out.txt'


# check model
if not 'model' in setup['lpcnet']:
    print(f'warning: did not find model entry in setup, using default lpcnet')
    model_name = 'lpcnet'
else:
    model_name = setup['lpcnet']['model']

# prepare output folder
if os.path.exists(args.output) and not debug and not args.finalize:
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
        repo = git.Repo(working_dir)
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

# prepare inference test if wanted
run_inference_test = False
if type(args.test_features) != type(None):
    test_features = load_features(args.test_features)
    inference_test_dir = os.path.join(args.output, 'inference_test')
    os.makedirs(inference_test_dir, exist_ok=True)
    run_inference_test = True

# training parameters
batch_size      = setup['training']['batch_size']
epochs          = setup['training']['epochs']
lr              = setup['training']['lr']
lr_decay_factor = setup['training']['lr_decay_factor']

# load training dataset
lpcnet_config = setup['lpcnet']['config']
data = LPCNetDataset(   setup['dataset'],
                        features=lpcnet_config['features'],
                        input_signals=lpcnet_config['signals'],
                        target=lpcnet_config['target'],
                        frames_per_sample=setup['training']['frames_per_sample'],
                        feature_history=lpcnet_config['feature_history'],
                        feature_lookahead=lpcnet_config['feature_lookahead'],
                        lpc_gamma=lpcnet_config.get('lpc_gamma', 1))

# load validation dataset if given
if 'validation_dataset' in setup:
    validation_data = LPCNetDataset(   setup['validation_dataset'],
                        features=lpcnet_config['features'],
                        input_signals=lpcnet_config['signals'],
                        target=lpcnet_config['target'],
                        frames_per_sample=setup['training']['frames_per_sample'],
                        feature_history=lpcnet_config['feature_history'],
                        feature_lookahead=lpcnet_config['feature_lookahead'],
                        lpc_gamma=lpcnet_config.get('lpc_gamma', 1))

    validation_dataloader = torch.utils.data.DataLoader(validation_data, batch_size=batch_size, drop_last=True, num_workers=4)

    run_validation = True
else:
    run_validation = False

# create model
model = model_dict[model_name](setup['lpcnet']['config'])

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
dataloader = torch.utils.data.DataLoader(data, batch_size=batch_size, drop_last=True, shuffle=True, num_workers=4)

# optimizer is introduced to trainable parameters
parameters = [p for p in model.parameters() if p.requires_grad]
optimizer = torch.optim.Adam(parameters, lr=lr)

# learning rate scheduler
scheduler = LambdaLR(optimizer=optimizer, lr_lambda=lambda x : 1 / (1 + lr_decay_factor * x))

# loss
criterion = torch.nn.NLLLoss()

# model checkpoint
checkpoint = {
    'setup'         : setup,
    'state_dict'    : model.state_dict(),
    'loss'          : -1
}

if not args.no_redirect:
    print(f"re-directing output to {os.path.join(args.output, output_file)}")
    sys.stdout = open(os.path.join(args.output, output_file), "w")

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

    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_epoch_{ep}.pth'))
    torch.save(checkpoint, os.path.join(checkpoint_dir, checkpoint_prefix + f'_last.pth'))

    # run inference test
    if run_inference_test:
        model.to("cpu")
        print("running inference test...")

        output = model.generate(test_features['features'], test_features['periods'], test_features['lpcs'])

        testfilename = os.path.join(inference_test_dir, output_prefix + f'_epoch_{ep}.wav')

        wavwrite16(testfilename, output.numpy(), 16000)

        model.to(device)

    print()

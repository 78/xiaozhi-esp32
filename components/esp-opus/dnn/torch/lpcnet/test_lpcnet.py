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

import argparse

import torch
import numpy as np


from models import model_dict
from utils.data import load_features
from utils.wav import wavwrite16

debug = False
if debug:
    args = type('dummy', (object,),
    {
        'features'      : 'features.f32',
        'checkpoint'    : 'checkpoint.pth',
        'output'        : 'out.wav',
        'version'       : 2
    })()
else:
    parser = argparse.ArgumentParser()

    parser.add_argument('features', type=str, help='feature file')
    parser.add_argument('checkpoint', type=str, help='checkpoint file')
    parser.add_argument('output', type=str, help='output file')
    parser.add_argument('--version', type=int, help='feature version', default=2)

    args = parser.parse_args()


torch.set_num_threads(2)

version = args.version
feature_file = args.features
checkpoint_file = args.checkpoint



output_file = args.output
if not output_file.endswith('.wav'):
    output_file += '.wav'

checkpoint = torch.load(checkpoint_file, map_location="cpu")

# check model
if not 'model' in checkpoint['setup']['lpcnet']:
    print(f'warning: did not find model entry in setup, using default lpcnet')
    model_name = 'lpcnet'
else:
    model_name = checkpoint['setup']['lpcnet']['model']

model = model_dict[model_name](checkpoint['setup']['lpcnet']['config'])

model.load_state_dict(checkpoint['state_dict'])

data = load_features(feature_file)

output = model.generate(data['features'], data['periods'], data['lpcs'])

wavwrite16(output_file, output.numpy(), 16000)

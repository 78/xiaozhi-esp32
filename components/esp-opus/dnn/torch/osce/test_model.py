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

from scipy.io import wavfile


from models import model_dict
from utils.silk_features import load_inference_data
from utils import endoscopy

debug = False
if debug:
    args = type('dummy', (object,),
    {
        'input'         : 'testitems/all_0_orig.se',
        'checkpoint'    : 'testout/checkpoints/checkpoint_epoch_5.pth',
        'output'        : 'out.wav',
    })()
else:
    parser = argparse.ArgumentParser()

    parser.add_argument('input', type=str, help='path to folder with features and signals')
    parser.add_argument('checkpoint', type=str, help='checkpoint file')
    parser.add_argument('output', type=str, help='output file')
    parser.add_argument('--debug', action='store_true', help='enables debug output')


    args = parser.parse_args()


torch.set_num_threads(2)

input_folder = args.input
checkpoint_file = args.checkpoint


output_file = args.output
if not output_file.endswith('.wav'):
    output_file += '.wav'

checkpoint = torch.load(checkpoint_file, map_location="cpu")

# check model
if not 'name' in checkpoint['setup']['model']:
    print(f'warning: did not find model name entry in setup, using pitchpostfilter per default')
    model_name = 'pitchpostfilter'
else:
    model_name = checkpoint['setup']['model']['name']

model = model_dict[model_name](*checkpoint['setup']['model']['args'], **checkpoint['setup']['model']['kwargs'])

model.load_state_dict(checkpoint['state_dict'])

# generate model input
setup = checkpoint['setup']
signal, features, periods, numbits = load_inference_data(input_folder, **setup['data'])

if args.debug:
    endoscopy.init()

output = model.process(signal, features, periods, numbits, debug=args.debug)

wavfile.write(output_file, 16000, output.cpu().numpy())

if args.debug:
    endoscopy.close()

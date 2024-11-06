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

import yaml

from models import model_dict


debug = False
if debug:
    args = type('dummy', (object,),
    {
        'setup' : 'setups/lpcnet_m/setup_1_4_concatenative.yml',
        'hierarchical_sampling' : False
    })()
else:
    parser = argparse.ArgumentParser()
    parser.add_argument('setup', type=str, help='setup yaml file')
    parser.add_argument('--hierarchical-sampling', action="store_true", help='whether to assume hierarchical sampling (default=False)', default=False)

    args = parser.parse_args()

with open(args.setup, 'r') as f:
    setup = yaml.load(f.read(), yaml.FullLoader)

# check model
if not 'model' in setup['lpcnet']:
    print(f'warning: did not find model entry in setup, using default lpcnet')
    model_name = 'lpcnet'
else:
    model_name = setup['lpcnet']['model']

# create model
model = model_dict[model_name](setup['lpcnet']['config'])

gflops = model.get_gflops(16000, verbose=True, hierarchical_sampling=args.hierarchical_sampling)

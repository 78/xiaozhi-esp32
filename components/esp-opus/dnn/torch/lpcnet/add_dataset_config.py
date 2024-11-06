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
import os

import yaml


from utils.templates import dataset_template_v1, dataset_template_v2




parser = argparse.ArgumentParser("add_dataset_config.py")

parser.add_argument('path', type=str, help='path to folder containing feature and data file')
parser.add_argument('--version', type=int, help="dataset version, 1 for classic LPCNet with 55 feature slots, 2 for new format with 36 feature slots.", default=2)
parser.add_argument('--description', type=str, help='brief dataset description', default="I will add a description later")
args = parser.parse_args()


if args.version == 1:
    template = dataset_template_v1
    data_extension = '.u8'
elif args.version == 2:
    template = dataset_template_v2
    data_extension = '.s16'
else:
    raise ValueError(f"unknown dataset version {args.version}")

# get folder content
content = os.listdir(args.path)

features = [c for c in content if c.endswith('.f32')]

if len(features) != 1:
    print("could not determine feature file")
else:
    template['feature_file'] = features[0]

data = [c for c in content if c.endswith(data_extension)]
if len(data) != 1:
    print("could not determine data file")
else:
    template['signal_file'] = data[0]

template['description'] = args.description

with open(os.path.join(args.path, 'info.yml'), 'w') as f:
    yaml.dump(template, f)

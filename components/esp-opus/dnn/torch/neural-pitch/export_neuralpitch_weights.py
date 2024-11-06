"""
/* Copyright (c) 2022 Amazon
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

sys.path.append(os.path.join(os.path.dirname(__file__), '../weight-exchange'))


parser = argparse.ArgumentParser()

parser.add_argument('checkpoint', type=str, help='model checkpoint')
parser.add_argument('output_dir', type=str, help='output folder')

args = parser.parse_args()

import torch
import numpy as np

from models import PitchDNN
from wexchange.torch import dump_torch_weights
from wexchange.c_export import CWriter, print_vector

def c_export(args, model):

    message = f"Auto generated from checkpoint {os.path.basename(args.checkpoint)}"

    writer = CWriter(os.path.join(args.output_dir, "pitchdnn_data"), message=message, model_struct_name='PitchDNN')
    writer.header.write(
f"""
#include "opus_types.h"
"""
        )

    dense_layers = [
        ('if_upsample.0', "dense_if_upsampler_1"),
        ('if_upsample.2', "dense_if_upsampler_2"),
        ('downsample.0', "dense_downsampler"),
        ("upsample.0", "dense_final_upsampler")
    ]


    for name, export_name in dense_layers:
        layer = model.get_submodule(name)
        dump_torch_weights(writer, layer, name=export_name, verbose=True, quantize=True, scale=None)

    conv_layers = [
        ('conv.1', "conv2d_1"),
        ('conv.4', "conv2d_2")
    ]


    for name, export_name in conv_layers:
        layer = model.get_submodule(name)
        dump_torch_weights(writer, layer, name=export_name, verbose=True)


    gru_layers = [
        ("GRU", "gru_1"),
    ]

    max_rnn_units = max([dump_torch_weights(writer, model.get_submodule(name), export_name, verbose=True, input_sparse=False, quantize=True, scale=None, recurrent_scale=None)
                             for name, export_name in gru_layers])

    writer.header.write(
f"""

#define PITCH_DNN_MAX_RNN_UNITS {max_rnn_units}

"""
        )

    writer.close()


if __name__ == "__main__":

    os.makedirs(args.output_dir, exist_ok=True)
    model = PitchDNN()
    checkpoint = torch.load(args.checkpoint, map_location='cpu')
    model.load_state_dict(checkpoint['state_dict'])
    c_export(args, model)

import os
import sys
import argparse

import torch
from torch import nn


sys.path.append(os.path.join(os.path.split(__file__)[0], '../weight-exchange'))
import wexchange.torch

from models import model_dict

unquantized = [
    'bfcc_with_corr_upsampler.fc',
    'cont_net.0',
    'fwc6.cont_fc.0',
    'fwc6.fc.0',
    'fwc6.fc.1.gate',
    'fwc7.cont_fc.0',
    'fwc7.fc.0',
    'fwc7.fc.1.gate'
]

description=f"""
This is an unsafe dumping script for FWGAN models. It assumes that all weights are included in Linear, Conv1d or GRU layer
and will fail to export any other weights.

Furthermore, the quanitze option relies on the following explicit list of layers to be excluded:
{unquantized}.

Modify this script manually if adjustments are needed.
"""

parser = argparse.ArgumentParser(description=description)
parser.add_argument('model', choices=['fwgan400', 'fwgan500'], help='model name')
parser.add_argument('weightfile', type=str, help='weight file path')
parser.add_argument('export_folder', type=str)
parser.add_argument('--export-filename', type=str, default='fwgan_data', help='filename for source and header file (.c and .h will be added), defaults to fwgan_data')
parser.add_argument('--struct-name', type=str, default='FWGAN', help='name for C struct, defaults to FWGAN')
parser.add_argument('--quantize', action='store_true', help='apply quantization')

if __name__ == "__main__":
    args = parser.parse_args()

    model = model_dict[args.model]()

    print(f"loading weights from {args.weightfile}...")
    saved_gen= torch.load(args.weightfile, map_location='cpu')
    model.load_state_dict(saved_gen)
    def _remove_weight_norm(m):
        try:
            torch.nn.utils.remove_weight_norm(m)
        except ValueError:  # this module didn't have weight norm
            return
    model.apply(_remove_weight_norm)


    print("dumping model...")
    quantize_model=args.quantize

    output_folder = args.export_folder
    os.makedirs(output_folder, exist_ok=True)

    writer = wexchange.c_export.c_writer.CWriter(os.path.join(output_folder, args.export_filename), model_struct_name=args.struct_name)

    for name, module in model.named_modules():

        if quantize_model:
            quantize=name not in unquantized
            scale = None if quantize else 1/128
        else:
            quantize=False
            scale=1/128

        if isinstance(module, nn.Linear):
            print(f"dumping linear layer {name}...")
            wexchange.torch.dump_torch_dense_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale)

        if isinstance(module, nn.Conv1d):
            print(f"dumping conv1d layer {name}...")
            wexchange.torch.dump_torch_conv1d_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale)

        if isinstance(module, nn.GRU):
            print(f"dumping GRU layer {name}...")
            wexchange.torch.dump_torch_gru_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale, recurrent_scale=scale)

    writer.close()

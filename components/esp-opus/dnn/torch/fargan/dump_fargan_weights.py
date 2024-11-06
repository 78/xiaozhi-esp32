import os
import sys
import argparse

import torch
from torch import nn


sys.path.append(os.path.join(os.path.split(__file__)[0], '../weight-exchange'))
import wexchange.torch

import fargan
#from models import model_dict

unquantized = [ 'cond_net.pembed', 'cond_net.fdense1', 'sig_net.cond_gain_dense', 'sig_net.gain_dense_out' ]

unquantized2 = [
    'cond_net.pembed',
    'cond_net.fdense1',
    'cond_net.fconv1',
    'cond_net.fconv2',
    'cont_net.0',
    'sig_net.cond_gain_dense',
    'sig_net.fwc0.conv',
    'sig_net.fwc0.glu.gate',
    'sig_net.dense1_glu.gate',
    'sig_net.gru1_glu.gate',
    'sig_net.gru2_glu.gate',
    'sig_net.gru3_glu.gate',
    'sig_net.skip_glu.gate',
    'sig_net.skip_dense',
    'sig_net.sig_dense_out',
    'sig_net.gain_dense_out'
]

description=f"""
This is an unsafe dumping script for FARGAN models. It assumes that all weights are included in Linear, Conv1d or GRU layer
and will fail to export any other weights.

Furthermore, the quanitze option relies on the following explicit list of layers to be excluded:
{unquantized}.

Modify this script manually if adjustments are needed.
"""

parser = argparse.ArgumentParser(description=description)
parser.add_argument('weightfile', type=str, help='weight file path')
parser.add_argument('export_folder', type=str)
parser.add_argument('--export-filename', type=str, default='fargan_data', help='filename for source and header file (.c and .h will be added), defaults to fargan_data')
parser.add_argument('--struct-name', type=str, default='FARGAN', help='name for C struct, defaults to FARGAN')
parser.add_argument('--quantize', action='store_true', help='apply quantization')

if __name__ == "__main__":
    args = parser.parse_args()

    print(f"loading weights from {args.weightfile}...")
    saved_gen= torch.load(args.weightfile, map_location='cpu')
    saved_gen['model_args']    = ()
    saved_gen['model_kwargs']  = {'cond_size': 256, 'gamma': 0.9}

    model = fargan.FARGAN(*saved_gen['model_args'], **saved_gen['model_kwargs'])
    model.load_state_dict(saved_gen['state_dict'], strict=False)
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

        elif isinstance(module, nn.Conv1d):
            print(f"dumping conv1d layer {name}...")
            wexchange.torch.dump_torch_conv1d_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale)

        elif isinstance(module, nn.GRU):
            print(f"dumping GRU layer {name}...")
            wexchange.torch.dump_torch_gru_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale, recurrent_scale=scale)

        elif isinstance(module, nn.GRUCell):
            print(f"dumping GRUCell layer {name}...")
            wexchange.torch.dump_torch_grucell_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale, recurrent_scale=scale)

        elif isinstance(module, nn.Embedding):
            print(f"dumping Embedding layer {name}...")
            wexchange.torch.dump_torch_embedding_weights(writer, module, name.replace('.', '_'), quantize=quantize, scale=scale)
            #wexchange.torch.dump_torch_embedding_weights(writer, module)

        else:
            print(f"Ignoring layer {name}...")

    writer.close()

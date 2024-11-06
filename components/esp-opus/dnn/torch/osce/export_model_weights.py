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

import hashlib

sys.path.append(os.path.join(os.path.dirname(__file__), '../weight-exchange'))

import torch
import wexchange.torch
from wexchange.torch import dump_torch_weights
from models import model_dict

from utils.layers.limited_adaptive_comb1d import LimitedAdaptiveComb1d
from utils.layers.limited_adaptive_conv1d import LimitedAdaptiveConv1d
from utils.layers.td_shaper import TDShaper
from utils.misc import remove_all_weight_norm
from wexchange.torch import dump_torch_weights



parser = argparse.ArgumentParser()

parser.add_argument('checkpoint', type=str, help='LACE or NoLACE model checkpoint')
parser.add_argument('output_dir', type=str, help='output folder')
parser.add_argument('--quantize', action="store_true", help='quantization according to schedule')

sparse_default=False
schedules = {
    'nolace': [
        ('pitch_embedding', dict()),
        ('feature_net.conv1', dict()),
        ('feature_net.conv2', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('feature_net.tconv', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('feature_net.gru', dict(quantize=True, scale=None, recurrent_scale=None, input_sparse=sparse_default, recurrent_sparse=sparse_default)),
        ('cf1', dict(quantize=True, scale=None)),
        ('cf2', dict(quantize=True, scale=None)),
        ('af1', dict(quantize=True, scale=None)),
        ('tdshape1', dict(quantize=True, scale=None)),
        ('tdshape2', dict(quantize=True, scale=None)),
        ('tdshape3', dict(quantize=True, scale=None)),
        ('af2', dict(quantize=True, scale=None)),
        ('af3', dict(quantize=True, scale=None)),
        ('af4', dict(quantize=True, scale=None)),
        ('post_cf1', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('post_cf2', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('post_af1', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('post_af2', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('post_af3', dict(quantize=True, scale=None, sparse=sparse_default))
    ],
    'lace' : [
        ('pitch_embedding', dict()),
        ('feature_net.conv1', dict()),
        ('feature_net.conv2', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('feature_net.tconv', dict(quantize=True, scale=None, sparse=sparse_default)),
        ('feature_net.gru', dict(quantize=True, scale=None, recurrent_scale=None, input_sparse=sparse_default, recurrent_sparse=sparse_default)),
        ('cf1', dict(quantize=True, scale=None)),
        ('cf2', dict(quantize=True, scale=None)),
        ('af1', dict(quantize=True, scale=None))
    ]
}


# auxiliary functions
def sha1(filename):
    BUF_SIZE = 65536
    sha1 = hashlib.sha1()

    with open(filename, 'rb') as f:
        while True:
            data = f.read(BUF_SIZE)
            if not data:
                break
            sha1.update(data)

    return sha1.hexdigest()

def osce_dump_generic(writer, name, module):
    if isinstance(module, torch.nn.Linear) or isinstance(module, torch.nn.Conv1d) \
            or isinstance(module, torch.nn.ConvTranspose1d) or isinstance(module, torch.nn.Embedding) \
                or isinstance(module, LimitedAdaptiveConv1d) or isinstance(module, LimitedAdaptiveComb1d) \
                    or isinstance(module, TDShaper) or isinstance(module, torch.nn.GRU):
                        dump_torch_weights(writer, module, name=name, verbose=True)
    else:
        for child_name, child in module.named_children():
            osce_dump_generic(writer, (name + "_" + child_name).replace("feature_net", "fnet"), child)


def export_name(name):
    name = name.replace('.', '_')
    name = name.replace('feature_net', 'fnet')
    return name

def osce_scheduled_dump(writer, prefix, model, schedule):
    if not prefix.endswith('_'):
        prefix += '_'

    for name, kwargs in schedule:
        dump_torch_weights(writer, model.get_submodule(name), prefix + export_name(name), **kwargs, verbose=True)

if __name__ == "__main__":
    args = parser.parse_args()

    checkpoint_path = args.checkpoint
    outdir = args.output_dir
    os.makedirs(outdir, exist_ok=True)

    # dump message
    message = f"Auto generated from checkpoint {os.path.basename(checkpoint_path)} (sha1: {sha1(checkpoint_path)})"

    # create model and load weights
    checkpoint = torch.load(checkpoint_path, map_location='cpu')
    model = model_dict[checkpoint['setup']['model']['name']](*checkpoint['setup']['model']['args'], **checkpoint['setup']['model']['kwargs'])
    model.load_state_dict(checkpoint['state_dict'])
    remove_all_weight_norm(model, verbose=True)

    # CWriter
    model_name = checkpoint['setup']['model']['name']
    cwriter = wexchange.c_export.CWriter(os.path.join(outdir, model_name + "_data"), message=message, model_struct_name=model_name.upper() + 'Layers', add_typedef=True)

    # Add custom includes and global parameters
    cwriter.header.write(f'''
#define {model_name.upper()}_PREEMPH {model.preemph}f
#define {model_name.upper()}_FRAME_SIZE {model.FRAME_SIZE}
#define {model_name.upper()}_OVERLAP_SIZE 40
#define {model_name.upper()}_NUM_FEATURES {model.num_features}
#define {model_name.upper()}_PITCH_MAX {model.pitch_max}
#define {model_name.upper()}_PITCH_EMBEDDING_DIM {model.pitch_embedding_dim}
#define {model_name.upper()}_NUMBITS_RANGE_LOW {model.numbits_range[0]}
#define {model_name.upper()}_NUMBITS_RANGE_HIGH {model.numbits_range[1]}
#define {model_name.upper()}_NUMBITS_EMBEDDING_DIM {model.numbits_embedding_dim}
#define {model_name.upper()}_COND_DIM {model.cond_dim}
#define {model_name.upper()}_HIDDEN_FEATURE_DIM {model.hidden_feature_dim}
''')

    for i, s in enumerate(model.numbits_embedding.scale_factors):
        cwriter.header.write(f"#define {model_name.upper()}_NUMBITS_SCALE_{i} {float(s.detach().cpu())}f\n")

    # dump layers
    if model_name in schedules and args.quantize:
        osce_scheduled_dump(cwriter, model_name, model, schedules[model_name])
    else:
        osce_dump_generic(cwriter, model_name, model)

    cwriter.close()

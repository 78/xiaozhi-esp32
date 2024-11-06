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

parser.add_argument('checkpoint', type=str, help='rdovae model checkpoint')
parser.add_argument('output_dir', type=str, help='output folder')
parser.add_argument('--format', choices=['C', 'numpy'], help='output format, default: C', default='C')

args = parser.parse_args()

import torch
import numpy as np

from rdovae import RDOVAE
from wexchange.torch import dump_torch_weights
from wexchange.c_export import CWriter, print_vector

def print_xml(xmlout, val, param, anchor, name):
    xmlout.write(
f"""
            <table anchor="{anchor}_{name}">
                <name>{param} values for {name}</name>
                <thead>
                    <tr><th>k</th><th>Q0</th><th>Q1</th><th>Q2</th><th>Q3</th><th>Q4</th><th>Q5</th><th>Q6</th><th>Q7</th><th>Q8</th><th>Q9</th><th>Q10</th><th>Q11</th><th>Q12</th><th>Q13</th><th>Q14</th><th>Q15</th></tr>
                </thead>
                <tbody>
""")
    for k in range(val.shape[1]):
        xmlout.write(f"        <tr><th>{k}</th>")
        for j in range(val.shape[0]):
            xmlout.write(f"<th>{val[j][k]}</th>")
        xmlout.write("</tr>\n")
    xmlout.write(
f"""
                </tbody>
            </table>
""")
def dump_statistical_model(writer, w, name, xmlout):
    levels = w.shape[0]

    print("printing statistical model")
    quant_scales    = torch.nn.functional.softplus(w[:, 0, :]).numpy()
    dead_zone       = 0.05 * torch.nn.functional.softplus(w[:, 1, :]).numpy()
    r               = torch.sigmoid(w[:, 5 , :]).numpy()
    p0              = torch.sigmoid(w[:, 4 , :]).numpy()
    p0              = 1 - r ** (0.5 + 0.5 * p0)

    scales_norm = 255./256./(1e-15+np.max(quant_scales,axis=0))
    quant_scales = quant_scales*scales_norm
    quant_scales_q8 = np.round(quant_scales * 2**8).astype(np.uint16)
    dead_zone_q8   = np.clip(np.round(dead_zone * 2**8), 0, 255).astype(np.uint16)
    r_q8           = np.clip(np.round(r * 2**8), 0, 255).astype(np.uint8)
    p0_q8          = np.clip(np.round(p0 * 2**8), 0, 255).astype(np.uint16)

    mask = (np.max(r_q8,axis=0) > 0) * (np.min(p0_q8,axis=0) < 255)
    quant_scales_q8 = quant_scales_q8[:, mask]
    dead_zone_q8 = dead_zone_q8[:, mask]
    r_q8 = r_q8[:, mask]
    p0_q8 = p0_q8[:, mask]
    N = r_q8.shape[-1]

    print_vector(writer.source, quant_scales_q8, f'dred_{name}_quant_scales_q8', dtype='opus_uint8', static=False)
    print_vector(writer.source, dead_zone_q8, f'dred_{name}_dead_zone_q8', dtype='opus_uint8', static=False)
    print_vector(writer.source, r_q8, f'dred_{name}_r_q8', dtype='opus_uint8', static=False)
    print_vector(writer.source, p0_q8, f'dred_{name}_p0_q8', dtype='opus_uint8', static=False)

    print_xml(xmlout, quant_scales_q8, "Scale", "scale", name)
    print_xml(xmlout, dead_zone_q8, "Dead zone", "deadzone", name)
    print_xml(xmlout, r_q8, "Decay (r)", "decay", name)
    print_xml(xmlout, p0_q8, "P(0)", "p0", name)

    writer.header.write(
f"""
extern const opus_uint8 dred_{name}_quant_scales_q8[{levels * N}];
extern const opus_uint8 dred_{name}_dead_zone_q8[{levels * N}];
extern const opus_uint8 dred_{name}_r_q8[{levels * N}];
extern const opus_uint8 dred_{name}_p0_q8[{levels * N}];

"""
    )
    return N, mask, torch.tensor(scales_norm[mask])


def c_export(args, model):

    message = f"Auto generated from checkpoint {os.path.basename(args.checkpoint)}"

    enc_writer = CWriter(os.path.join(args.output_dir, "dred_rdovae_enc_data"), message=message, model_struct_name='RDOVAEEnc')
    dec_writer = CWriter(os.path.join(args.output_dir, "dred_rdovae_dec_data"), message=message, model_struct_name='RDOVAEDec')
    stats_writer = CWriter(os.path.join(args.output_dir, "dred_rdovae_stats_data"), message=message, enable_binary_blob=False)
    constants_writer = CWriter(os.path.join(args.output_dir, "dred_rdovae_constants"), message=message, header_only=True, enable_binary_blob=False)
    xmlout = open("stats.xml", "w")

    # some custom includes
    for writer in [enc_writer, dec_writer]:
        writer.header.write(
f"""
#include "opus_types.h"

#include "dred_rdovae.h"

#include "dred_rdovae_constants.h"

"""
        )

    stats_writer.header.write(
f"""
#include "opus_types.h"

#include "dred_rdovae_constants.h"

"""
        )

    latent_out = model.get_submodule('core_encoder.module.z_dense')
    state_out = model.get_submodule('core_encoder.module.state_dense_2')
    orig_latent_dim = latent_out.weight.shape[0]
    orig_state_dim = state_out.weight.shape[0]
    # statistical model
    qembedding = model.statistical_model.quant_embedding.weight.detach()
    levels = qembedding.shape[0]
    qembedding = torch.reshape(qembedding, (levels, 6, -1))

    latent_dim, latent_mask, latent_scale = dump_statistical_model(stats_writer, qembedding[:, :, :orig_latent_dim], 'latent', xmlout)
    state_dim, state_mask, state_scale = dump_statistical_model(stats_writer, qembedding[:, :, orig_latent_dim:], 'state', xmlout)

    padded_latent_dim = (latent_dim+7)//8*8
    latent_pad = padded_latent_dim - latent_dim;
    w = latent_out.weight[latent_mask,:]
    w = w/latent_scale[:, None]
    w = torch.cat([w, torch.zeros(latent_pad, w.shape[1])], dim=0)
    b = latent_out.bias[latent_mask]
    b = b/latent_scale
    b = torch.cat([b, torch.zeros(latent_pad)], dim=0)
    latent_out.weight = torch.nn.Parameter(w)
    latent_out.bias = torch.nn.Parameter(b)

    padded_state_dim = (state_dim+7)//8*8
    state_pad = padded_state_dim - state_dim;
    w = state_out.weight[state_mask,:]
    w = w/state_scale[:, None]
    w = torch.cat([w, torch.zeros(state_pad, w.shape[1])], dim=0)
    b = state_out.bias[state_mask]
    b = b/state_scale
    b = torch.cat([b, torch.zeros(state_pad)], dim=0)
    state_out.weight = torch.nn.Parameter(w)
    state_out.bias = torch.nn.Parameter(b)

    latent_in = model.get_submodule('core_decoder.module.dense_1')
    state_in = model.get_submodule('core_decoder.module.hidden_init')
    latent_in.weight = torch.nn.Parameter(latent_in.weight[:,latent_mask]*latent_scale)
    state_in.weight = torch.nn.Parameter(state_in.weight[:,state_mask]*state_scale)

    # encoder
    encoder_dense_layers = [
        ('core_encoder.module.dense_1'       , 'enc_dense1',   'TANH', False,),
        ('core_encoder.module.z_dense'       , 'enc_zdense',   'LINEAR', True,),
        ('core_encoder.module.state_dense_1' , 'gdense1'    ,   'TANH', True,),
        ('core_encoder.module.state_dense_2' , 'gdense2'    ,   'TANH', True)
    ]

    for name, export_name, _, quantize in encoder_dense_layers:
        layer = model.get_submodule(name)
        dump_torch_weights(enc_writer, layer, name=export_name, verbose=True, quantize=quantize, scale=None)


    encoder_gru_layers = [
        ('core_encoder.module.gru1'       , 'enc_gru1',   'TANH', True),
        ('core_encoder.module.gru2'       , 'enc_gru2',   'TANH', True),
        ('core_encoder.module.gru3'       , 'enc_gru3',   'TANH', True),
        ('core_encoder.module.gru4'       , 'enc_gru4',   'TANH', True),
        ('core_encoder.module.gru5'       , 'enc_gru5',   'TANH', True),
    ]

    enc_max_rnn_units = max([dump_torch_weights(enc_writer, model.get_submodule(name), export_name, verbose=True, input_sparse=True, quantize=quantize, scale=None, recurrent_scale=None)
                             for name, export_name, _, quantize in encoder_gru_layers])


    encoder_conv_layers = [
        ('core_encoder.module.conv1.conv'       , 'enc_conv1',   'TANH', True),
        ('core_encoder.module.conv2.conv'       , 'enc_conv2',   'TANH', True),
        ('core_encoder.module.conv3.conv'       , 'enc_conv3',   'TANH', True),
        ('core_encoder.module.conv4.conv'       , 'enc_conv4',   'TANH', True),
        ('core_encoder.module.conv5.conv'       , 'enc_conv5',   'TANH', True),
    ]

    enc_max_conv_inputs = max([dump_torch_weights(enc_writer, model.get_submodule(name), export_name, verbose=True, quantize=quantize, scale=None) for name, export_name, _, quantize in encoder_conv_layers])


    del enc_writer

    # decoder
    decoder_dense_layers = [
        ('core_decoder.module.dense_1'      , 'dec_dense1',  'TANH', False),
        ('core_decoder.module.glu1.gate'    , 'dec_glu1',    'TANH', True),
        ('core_decoder.module.glu2.gate'    , 'dec_glu2',    'TANH', True),
        ('core_decoder.module.glu3.gate'    , 'dec_glu3',    'TANH', True),
        ('core_decoder.module.glu4.gate'    , 'dec_glu4',    'TANH', True),
        ('core_decoder.module.glu5.gate'    , 'dec_glu5',    'TANH', True),
        ('core_decoder.module.output'       , 'dec_output',  'LINEAR', True),
        ('core_decoder.module.hidden_init'  , 'dec_hidden_init',        'TANH', False),
        ('core_decoder.module.gru_init'     , 'dec_gru_init','TANH', True),
    ]

    for name, export_name, _, quantize in decoder_dense_layers:
        layer = model.get_submodule(name)
        dump_torch_weights(dec_writer, layer, name=export_name, verbose=True, quantize=quantize, scale=None)


    decoder_gru_layers = [
        ('core_decoder.module.gru1'         , 'dec_gru1',    'TANH', True),
        ('core_decoder.module.gru2'         , 'dec_gru2',    'TANH', True),
        ('core_decoder.module.gru3'         , 'dec_gru3',    'TANH', True),
        ('core_decoder.module.gru4'         , 'dec_gru4',    'TANH', True),
        ('core_decoder.module.gru5'         , 'dec_gru5',    'TANH', True),
    ]

    dec_max_rnn_units = max([dump_torch_weights(dec_writer, model.get_submodule(name), export_name, verbose=True, input_sparse=True, quantize=quantize, scale=None, recurrent_scale=None)
                             for name, export_name, _, quantize in decoder_gru_layers])

    decoder_conv_layers = [
        ('core_decoder.module.conv1.conv'       , 'dec_conv1',   'TANH', True),
        ('core_decoder.module.conv2.conv'       , 'dec_conv2',   'TANH', True),
        ('core_decoder.module.conv3.conv'       , 'dec_conv3',   'TANH', True),
        ('core_decoder.module.conv4.conv'       , 'dec_conv4',   'TANH', True),
        ('core_decoder.module.conv5.conv'       , 'dec_conv5',   'TANH', True),
    ]

    dec_max_conv_inputs = max([dump_torch_weights(dec_writer, model.get_submodule(name), export_name, verbose=True, quantize=quantize, scale=None) for name, export_name, _, quantize in decoder_conv_layers])

    del dec_writer

    del stats_writer

    # constants
    constants_writer.header.write(
f"""
#define DRED_NUM_FEATURES {model.feature_dim}

#define DRED_LATENT_DIM {latent_dim}

#define DRED_STATE_DIM {state_dim}

#define DRED_PADDED_LATENT_DIM {padded_latent_dim}

#define DRED_PADDED_STATE_DIM {padded_state_dim}

#define DRED_NUM_QUANTIZATION_LEVELS {model.quant_levels}

#define DRED_MAX_RNN_NEURONS {max(enc_max_rnn_units, dec_max_rnn_units)}

#define DRED_MAX_CONV_INPUTS {max(enc_max_conv_inputs, dec_max_conv_inputs)}

#define DRED_ENC_MAX_RNN_NEURONS {enc_max_conv_inputs}

#define DRED_ENC_MAX_CONV_INPUTS {enc_max_conv_inputs}

#define DRED_DEC_MAX_RNN_NEURONS {dec_max_rnn_units}

"""
    )

    del constants_writer


def numpy_export(args, model):

    exchange_name_to_name = {
        'encoder_stack_layer1_dense'    : 'core_encoder.module.dense_1',
        'encoder_stack_layer3_dense'    : 'core_encoder.module.dense_2',
        'encoder_stack_layer5_dense'    : 'core_encoder.module.dense_3',
        'encoder_stack_layer7_dense'    : 'core_encoder.module.dense_4',
        'encoder_stack_layer8_dense'    : 'core_encoder.module.dense_5',
        'encoder_state_layer1_dense'    : 'core_encoder.module.state_dense_1',
        'encoder_state_layer2_dense'    : 'core_encoder.module.state_dense_2',
        'encoder_stack_layer2_gru'      : 'core_encoder.module.gru_1',
        'encoder_stack_layer4_gru'      : 'core_encoder.module.gru_2',
        'encoder_stack_layer6_gru'      : 'core_encoder.module.gru_3',
        'encoder_stack_layer9_conv'     : 'core_encoder.module.conv1',
        'statistical_model_embedding'   : 'statistical_model.quant_embedding',
        'decoder_state1_dense'          : 'core_decoder.module.gru_1_init',
        'decoder_state2_dense'          : 'core_decoder.module.gru_2_init',
        'decoder_state3_dense'          : 'core_decoder.module.gru_3_init',
        'decoder_stack_layer1_dense'    : 'core_decoder.module.dense_1',
        'decoder_stack_layer3_dense'    : 'core_decoder.module.dense_2',
        'decoder_stack_layer5_dense'    : 'core_decoder.module.dense_3',
        'decoder_stack_layer7_dense'    : 'core_decoder.module.dense_4',
        'decoder_stack_layer8_dense'    : 'core_decoder.module.dense_5',
        'decoder_stack_layer9_dense'    : 'core_decoder.module.output',
        'decoder_stack_layer2_gru'      : 'core_decoder.module.gru_1',
        'decoder_stack_layer4_gru'      : 'core_decoder.module.gru_2',
        'decoder_stack_layer6_gru'      : 'core_decoder.module.gru_3'
    }

    name_to_exchange_name = {value : key for key, value in exchange_name_to_name.items()}

    for name, exchange_name in name_to_exchange_name.items():
        print(f"printing layer {name}...")
        dump_torch_weights(os.path.join(args.output_dir, exchange_name), model.get_submodule(name))


if __name__ == "__main__":


    os.makedirs(args.output_dir, exist_ok=True)


    # load model from checkpoint
    checkpoint = torch.load(args.checkpoint, map_location='cpu')
    model = RDOVAE(*checkpoint['model_args'], **checkpoint['model_kwargs'])
    missing_keys, unmatched_keys = model.load_state_dict(checkpoint['state_dict'], strict=False)
    def _remove_weight_norm(m):
        try:
            torch.nn.utils.remove_weight_norm(m)
        except ValueError:  # this module didn't have weight norm
            return
    model.apply(_remove_weight_norm)


    if len(missing_keys) > 0:
        raise ValueError(f"error: missing keys in state dict")

    if len(unmatched_keys) > 0:
        print(f"warning: the following keys were unmatched {unmatched_keys}")

    if args.format == 'C':
        c_export(args, model)
    elif args.format == 'numpy':
        numpy_export(args, model)
    else:
        raise ValueError(f'error: unknown export format {args.format}')

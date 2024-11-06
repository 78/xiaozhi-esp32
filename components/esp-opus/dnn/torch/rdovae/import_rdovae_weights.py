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
os.environ['CUDA_VISIBLE_DEVICES'] = ""

import argparse



parser = argparse.ArgumentParser()

parser.add_argument('exchange_folder', type=str, help='exchange folder path')
parser.add_argument('output', type=str, help='path to output model checkpoint')

model_group = parser.add_argument_group(title="model parameters")
model_group.add_argument('--num-features', type=int, help="number of features, default: 20", default=20)
model_group.add_argument('--latent-dim', type=int, help="number of symbols produces by encoder, default: 80", default=80)
model_group.add_argument('--cond-size', type=int, help="first conditioning size, default: 256", default=256)
model_group.add_argument('--cond-size2', type=int, help="second conditioning size, default: 256", default=256)
model_group.add_argument('--state-dim', type=int, help="dimensionality of transfered state, default: 24", default=24)
model_group.add_argument('--quant-levels', type=int, help="number of quantization levels, default: 40", default=40)

args = parser.parse_args()

import torch
from rdovae import RDOVAE
from wexchange.torch import load_torch_weights

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

if __name__ == "__main__":
    checkpoint = dict()

    # parameters
    num_features    = args.num_features
    latent_dim      = args.latent_dim
    quant_levels    = args.quant_levels
    cond_size       = args.cond_size
    cond_size2      = args.cond_size2
    state_dim       = args.state_dim


    # model
    checkpoint['model_args']    = (num_features, latent_dim, quant_levels, cond_size, cond_size2)
    checkpoint['model_kwargs']  = {'state_dim': state_dim}
    model = RDOVAE(*checkpoint['model_args'], **checkpoint['model_kwargs'])

    dense_layer_names = [
        'encoder_stack_layer1_dense',
        'encoder_stack_layer3_dense',
        'encoder_stack_layer5_dense',
        'encoder_stack_layer7_dense',
        'encoder_stack_layer8_dense',
        'encoder_state_layer1_dense',
        'encoder_state_layer2_dense',
        'decoder_state1_dense',
        'decoder_state2_dense',
        'decoder_state3_dense',
        'decoder_stack_layer1_dense',
        'decoder_stack_layer3_dense',
        'decoder_stack_layer5_dense',
        'decoder_stack_layer7_dense',
        'decoder_stack_layer8_dense',
        'decoder_stack_layer9_dense'
    ]

    gru_layer_names = [
        'encoder_stack_layer2_gru',
        'encoder_stack_layer4_gru',
        'encoder_stack_layer6_gru',
        'decoder_stack_layer2_gru',
        'decoder_stack_layer4_gru',
        'decoder_stack_layer6_gru'
    ]

    conv1d_layer_names = [
        'encoder_stack_layer9_conv'
    ]

    embedding_layer_names = [
        'statistical_model_embedding'
    ]

    for name in dense_layer_names + gru_layer_names + conv1d_layer_names + embedding_layer_names:
        print(f"loading weights for layer {exchange_name_to_name[name]}")
        layer = model.get_submodule(exchange_name_to_name[name])
        load_torch_weights(os.path.join(args.exchange_folder, name), layer)

    checkpoint['state_dict'] = model.state_dict()

    torch.save(checkpoint, args.output)
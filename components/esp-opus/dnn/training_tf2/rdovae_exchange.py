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


import argparse
import os
import sys

os.environ['CUDA_VISIBLE_DEVICES'] = ""

parser = argparse.ArgumentParser()

parser.add_argument('weights', metavar="<weight file>", type=str, help='model weight file in hdf5 format')
parser.add_argument('output', metavar="<output folder>", type=str, help='output exchange folder')
parser.add_argument('--cond-size', type=int, help="conditioning size (default: 256)", default=256)
parser.add_argument('--latent-dim', type=int, help="dimension of latent space (default: 80)", default=80)
parser.add_argument('--quant-levels', type=int, help="number of quantization steps (default: 16)", default=16)

args = parser.parse_args()

# now import the heavy stuff
from rdovae import new_rdovae_model
from wexchange.tf import dump_tf_weights, load_tf_weights


exchange_name = {
    'enc_dense1'    : 'encoder_stack_layer1_dense',
    'enc_dense3'    : 'encoder_stack_layer3_dense',
    'enc_dense5'    : 'encoder_stack_layer5_dense',
    'enc_dense7'    : 'encoder_stack_layer7_dense',
    'enc_dense8'    : 'encoder_stack_layer8_dense',
    'gdense1'       : 'encoder_state_layer1_dense',
    'gdense2'       : 'encoder_state_layer2_dense',
    'enc_dense2'    : 'encoder_stack_layer2_gru',
    'enc_dense4'    : 'encoder_stack_layer4_gru',
    'enc_dense6'    : 'encoder_stack_layer6_gru',
    'bits_dense'    : 'encoder_stack_layer9_conv',
    'qembedding'    : 'statistical_model_embedding',
    'state1'        : 'decoder_state1_dense',
    'state2'        : 'decoder_state2_dense',
    'state3'        : 'decoder_state3_dense',
    'dec_dense1'    : 'decoder_stack_layer1_dense',
    'dec_dense3'    : 'decoder_stack_layer3_dense',
    'dec_dense5'    : 'decoder_stack_layer5_dense',
    'dec_dense7'    : 'decoder_stack_layer7_dense',
    'dec_dense8'    : 'decoder_stack_layer8_dense',
    'dec_final'     : 'decoder_stack_layer9_dense',
    'dec_dense2'    : 'decoder_stack_layer2_gru',
    'dec_dense4'    : 'decoder_stack_layer4_gru',
    'dec_dense6'    : 'decoder_stack_layer6_gru'
}


if __name__ == "__main__":

    model, encoder, decoder, qembedding = new_rdovae_model(20, args.latent_dim, cond_size=args.cond_size, nb_quant=args.quant_levels)
    model.load_weights(args.weights)

    os.makedirs(args.output, exist_ok=True)

    # encoder
    encoder_dense_names = [
        'enc_dense1',
        'enc_dense3',
        'enc_dense5',
        'enc_dense7',
        'enc_dense8',
        'gdense1',
        'gdense2'
    ]

    encoder_gru_names = [
        'enc_dense2',
        'enc_dense4',
        'enc_dense6'
    ]

    encoder_conv1d_names = [
        'bits_dense'
    ]


    for name in encoder_dense_names + encoder_gru_names + encoder_conv1d_names:
        print(f"writing layer {exchange_name[name]}...")
        dump_tf_weights(os.path.join(args.output, exchange_name[name]), encoder.get_layer(name))

    # qembedding
    print(f"writing layer {exchange_name['qembedding']}...")
    dump_tf_weights(os.path.join(args.output, exchange_name['qembedding']), qembedding)

    # decoder
    decoder_dense_names = [
        'state1',
        'state2',
        'state3',
        'dec_dense1',
        'dec_dense3',
        'dec_dense5',
        'dec_dense7',
        'dec_dense8',
        'dec_final'
    ]

    decoder_gru_names = [
        'dec_dense2',
        'dec_dense4',
        'dec_dense6'
    ]

    for name in decoder_dense_names + decoder_gru_names:
        print(f"writing layer {exchange_name[name]}...")
        dump_tf_weights(os.path.join(args.output, exchange_name[name]), decoder.get_layer(name))

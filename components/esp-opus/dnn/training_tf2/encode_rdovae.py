#!/usr/bin/python3
'''Copyright (c) 2021-2022 Amazon
   Copyright (c) 2018-2019 Mozilla

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
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
'''

# Train an LPCNet model

import argparse
#from plc_loader import PLCLoader

parser = argparse.ArgumentParser(description='Train a PLC model')

parser.add_argument('features', metavar='<features file>', help='binary features file (float32)')
parser.add_argument('output', metavar='<output>', help='trained model file (.h5)')
parser.add_argument('--model', metavar='<model>', default='rdovae', help='PLC model python definition (without .py)')
group1 = parser.add_mutually_exclusive_group()
group1.add_argument('--weights', metavar='<input weights>', help='model weights')
parser.add_argument('--cond-size', metavar='<units>', default=1024, type=int, help='number of units in conditioning network (default 1024)')
parser.add_argument('--batch-size', metavar='<batch size>', default=1, type=int, help='batch size to use (default 128)')
parser.add_argument('--seq-length', metavar='<sequence length>', default=1000, type=int, help='sequence length to use (default 1000)')


args = parser.parse_args()

import importlib
rdovae = importlib.import_module(args.model)

from rdovae import apply_dead_zone

import sys
import numpy as np
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.callbacks import ModelCheckpoint, CSVLogger
import tensorflow.keras.backend as K
import h5py

import tensorflow as tf
from rdovae import pvq_quantize

# Try reducing batch_size if you run out of memory on your GPU
batch_size = args.batch_size

model, encoder, decoder, qembedding = rdovae.new_rdovae_model(nb_used_features=20, nb_bits=80, batch_size=batch_size, cond_size=args.cond_size)
model.load_weights(args.weights)

lpc_order = 16

feature_file = args.features
nb_features = model.nb_used_features + lpc_order
nb_used_features = model.nb_used_features
sequence_size = args.seq_length

# u for unquantised, load 16 bit PCM samples and convert to mu-law


features = np.memmap(feature_file, dtype='float32', mode='r')
nb_sequences = len(features)//(nb_features*sequence_size)//batch_size*batch_size
features = features[:nb_sequences*sequence_size*nb_features]

features = np.reshape(features, (nb_sequences, sequence_size, nb_features))
print(features.shape)
features = features[:, :, :nb_used_features]
#features = np.random.randn(73600, 1000, 17)


bits, gru_state_dec = encoder.predict([features], batch_size=batch_size)
(gru_state_dec).astype('float32').tofile(args.output + "-state.f32")


#dist = rdovae.feat_dist_loss(features, quant_out)
#rate = rdovae.sq1_rate_loss(features, model_bits)
#rate2 = rdovae.sq_rate_metric(features, model_bits)
#print(dist, rate, rate2)

print("shapes are:")
print(bits.shape)
print(gru_state_dec.shape)

features.astype('float32').tofile(args.output + "-input.f32")
#quant_out.astype('float32').tofile(args.output + "-enc_dec.f32")
nbits=80
bits.astype('float32').tofile(args.output + "-syms.f32")

lambda_val = 0.0002 * np.ones((nb_sequences, sequence_size//2, 1))
quant_id = np.round(3.8*np.log(lambda_val/.0002)).astype('int16')
quant_id = quant_id[:,:,0]
quant_embed = qembedding(quant_id)
quant_scale = tf.math.softplus(quant_embed[:,:,:nbits])
dead_zone = tf.math.softplus(quant_embed[:, :, nbits : 2 * nbits])

bits = bits*quant_scale
bits = np.round(apply_dead_zone([bits, dead_zone]).numpy())
bits = bits/quant_scale

gru_state_dec = pvq_quantize(gru_state_dec, 82)
#gru_state_dec = gru_state_dec/(1e-15+tf.norm(gru_state_dec, axis=-1,keepdims=True))
gru_state_dec = gru_state_dec[:,-1,:]
dec_out = decoder([bits[:,1::2,:], gru_state_dec])

print(dec_out.shape)

dec_out.numpy().astype('float32').tofile(args.output + "-quant_out.f32")

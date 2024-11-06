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
from plc_loader import PLCLoader

parser = argparse.ArgumentParser(description='Test a PLC model')

parser.add_argument('weights', metavar='<weights file>', help='weights file (.h5)')
parser.add_argument('features', metavar='<features file>', help='binary features file (float32)')
parser.add_argument('output', metavar='<output>', help='reconstructed file (float32)')
parser.add_argument('--model', metavar='<model>', default='lpcnet_plc', help='PLC model python definition (without .py)')
group1 = parser.add_mutually_exclusive_group()

parser.add_argument('--gru-size', metavar='<units>', default=256, type=int, help='number of units in GRU (default 256)')
parser.add_argument('--cond-size', metavar='<units>', default=128, type=int, help='number of units in conditioning network (default 128)')


args = parser.parse_args()

import importlib
lpcnet = importlib.import_module(args.model)

import sys
import numpy as np
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.callbacks import ModelCheckpoint, CSVLogger
import tensorflow.keras.backend as K
import h5py

import tensorflow as tf
#gpus = tf.config.experimental.list_physical_devices('GPU')
#if gpus:
#  try:
#    tf.config.experimental.set_virtual_device_configuration(gpus[0], [tf.config.experimental.VirtualDeviceConfiguration(memory_limit=5120)])
#  except RuntimeError as e:
#    print(e)

model = lpcnet.new_lpcnet_plc_model(rnn_units=args.gru_size, batch_size=1, training=False, quantize=False, cond_size=args.cond_size)
model.compile()

lpc_order = 16

feature_file = args.features
nb_features = model.nb_used_features + lpc_order
nb_used_features = model.nb_used_features

# u for unquantised, load 16 bit PCM samples and convert to mu-law

features = np.loadtxt(feature_file)
print(features.shape)
sequence_size = features.shape[0]
lost = np.reshape(features[:,-1:], (1, sequence_size, 1))
features = features[:,:nb_used_features]
features = np.reshape(features, (1, sequence_size, nb_used_features))


model.load_weights(args.weights)

features = features*lost
out = model.predict([features, lost])

out = features + (1-lost)*out

np.savetxt(args.output, out[0,:,:])

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

parser = argparse.ArgumentParser(description='Train a PLC model')

parser.add_argument('features', metavar='<features file>', help='binary features file (float32)')
parser.add_argument('lost_file', metavar='<packet loss file>', help='packet loss traces (int8)')
parser.add_argument('output', metavar='<output>', help='trained model file (.h5)')
parser.add_argument('--model', metavar='<model>', default='lpcnet_plc', help='PLC model python definition (without .py)')
group1 = parser.add_mutually_exclusive_group()
group1.add_argument('--quantize', metavar='<input weights>', help='quantize model')
group1.add_argument('--retrain', metavar='<input weights>', help='continue training model')
parser.add_argument('--gru-size', metavar='<units>', default=256, type=int, help='number of units in GRU (default 256)')
parser.add_argument('--cond-size', metavar='<units>', default=128, type=int, help='number of units in conditioning network (default 128)')
parser.add_argument('--epochs', metavar='<epochs>', default=120, type=int, help='number of epochs to train for (default 120)')
parser.add_argument('--batch-size', metavar='<batch size>', default=128, type=int, help='batch size to use (default 128)')
parser.add_argument('--seq-length', metavar='<sequence length>', default=1000, type=int, help='sequence length to use (default 1000)')
parser.add_argument('--lr', metavar='<learning rate>', type=float, help='learning rate')
parser.add_argument('--decay', metavar='<decay>', type=float, help='learning rate decay')
parser.add_argument('--band-loss', metavar='<weight>', default=1.0, type=float, help='weight of band loss (default 1.0)')
parser.add_argument('--loss-bias', metavar='<bias>', default=0.0, type=float, help='loss bias towards low energy (default 0.0)')
parser.add_argument('--logdir', metavar='<log dir>', help='directory for tensorboard log files')


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

nb_epochs = args.epochs

# Try reducing batch_size if you run out of memory on your GPU
batch_size = args.batch_size

quantize = args.quantize is not None
retrain = args.retrain is not None

if quantize:
    lr = 0.00003
    decay = 0
    input_model = args.quantize
else:
    lr = 0.001
    decay = 2.5e-5

if args.lr is not None:
    lr = args.lr

if args.decay is not None:
    decay = args.decay

if retrain:
    input_model = args.retrain

def plc_loss(alpha=1.0, bias=0.):
    def loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        e_bands = tf.signal.idct(e[:,:,:-2], norm='ortho')
        bias_mask = K.minimum(1., K.maximum(0., 4*y_true[:,:,-1:]))
        l1_loss = K.mean(K.abs(e)) + 0.1*K.mean(K.maximum(0., -e[:,:,-1:])) + alpha*K.mean(K.abs(e_bands) + bias*bias_mask*K.maximum(0., e_bands)) + K.mean(K.minimum(K.abs(e[:,:,18:19]),1.)) + 8*K.mean(K.minimum(K.abs(e[:,:,18:19]),.4))
        return l1_loss
    return loss

def plc_l1_loss():
    def L1_loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        l1_loss = K.mean(K.abs(e))
        return l1_loss
    return L1_loss

def plc_ceps_loss():
    def ceps_loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        l1_loss = K.mean(K.abs(e[:,:,:-2]))
        return l1_loss
    return ceps_loss

def plc_band_loss():
    def L1_band_loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        e_bands = tf.signal.idct(e[:,:,:-2], norm='ortho')
        l1_loss = K.mean(K.abs(e_bands))
        return l1_loss
    return L1_band_loss

def plc_pitch_loss():
    def pitch_loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        l1_loss = K.mean(K.minimum(K.abs(e[:,:,18:19]),.4))
        return l1_loss
    return pitch_loss

opt = Adam(lr, decay=decay, beta_2=0.99)
strategy = tf.distribute.experimental.MultiWorkerMirroredStrategy()

with strategy.scope():
    model = lpcnet.new_lpcnet_plc_model(rnn_units=args.gru_size, batch_size=batch_size, training=True, quantize=quantize, cond_size=args.cond_size)
    model.compile(optimizer=opt, loss=plc_loss(alpha=args.band_loss, bias=args.loss_bias), metrics=[plc_l1_loss(), plc_ceps_loss(), plc_band_loss(), plc_pitch_loss()])
    model.summary()

lpc_order = 16

feature_file = args.features
nb_features = model.nb_used_features + lpc_order + model.nb_burg_features
nb_used_features = model.nb_used_features
nb_burg_features = model.nb_burg_features
sequence_size = args.seq_length

# u for unquantised, load 16 bit PCM samples and convert to mu-law


features = np.memmap(feature_file, dtype='float32', mode='r')
nb_sequences = len(features)//(nb_features*sequence_size)//batch_size*batch_size
features = features[:nb_sequences*sequence_size*nb_features]

features = np.reshape(features, (nb_sequences, sequence_size, nb_features))

features = features[:, :, :nb_used_features+model.nb_burg_features]

lost = np.memmap(args.lost_file, dtype='int8', mode='r')

# dump models to disk as we go
checkpoint = ModelCheckpoint('{}_{}_{}.h5'.format(args.output, args.gru_size, '{epoch:02d}'))

if args.retrain is not None:
    model.load_weights(args.retrain)

if quantize or retrain:
    #Adapting from an existing model
    model.load_weights(input_model)

model.save_weights('{}_{}_initial.h5'.format(args.output, args.gru_size))

loader = PLCLoader(features, lost, nb_burg_features, batch_size)

callbacks = [checkpoint]
if args.logdir is not None:
    logdir = '{}/{}_{}_logs'.format(args.logdir, args.output, args.gru_size)
    tensorboard_callback = tf.keras.callbacks.TensorBoard(log_dir=logdir)
    callbacks.append(tensorboard_callback)

model.fit(loader, epochs=nb_epochs, validation_split=0.0, callbacks=callbacks)

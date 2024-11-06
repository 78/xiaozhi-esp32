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

import math
import tensorflow as tf
from tensorflow.keras.models import Model
from tensorflow.keras.layers import Input, GRU, Dense, Embedding, Reshape, Concatenate, Lambda, Conv1D, Multiply, Add, Bidirectional, MaxPooling1D, Activation, GaussianNoise
from tensorflow.compat.v1.keras.layers import CuDNNGRU
from tensorflow.keras import backend as K
from tensorflow.keras.constraints import Constraint
from tensorflow.keras.initializers import Initializer
from tensorflow.keras.callbacks import Callback
import numpy as np

def quant_regularizer(x):
    Q = 128
    Q_1 = 1./Q
    #return .01 * tf.reduce_mean(1 - tf.math.cos(2*3.1415926535897931*(Q*x-tf.round(Q*x))))
    return .01 * tf.reduce_mean(K.sqrt(K.sqrt(1.0001 - tf.math.cos(2*3.1415926535897931*(Q*x-tf.round(Q*x))))))


class WeightClip(Constraint):
    '''Clips the weights incident to each hidden unit to be inside a range
    '''
    def __init__(self, c=2):
        self.c = c

    def __call__(self, p):
        # Ensure that abs of adjacent weights don't sum to more than 127. Otherwise there's a risk of
        # saturation when implementing dot products with SSSE3 or AVX2.
        return self.c*p/tf.maximum(self.c, tf.repeat(tf.abs(p[:, 1::2])+tf.abs(p[:, 0::2]), 2, axis=1))
        #return K.clip(p, -self.c, self.c)

    def get_config(self):
        return {'name': self.__class__.__name__,
            'c': self.c}

constraint = WeightClip(0.992)

def new_lpcnet_plc_model(rnn_units=256, nb_used_features=20, nb_burg_features=36, batch_size=128, training=False, adaptation=False, quantize=False, cond_size=128):
    feat = Input(shape=(None, nb_used_features+nb_burg_features), batch_size=batch_size)
    lost = Input(shape=(None, 1), batch_size=batch_size)

    fdense1 = Dense(cond_size, activation='tanh', name='plc_dense1')

    cfeat = Concatenate()([feat, lost])
    cfeat = fdense1(cfeat)
    #cfeat = Conv1D(cond_size, 3, padding='causal', activation='tanh', name='plc_conv1')(cfeat)

    quant = quant_regularizer if quantize else None

    if training:
        rnn = CuDNNGRU(rnn_units, return_sequences=True, return_state=True, name='plc_gru1', stateful=True,
              kernel_constraint=constraint, recurrent_constraint = constraint, kernel_regularizer=quant, recurrent_regularizer=quant)
        rnn2 = CuDNNGRU(rnn_units, return_sequences=True, return_state=True, name='plc_gru2', stateful=True,
              kernel_constraint=constraint, recurrent_constraint = constraint, kernel_regularizer=quant, recurrent_regularizer=quant)
    else:
        rnn = GRU(rnn_units, return_sequences=True, return_state=True, recurrent_activation="sigmoid", reset_after='true', name='plc_gru1', stateful=True,
              kernel_constraint=constraint, recurrent_constraint = constraint, kernel_regularizer=quant, recurrent_regularizer=quant)
        rnn2 = GRU(rnn_units, return_sequences=True, return_state=True, recurrent_activation="sigmoid", reset_after='true', name='plc_gru2', stateful=True,
              kernel_constraint=constraint, recurrent_constraint = constraint, kernel_regularizer=quant, recurrent_regularizer=quant)

    gru_out1, _ = rnn(cfeat)
    gru_out1 = GaussianNoise(.005)(gru_out1)
    gru_out2, _ = rnn2(gru_out1)

    out_dense = Dense(nb_used_features, activation='linear', name='plc_out')
    plc_out = out_dense(gru_out2)

    model = Model([feat, lost], plc_out)
    model.rnn_units = rnn_units
    model.cond_size = cond_size
    model.nb_used_features = nb_used_features
    model.nb_burg_features = nb_burg_features

    return model

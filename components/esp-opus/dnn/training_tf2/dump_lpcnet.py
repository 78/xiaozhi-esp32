#!/usr/bin/python3
'''Copyright (c) 2017-2018 Mozilla

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

import os
import io
import lpcnet
import sys
import numpy as np
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.layers import Layer, GRU, Dense, Conv1D, Embedding
from ulaw import ulaw2lin, lin2ulaw
from mdense import MDense
from diffembed import diff_Embed
from parameters import get_parameter
import h5py
import re
import argparse


# no cuda devices needed
os.environ['CUDA_VISIBLE_DEVICES'] = ""

# Flag for dumping e2e (differentiable lpc) network weights
flag_e2e = False


max_rnn_neurons = 1
max_conv_inputs = 1
max_mdense_tmp = 1

def printVector(f, vector, name, dtype='float', dotp=False):
    global array_list
    if dotp:
        vector = vector.reshape((vector.shape[0]//4, 4, vector.shape[1]//8, 8))
        vector = vector.transpose((2, 0, 3, 1))
    v = np.reshape(vector, (-1));
    #print('static const float ', name, '[', len(v), '] = \n', file=f)
    if name not in array_list:
        array_list.append(name)
    f.write('#ifndef USE_WEIGHTS_FILE\n')
    f.write('#define WEIGHTS_{}_DEFINED\n'.format(name))
    f.write('#define WEIGHTS_{}_TYPE WEIGHT_TYPE_{}\n'.format(name, dtype))
    f.write('static const {} {}[{}] = {{\n   '.format(dtype, name, len(v)))
    for i in range(0, len(v)):
        f.write('{}'.format(v[i]))
        if (i!=len(v)-1):
            f.write(',')
        else:
            break;
        if (i%8==7):
            f.write("\n   ")
        else:
            f.write(" ")
    #print(v, file=f)
    f.write('\n};\n')
    f.write('#endif\n\n')
    return;

def printSparseVector(f, A, name, have_diag=True):
    N = A.shape[0]
    M = A.shape[1]
    W = np.zeros((0,), dtype='int')
    W0 = np.zeros((0,))
    if have_diag:
        diag = np.concatenate([np.diag(A[:,:N]), np.diag(A[:,N:2*N]), np.diag(A[:,2*N:])])
        A[:,:N] = A[:,:N] - np.diag(np.diag(A[:,:N]))
        A[:,N:2*N] = A[:,N:2*N] - np.diag(np.diag(A[:,N:2*N]))
        A[:,2*N:] = A[:,2*N:] - np.diag(np.diag(A[:,2*N:]))
        printVector(f, diag, name + '_diag')
    AQ = np.minimum(127, np.maximum(-128, np.round(A*128))).astype('int')
    idx = np.zeros((0,), dtype='int')
    for i in range(M//8):
        pos = idx.shape[0]
        idx = np.append(idx, -1)
        nb_nonzero = 0
        for j in range(N//4):
            block = A[j*4:(j+1)*4, i*8:(i+1)*8]
            qblock = AQ[j*4:(j+1)*4, i*8:(i+1)*8]
            if np.sum(np.abs(block)) > 1e-10:
                nb_nonzero = nb_nonzero + 1
                idx = np.append(idx, j*4)
                vblock = qblock.transpose((1,0)).reshape((-1,))
                W0 = np.concatenate([W0, block.reshape((-1,))])
                W = np.concatenate([W, vblock])
        idx[pos] = nb_nonzero
    f.write('#ifdef DOT_PROD\n')
    printVector(f, W, name, dtype='qweight')
    f.write('#else /*DOT_PROD*/\n')
    printVector(f, W0, name, dtype='qweight')
    f.write('#endif /*DOT_PROD*/\n')
    #idx = np.tile(np.concatenate([np.array([N]), np.arange(N)]), 3*N//16)
    printVector(f, idx, name + '_idx', dtype='int')
    return AQ

def dump_layer_ignore(self, f, hf):
    print("ignoring layer " + self.name + " of type " + self.__class__.__name__)
    return False
Layer.dump_layer = dump_layer_ignore

def dump_sparse_gru(self, f, hf):
    global max_rnn_neurons
    name = 'sparse_' + self.name
    print("printing layer " + name + " of type sparse " + self.__class__.__name__)
    weights = self.get_weights()
    qweights = printSparseVector(f, weights[1], name + '_recurrent_weights')
    printVector(f, weights[-1], name + '_bias')
    subias = weights[-1].copy()
    subias[1,:] = subias[1,:] - np.sum(qweights*(1./128),axis=0)
    printVector(f, subias, name + '_subias')
    if hasattr(self, 'activation'):
        activation = self.activation.__name__.upper()
    else:
        activation = 'TANH'
    if hasattr(self, 'reset_after') and not self.reset_after:
        reset_after = 0
    else:
        reset_after = 1
    neurons = weights[0].shape[1]//3
    max_rnn_neurons = max(max_rnn_neurons, neurons)
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('#define {}_STATE_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    model_struct.write('  SparseGRULayer {};\n'.format(name));
    model_init.write('  if (sparse_gru_init(&model->{}, arrays, "{}_bias", "{}_subias", "{}_recurrent_weights_diag", "{}_recurrent_weights", "{}_recurrent_weights_idx",  {}, ACTIVATION_{}, {})) return 1;\n'
            .format(name, name, name, name, name, name, weights[0].shape[1]//3, activation, reset_after))
    return True

def dump_grub(self, f, hf, gru_a_size):
    global max_rnn_neurons
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    qweight = printSparseVector(f, weights[0][:gru_a_size, :], name + '_weights', have_diag=False)

    f.write('#ifdef DOT_PROD\n')
    qweight2 = np.clip(np.round(128.*weights[1]).astype('int'), -128, 127)
    printVector(f, qweight2, name + '_recurrent_weights', dotp=True, dtype='qweight')
    f.write('#else /*DOT_PROD*/\n')
    printVector(f, weights[1], name + '_recurrent_weights')
    f.write('#endif /*DOT_PROD*/\n')

    printVector(f, weights[-1], name + '_bias')
    subias = weights[-1].copy()
    subias[0,:] = subias[0,:] - np.sum(qweight*(1./128.),axis=0)
    subias[1,:] = subias[1,:] - np.sum(qweight2*(1./128.),axis=0)
    printVector(f, subias, name + '_subias')
    if hasattr(self, 'activation'):
        activation = self.activation.__name__.upper()
    else:
        activation = 'TANH'
    if hasattr(self, 'reset_after') and not self.reset_after:
        reset_after = 0
    else:
        reset_after = 1
    neurons = weights[0].shape[1]//3
    max_rnn_neurons = max(max_rnn_neurons, neurons)
    model_struct.write('  GRULayer {};\n'.format(name));
    model_init.write('  if (gru_init(&model->{}, arrays, "{}_bias", "{}_subias", "{}_weights", "{}_weights_idx", "{}_recurrent_weights", {}, {}, ACTIVATION_{}, {})) return 1;\n'
            .format(name, name, name, name, name, name, gru_a_size, weights[0].shape[1]//3, activation, reset_after))
    return True

def dump_gru_layer_dummy(self, f, hf):
    name = self.name
    weights = self.get_weights()
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('#define {}_STATE_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    return True;

GRU.dump_layer = dump_gru_layer_dummy

def dump_dense_layer_impl(name, weights, bias, activation, f, hf):
    printVector(f, weights, name + '_weights')
    printVector(f, bias, name + '_bias')
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights.shape[1]))
    model_struct.write('  DenseLayer {};\n'.format(name));
    model_init.write('  if (dense_init(&model->{}, arrays, "{}_bias", "{}_weights", {}, {}, ACTIVATION_{})) return 1;\n'
            .format(name, name, name, weights.shape[0], weights.shape[1], activation))

def dump_dense_layer(self, f, hf):
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    activation = self.activation.__name__.upper()
    dump_dense_layer_impl(name, weights[0], weights[1], activation, f, hf)
    return False

Dense.dump_layer = dump_dense_layer

def dump_mdense_layer(self, f, hf):
    global max_mdense_tmp
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    printVector(f, np.transpose(weights[0], (0, 2, 1)), name + '_weights')
    printVector(f, np.transpose(weights[1], (1, 0)), name + '_bias')
    printVector(f, np.transpose(weights[2], (1, 0)), name + '_factor')
    activation = self.activation.__name__.upper()
    max_mdense_tmp = max(max_mdense_tmp, weights[0].shape[0]*weights[0].shape[2])
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[0]))
    model_struct.write('  MDenseLayer {};\n'.format(name));
    model_init.write('  if (mdense_init(&model->{}, arrays, "{}_bias",  "{}_weights",  "{}_factor",  {}, {}, {}, ACTIVATION_{})) return 1;\n'
            .format(name, name, name, name, weights[0].shape[1], weights[0].shape[0], weights[0].shape[2], activation))
    return False
MDense.dump_layer = dump_mdense_layer

def dump_conv1d_layer(self, f, hf):
    global max_conv_inputs
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    printVector(f, weights[0], name + '_weights')
    printVector(f, weights[-1], name + '_bias')
    activation = self.activation.__name__.upper()
    max_conv_inputs = max(max_conv_inputs, weights[0].shape[1]*weights[0].shape[0])
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[2]))
    hf.write('#define {}_STATE_SIZE ({}*{})\n'.format(name.upper(), weights[0].shape[1], (weights[0].shape[0]-1)))
    hf.write('#define {}_DELAY {}\n'.format(name.upper(), (weights[0].shape[0]-1)//2))
    model_struct.write('  Conv1DLayer {};\n'.format(name));
    model_init.write('  if (conv1d_init(&model->{}, arrays, "{}_bias", "{}_weights", {}, {}, {}, ACTIVATION_{})) return 1;\n'
            .format(name, name, name, weights[0].shape[1], weights[0].shape[0], weights[0].shape[2], activation))
    return True
Conv1D.dump_layer = dump_conv1d_layer


def dump_embedding_layer_impl(name, weights, f, hf):
    printVector(f, weights, name + '_weights')
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights.shape[1]))
    model_struct.write('  EmbeddingLayer {};\n'.format(name));
    model_init.write('  if (embedding_init(&model->{}, arrays, "{}_weights", {}, {})) return 1;\n'
            .format(name, name, weights.shape[0], weights.shape[1]))

def dump_embedding_layer(self, f, hf):
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()[0]
    dump_embedding_layer_impl(name, weights, f, hf)
    return False
Embedding.dump_layer = dump_embedding_layer
diff_Embed.dump_layer = dump_embedding_layer

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('model_file', type=str, help='model weight h5 file')
    parser.add_argument('--nnet-header', type=str, help='name of c header file for dumped model', default='nnet_data.h')
    parser.add_argument('--nnet-source', type=str, help='name of c source file for dumped model', default='nnet_data.c')
    parser.add_argument('--lpc-gamma', type=float, help='LPC weighting factor. If not specified I will attempt to read it from the model file with 1 as default', default=None)
    parser.add_argument('--lookahead', type=float, help='Features lookahead. If not specified I will attempt to read it from the model file with 2 as default', default=None)

    args = parser.parse_args()

    filename = args.model_file
    with h5py.File(filename, "r") as f:
        units = min(f['model_weights']['gru_a']['gru_a']['recurrent_kernel:0'].shape)
        units2 = min(f['model_weights']['gru_b']['gru_b']['recurrent_kernel:0'].shape)
        cond_size = min(f['model_weights']['feature_dense1']['feature_dense1']['kernel:0'].shape)
        e2e = 'rc2lpc' in f['model_weights']

    model, _, _ = lpcnet.new_lpcnet_model(rnn_units1=units, rnn_units2=units2, flag_e2e = e2e, cond_size=cond_size)
    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['sparse_categorical_accuracy'])
    #model.summary()

    model.load_weights(filename, by_name=True)

    cfile = args.nnet_source
    hfile = args.nnet_header

    f = open(cfile, 'w')
    hf = open(hfile, 'w')
    model_struct = io.StringIO()
    model_init = io.StringIO()
    model_struct.write('typedef struct {\n')
    model_init.write('#ifndef DUMP_BINARY_WEIGHTS\n')
    model_init.write('int init_lpcnet_model(LPCNetModel *model, const WeightArray *arrays) {\n')
    array_list = []

    f.write('/*This file is automatically generated from a Keras model*/\n')
    f.write('/*based on model {}*/\n\n'.format(sys.argv[1]))
    f.write('#ifdef HAVE_CONFIG_H\n#include "config.h"\n#endif\n\n#include "nnet.h"\n#include "{}"\n\n'.format(hfile))

    hf.write('/*This file is automatically generated from a Keras model*/\n\n')
    hf.write('#ifndef RNN_DATA_H\n#define RNN_DATA_H\n\n#include "nnet.h"\n\n')

    if e2e:
        hf.write('/* This is an end-to-end model */\n')
        hf.write('#define END2END\n\n')
    else:
        hf.write('/* This is *not* an end-to-end model */\n')
        hf.write('/* #define END2END */\n\n')

    # LPC weighting factor
    if type(args.lpc_gamma) == type(None):
        lpc_gamma = get_parameter(model, 'lpc_gamma', 1)
    else:
        lpc_gamma = args.lpc_gamma

    hf.write('/* LPC weighting factor */\n')
    hf.write('#define LPC_GAMMA ' + str(lpc_gamma) +'f\n\n')

    # look-ahead
    if type(args.lookahead) == type(None):
        lookahead = get_parameter(model, 'lookahead', 2)
    else:
        lookahead = args.lookahead

    hf.write('/* Features look-ahead */\n')
    hf.write('#define FEATURES_DELAY ' + str(lookahead) +'\n\n')

    embed_size = lpcnet.embed_size

    E = model.get_layer('embed_sig').get_weights()[0]
    W = model.get_layer('gru_a').get_weights()[0][:embed_size,:]
    dump_embedding_layer_impl('gru_a_embed_sig', np.dot(E, W), f, hf)
    W = model.get_layer('gru_a').get_weights()[0][embed_size:2*embed_size,:]
    dump_embedding_layer_impl('gru_a_embed_pred', np.dot(E, W), f, hf)
    W = model.get_layer('gru_a').get_weights()[0][2*embed_size:3*embed_size,:]
    dump_embedding_layer_impl('gru_a_embed_exc', np.dot(E, W), f, hf)
    W = model.get_layer('gru_a').get_weights()[0][3*embed_size:,:]
    #FIXME: dump only half the biases
    b = model.get_layer('gru_a').get_weights()[2]
    dump_dense_layer_impl('gru_a_dense_feature', W, b[:len(b)//2], 'LINEAR', f, hf)

    W = model.get_layer('gru_b').get_weights()[0][model.rnn_units1:,:]
    b = model.get_layer('gru_b').get_weights()[2]
    # Set biases to zero because they'll be included in the GRU input part
    # (we need regular and SU biases)
    dump_dense_layer_impl('gru_b_dense_feature', W, 0*b[:len(b)//2], 'LINEAR', f, hf)
    dump_grub(model.get_layer('gru_b'), f, hf, model.rnn_units1)

    layer_list = []
    for i, layer in enumerate(model.layers):
        if layer.dump_layer(f, hf):
            layer_list.append(layer.name)

    dump_sparse_gru(model.get_layer('gru_a'), f, hf)

    f.write('#ifndef USE_WEIGHTS_FILE\n')
    f.write('const WeightArray lpcnet_arrays[] = {\n')
    for name in array_list:
        f.write('#ifdef WEIGHTS_{}_DEFINED\n'.format(name))
        f.write('  {{"{}", WEIGHTS_{}_TYPE, sizeof({}), {}}},\n'.format(name, name, name, name))
        f.write('#endif\n')
    f.write('  {NULL, 0, 0, NULL}\n};\n')
    f.write('#endif\n')

    model_init.write('  return 0;\n}\n')
    model_init.write('#endif\n')
    f.write(model_init.getvalue())

    hf.write('#define MAX_RNN_NEURONS {}\n\n'.format(max_rnn_neurons))
    hf.write('#define MAX_CONV_INPUTS {}\n\n'.format(max_conv_inputs))
    hf.write('#define MAX_MDENSE_TMP {}\n\n'.format(max_mdense_tmp))


    hf.write('typedef struct {\n')
    for i, name in enumerate(layer_list):
        hf.write('  float {}_state[{}_STATE_SIZE];\n'.format(name, name.upper()))
    hf.write('} NNetState;\n\n')

    model_struct.write('} LPCNetModel;\n\n')
    hf.write(model_struct.getvalue())
    hf.write('int init_lpcnet_model(LPCNetModel *model, const WeightArray *arrays);\n\n')
    hf.write('\n\n#endif\n')

    f.close()
    hf.close()

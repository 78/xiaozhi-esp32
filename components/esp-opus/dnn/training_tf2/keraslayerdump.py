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

""" helper functions for dumping some Keras layers to C files """

import numpy as np


def printVector(f, vector, name, dtype='float', dotp=False, static=True):
    """ prints vector as one-dimensional C array """
    if dotp:
        vector = vector.reshape((vector.shape[0]//4, 4, vector.shape[1]//8, 8))
        vector = vector.transpose((2, 0, 3, 1))
    v = np.reshape(vector, (-1))
    if static:
        f.write('static const {} {}[{}] = {{\n   '.format(dtype, name, len(v)))
    else:
        f.write('const {} {}[{}] = {{\n   '.format(dtype, name, len(v)))
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
    f.write('\n};\n\n')
    return vector

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
    printVector(f, idx, name + '_idx', dtype='int')
    return AQ

def dump_sparse_gru(self, f, hf):
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
    max_rnn_neurons = neurons
    f.write('const SparseGRULayer {} = {{\n   {}_bias,\n   {}_subias,\n   {}_recurrent_weights_diag,\n   {}_recurrent_weights,\n   {}_recurrent_weights_idx,\n   {}, ACTIVATION_{}, {}\n}};\n\n'
            .format(name, name, name, name, name, name, weights[0].shape[1]//3, activation, reset_after))
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('#define {}_STATE_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('extern const SparseGRULayer {};\n\n'.format(name));
    return max_rnn_neurons

def dump_gru_layer(self, f, hf, dotp=False, sparse=False):
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    if sparse:
        qweight = printSparseVector(f, weights[0], name + '_weights', have_diag=False)
    else:
        qweight = printVector(f, weights[0], name + '_weights')

    if dotp:
        f.write('#ifdef DOT_PROD\n')
        qweight2 = np.clip(np.round(128.*weights[1]).astype('int'), -128, 127)
        printVector(f, qweight2, name + '_recurrent_weights', dotp=True, dtype='qweight')
        f.write('#else /*DOT_PROD*/\n')
    else:
        qweight2 = weights[1]

    printVector(f, weights[1], name + '_recurrent_weights')
    if dotp:
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
    max_rnn_neurons = neurons
    f.write('const GRULayer {} = {{\n   {}_bias,\n   {}_subias,\n   {}_weights,\n   {},\n   {}_recurrent_weights,\n   {}, {}, ACTIVATION_{}, {}\n}};\n\n'
            .format(name, name, name, name, name + "_weights_idx" if sparse else "NULL", name, weights[0].shape[0], weights[0].shape[1]//3, activation, reset_after))
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('#define {}_STATE_SIZE {}\n'.format(name.upper(), weights[0].shape[1]//3))
    hf.write('extern const GRULayer {};\n\n'.format(name));
    return max_rnn_neurons

def dump_dense_layer_impl(name, weights, bias, activation, f, hf):
    printVector(f, weights, name + '_weights')
    printVector(f, bias, name + '_bias')
    f.write('const DenseLayer {} = {{\n   {}_bias,\n   {}_weights,\n   {}, {}, ACTIVATION_{}\n}};\n\n'
            .format(name, name, name, weights.shape[0], weights.shape[1], activation))
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights.shape[1]))
    hf.write('extern const DenseLayer {};\n\n'.format(name));

def dump_dense_layer(self, f, hf):
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    activation = self.activation.__name__.upper()
    dump_dense_layer_impl(name, weights[0], weights[1], activation, f, hf)
    return False

def dump_conv1d_layer(self, f, hf):
    name = self.name
    print("printing layer " + name + " of type " + self.__class__.__name__)
    weights = self.get_weights()
    printVector(f, weights[0], name + '_weights')
    printVector(f, weights[-1], name + '_bias')
    activation = self.activation.__name__.upper()
    max_conv_inputs = weights[0].shape[1]*weights[0].shape[0]
    f.write('const Conv1DLayer {} = {{\n   {}_bias,\n   {}_weights,\n   {}, {}, {}, ACTIVATION_{}\n}};\n\n'
            .format(name, name, name, weights[0].shape[1], weights[0].shape[0], weights[0].shape[2], activation))
    hf.write('#define {}_OUT_SIZE {}\n'.format(name.upper(), weights[0].shape[2]))
    hf.write('#define {}_STATE_SIZE ({}*{})\n'.format(name.upper(), weights[0].shape[1], (weights[0].shape[0]-1)))
    hf.write('#define {}_DELAY {}\n'.format(name.upper(), (weights[0].shape[0]-1)//2))
    hf.write('extern const Conv1DLayer {};\n\n'.format(name));
    return max_conv_inputs

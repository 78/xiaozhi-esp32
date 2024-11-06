'''Copyright (c) 2017-2018 Mozilla
   Copyright (c) 2022 Amazon

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

import numpy as np

from .c_writer import CWriter

def print_vector(writer, vector, name, dtype='float', reshape_8x4=False, static=True, debug_float=False):

    if isinstance(writer, CWriter):
        f = writer.source
        binary_blob = writer.enable_binary_blob
    else:
        f = writer
        binary_blob = False

    dtype_suffix = {
        'float' : 'float',
        'opus_int8' : 'int8',
        'opus_uint16' : 'uint16',
        'opus_int16' : 'int16',
        'int' : 'int',
        'qweight': 'qweight'
    }


    if binary_blob:
        f.write(
f'''
#ifndef USE_WEIGHTS_FILE
'''
        )
    writer.weight_arrays.append(name)

    if reshape_8x4:
        vector = vector.reshape((vector.shape[0]//4, 4, vector.shape[1]//8, 8))
        vector = vector.transpose((2, 0, 3, 1))

    v = np.reshape(vector, (-1))

    if debug_float:
        f.write('#ifndef DISABLE_DEBUG_FLOAT\n')
    f.write(
f'''
#define WEIGHTS_{name}_DEFINED
#define WEIGHTS_{name}_TYPE WEIGHT_TYPE_{dtype_suffix[dtype]}
'''
        )

    if static:
        f.write('static ')

    f.write(f'const {dtype} {name}[{len(v)}] = {{\n    ')

    for i in range(0, len(v)):

        f.write(f'{v[i]}')

        if (i!=len(v)-1):
            f.write(',')
        else:
            break

        if (i%8==7):
            f.write("\n    ")
        else:
            f.write(" ")

    f.write('\n};\n\n')
    if debug_float: f.write('#endif /*DISABLE_DEBUG_FLOAT*/\n')

    if binary_blob:
        f.write(
f'''
#endif /* USE_WEIGHTS_FILE */
'''
        )

    return vector



def extract_diagonal(A):
    """ input shape is (N, k*N) """

    N, M = A.shape
    B = A.copy()
    assert M % N == 0
    k = M // N

    diags = []
    for l in range(k):
        diag = np.diag(B[:, l * N : (l+1) * N]).copy()
        B[:, l * N : (l+1) * N] -= np.diag(diag)
        diags.append(diag)

    diag = np.concatenate(diags)

    return diag, B

def quantize_weight(weight, scale):
    scale = scale + 1e-30
    Aq = np.round(weight / scale).astype('int')
    if Aq.max() > 127 or Aq.min() <= -128:
        raise ValueError("value out of bounds in quantize_weight")
    Aq = np.clip(np.round(weight / scale).astype('int'), -128, 127)
    return Aq


def print_sparse_weight(writer, A, name, scale=1/128, have_diag=True, quantize=False):
    N = A.shape[0]
    M = A.shape[1]
    W = np.zeros((0,), dtype='int')
    W0 = np.zeros((0,))

    if have_diag:
        diag, A = extract_diagonal(A)
        print_vector(writer, diag, name + '_diag')

    if quantize:
        Aq = quantize_weight(A, scale)
    else:
        Aq = A

    # extract blocks
    idx = np.zeros((0,), dtype='int')
    for i in range(M//8):
        pos = idx.shape[0]
        idx = np.append(idx, -1)
        nb_nonzero = 0
        for j in range(N//4):
            block = A[j*4:(j+1)*4, i*8:(i+1)*8]
            qblock = Aq[j*4:(j+1)*4, i*8:(i+1)*8]
            if np.sum(np.abs(block)) > 1e-10:
                nb_nonzero = nb_nonzero + 1
                idx = np.append(idx, j*4)
                vblock = qblock.transpose((1,0)).reshape((-1,))
                W0 = np.concatenate([W0, block.reshape((-1,))])
                W = np.concatenate([W, vblock])
        idx[pos] = nb_nonzero

    if quantize: print_vector(writer, W, name + '_int8', reshape_8x4=False, dtype='opus_int8')
    print_vector(writer, W0, name + '_float', reshape_8x4=False, dtype='float', debug_float=quantize)
    print_vector(writer, idx, name + '_idx', reshape_8x4=False, dtype='int')

    return Aq



def compute_scaling(weight):
    """ computes optimal scaling vector for weight of shape (features_in, features_out) """

    n_in, n_out = weight.shape
    assert n_in % 4 == 0 and n_out % 8 == 0

    weight_max_abs = np.max(np.abs(weight), axis=0)
    weight_max_sum = np.max(np.abs(weight[: n_in : 2] + weight[1 : n_in : 2]), axis=0)
    scale_max = weight_max_abs / 127
    scale_sum = weight_max_sum / 129

    scale = np.maximum(scale_max, scale_sum)

    return scale

def qn(string):
    if string == "NULL": return string
    else: return '"' + string + '"'

def print_linear_layer(writer : CWriter,
                       name : str,
                       weight : np.ndarray,
                       bias : np.ndarray,
                       scale : np.ndarray = None,
                       sparse : bool = False,
                       diagonal : bool = False,
                       quantize : bool = True):

    """ prints linear layer

    Parameters:
    -----------
    name : str
        layer name
    weight: np.ndarray
    ...
    scale: np.ndarray or None
        If None auto scaling will be applied. Otherwise, output channels will be multiplied by scale (the usual broadcasting rules apply).


    """

    if len(weight.shape) != 2:
        raise ValueError('expecting 2-dim weight array in print_linear_layer')


    bias_name           = "NULL" if bias is None else name + "_bias"
    subias_name         = name + "_subias" if quantize else "NULL"
    scale_name          = name + "_scale" if quantize else "NULL"
    idx_name            = name + "_weights_idx" if sparse else "NULL"
    float_weight_name   = name + "_weights_float"
    int_weight_name     = name + "_weights_int8" if quantize else "NULL"
    diag_name           = name + "_weights_diag" if sparse and diagonal else "NULL"

    nb_inputs, nb_outputs = weight.shape

    if scale is None and quantize:
        scale = compute_scaling(weight)


    if sparse:
        weight_q = print_sparse_weight(writer, weight, name + "_weights", scale=scale, have_diag=diagonal, quantize=quantize)
    else:
        if quantize:
            weight_q = quantize_weight(weight, scale)
            print_vector(writer, weight_q, name + "_weights_int8", dtype='opus_int8', reshape_8x4=True)

        print_vector(writer, weight, name + "_weights_float", dtype='float', reshape_8x4=False, debug_float=quantize)

    if quantize:
        subias = (np.zeros(nb_outputs) if bias is None else bias) - np.sum(weight_q * scale, axis=0)
        print_vector(writer, subias, name + "_subias")

        final_scale = scale / 127 * np.ones(nb_outputs)
        print_vector(writer, final_scale, name + "_scale")

    if bias is not None:
        print_vector(writer, bias, name + "_bias")


    init_call = f'linear_init(&model->{name}, arrays, {qn(bias_name)}, {qn(subias_name)}, {qn(int_weight_name)},' \
        + f'{qn(float_weight_name)}, {qn(idx_name)}, {qn(diag_name)}, {qn(scale_name)}, {nb_inputs}, {nb_outputs})'

    writer.layer_dict[name] = ('LinearLayer', init_call)


def print_dense_layer(writer : CWriter,
                      name : str,
                      weight : np.ndarray,
                      bias : np.ndarray,
                      scale=1/128,
                      format : str = 'torch',
                      sparse=False,
                      diagonal=False,
                      quantize=False):

    if format == 'torch':
        weight = weight.transpose()

    print_linear_layer(writer, name, weight, bias, scale=scale, sparse=sparse, diagonal=diagonal, quantize=quantize)

    writer.header.write(f"\n#define {name.upper()}_OUT_SIZE {weight.shape[1]}\n")


def print_conv1d_layer(writer : CWriter,
                       name : str,
                       weight : np.ndarray,
                       bias : np.ndarray,
                       scale=1/128,
                       format : str = 'torch',
                       quantize=False,
                       sparse=False):


    if format == "torch":
        # convert to channels last
        weight = np.transpose(weight, (2, 1, 0))

    lin_weight = np.reshape(weight, (-1, weight.shape[-1]))
    print_linear_layer(writer, name, lin_weight, bias, scale=scale, sparse=sparse, diagonal=False, quantize=quantize)


    writer.header.write(f"\n#define {name.upper()}_OUT_SIZE {weight.shape[2]}\n")
    writer.header.write(f"\n#define {name.upper()}_IN_SIZE {weight.shape[1]}\n")
    writer.header.write(f"\n#define {name.upper()}_STATE_SIZE ({weight.shape[1]} * ({weight.shape[0] - 1}))\n")
    writer.header.write(f"\n#define {name.upper()}_DELAY {(weight.shape[0] - 1) // 2}\n") # CAVE: delay is not a property of the conv layer

    return weight.shape[0] * weight.shape[1]

def print_conv2d_layer(writer : CWriter,
                       name : str,
                       weight : np.ndarray,
                       bias : np.ndarray,
                       scale : float=1/128,
                       quantize : bool=False):

    if quantize:
        print("[print_conv2d_layer] warning: quantize argument ignored")

    bias_name = name + "_bias"
    float_weight_name = name + "_weight_float"

    print_vector(writer, weight, float_weight_name)
    print_vector(writer, bias, bias_name)

    # init function
    out_channels, in_channels, ksize1, ksize2 = weight.shape
    init_call = f'conv2d_init(&model->{name}, arrays, "{bias_name}", "{float_weight_name}", {in_channels}, {out_channels}, {ksize1}, {ksize2})'

    writer.layer_dict[name] = ('Conv2dLayer', init_call)



def print_gru_layer(writer : CWriter,
                    name : str,
                    weight : np.ndarray,
                    recurrent_weight : np.ndarray,
                    bias : np.ndarray,
                    recurrent_bias : np.ndarray,
                    format : str = 'torch',
                    quantize : bool = False,
                    input_sparse : bool = False,
                    recurrent_sparse : bool = False,
                    scale=1/128,
                    recurrent_scale=1/128
                    ):

    if format == "torch":
        # change gate ordering from rzn to zrn

        N = weight.shape[0] // 3
        for x in [weight, recurrent_weight, bias, recurrent_bias]:
            if x is None: continue
            tmp = x[0:N].copy()
            x[0:N] = x[N:2*N]
            x[N:2*N] = tmp

        weight = weight.transpose()
        recurrent_weight = recurrent_weight.transpose()
    else:
        N = weight.shape[1] // 3

    print_linear_layer(writer, name + "_input", weight, bias, scale=scale, sparse=input_sparse, quantize=quantize)
    print_linear_layer(writer, name + "_recurrent", recurrent_weight, recurrent_bias, scale=recurrent_scale, sparse=recurrent_sparse, diagonal=recurrent_sparse, quantize=quantize)

    # wrapping it up
    writer.header.write(f"\n#define {name.upper()}_OUT_SIZE {N}\n")
    writer.header.write(f"\n#define {name.upper()}_STATE_SIZE {N}\n")

    return N


def print_tconv1d_layer(writer : CWriter,
                       name : str,
                       weight : np.ndarray,
                       bias : np.ndarray,
                       stride: int,
                       scale=1/128,
                       quantize=False,
                       sparse=False):

    in_channels, out_channels, kernel_size = weight.shape


    linear_weight = weight.transpose(2, 1, 0).reshape(kernel_size * out_channels, in_channels).transpose(1, 0)
    linear_bias = np.repeat(bias[np.newaxis, :], kernel_size, 0).flatten()

    print_linear_layer(writer, name, linear_weight, linear_bias, scale=scale, quantize=quantize, sparse=sparse)

    writer.header.write(f"\n#define {name.upper()}_KERNEL_SIZE {kernel_size}\n")
    writer.header.write(f"\n#define {name.upper()}_STRIDE {stride}\n")
    writer.header.write(f"\n#define {name.upper()}_IN_CHANNELS {in_channels}\n")
    writer.header.write(f"\n#define {name.upper()}_OUT_CHANNELS {out_channels}\n")

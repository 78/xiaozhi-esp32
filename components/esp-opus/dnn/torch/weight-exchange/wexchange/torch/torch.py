"""
/* Copyright (c) 2023 Amazon
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
import sys

import torch
import numpy as np

sys.path.append(sys.path.append(os.path.join(os.path.dirname(__file__), '../osce')))
try:
    import utils.layers as osce_layers
    from utils.layers.limited_adaptive_conv1d import LimitedAdaptiveConv1d
    from utils.layers.limited_adaptive_comb1d import LimitedAdaptiveComb1d
    from utils.layers.td_shaper import TDShaper
    has_osce=True
except:
    has_osce=False

from wexchange.c_export import CWriter, print_gru_layer, print_dense_layer, print_conv1d_layer, print_tconv1d_layer, print_conv2d_layer

def dump_torch_adaptive_conv1d_weights(where, adaconv, name='adaconv', scale=1/128, quantize=False):


    w_kernel = adaconv.conv_kernel.weight.detach().cpu().numpy().copy()
    b_kernel = adaconv.conv_kernel.bias.detach().cpu().numpy().copy()
    w_gain = adaconv.filter_gain.weight.detach().cpu().numpy().copy()
    b_gain = adaconv.filter_gain.bias.detach().cpu().numpy().copy()

    if isinstance(where, CWriter):
        # pad kernel for quantization
        left_padding = adaconv.padding[0]
        kernel_size = adaconv.kernel_size
        in_channels = adaconv.in_channels
        out_channels = adaconv.out_channels
        feature_dim = adaconv.feature_dim

        if quantize and kernel_size % 8:
            kernel_padding = 8 - (kernel_size % 8)
            w_kernel = np.concatenate(
                (np.zeros((out_channels, in_channels, kernel_padding, feature_dim)), w_kernel.reshape(out_channels, in_channels, kernel_size, feature_dim)),
                dtype=w_kernel.dtype,
                axis=2).reshape(-1, feature_dim)
            b_kernel = np.concatenate(
                (np.zeros((out_channels, in_channels, kernel_padding)), b_kernel.reshape(out_channels, in_channels, kernel_size)),
                dtype=b_kernel.dtype,
                axis=2).reshape(-1)
            left_padding += kernel_padding
            kernel_size += kernel_padding

        # write relevant scalar parameters to header file
        where.header.write(f"""
#define {name.upper()}_FILTER_GAIN_A {adaconv.filter_gain_a:f}f
#define {name.upper()}_FILTER_GAIN_B {adaconv.filter_gain_b:f}f
#define {name.upper()}_SHAPE_GAIN {adaconv.shape_gain:f}f
#define {name.upper()}_KERNEL_SIZE {kernel_size}
#define {name.upper()}_FRAME_SIZE {adaconv.frame_size}
#define {name.upper()}_LEFT_PADDING {left_padding}
#define {name.upper()}_OVERLAP_SIZE {adaconv.overlap_size}
#define {name.upper()}_IN_CHANNELS {adaconv.in_channels}
#define {name.upper()}_OUT_CHANNELS {adaconv.out_channels}
#define {name.upper()}_NORM_P {adaconv.norm_p}
#define {name.upper()}_FEATURE_DIM {adaconv.feature_dim}
"""
        )

        print_dense_layer(where, name + "_kernel", w_kernel, b_kernel, scale=scale, format='torch', sparse=False, diagonal=False, quantize=quantize)
        print_dense_layer(where, name + "_gain", w_gain, b_gain, format='torch', sparse=False, diagonal=False, quantize=False)


    else:
        np.save(where, 'weight_kernel.npy', w_kernel)
        np.save(where, 'bias_kernel.npy', b_kernel)
        np.save(where, 'weight_gain.npy', w_gain)
        np.save(where, 'bias_gain.npy', b_gain)


def dump_torch_adaptive_comb1d_weights(where, adaconv, name='adaconv', scale=1/128, quantize=False):


    w_kernel = adaconv.conv_kernel.weight.detach().cpu().numpy().copy()
    b_kernel = adaconv.conv_kernel.bias.detach().cpu().numpy().copy()
    w_gain = adaconv.filter_gain.weight.detach().cpu().numpy().copy()
    b_gain = adaconv.filter_gain.bias.detach().cpu().numpy().copy()
    w_global_gain = adaconv.global_filter_gain.weight.detach().cpu().numpy().copy()
    b_global_gain = adaconv.global_filter_gain.bias.detach().cpu().numpy().copy()


    if isinstance(where, CWriter):
        # pad kernel for quantization
        left_padding = adaconv.padding[0]
        kernel_size = adaconv.kernel_size

        if quantize and w_kernel.shape[0] % 8:
            kernel_padding = 8 - (w_kernel.shape[0] % 8)
            w_kernel = np.concatenate((np.zeros((kernel_padding, w_kernel.shape[1])), w_kernel), dtype=w_kernel.dtype)
            b_kernel = np.concatenate((np.zeros((kernel_padding)), b_kernel), dtype=b_kernel.dtype)
            left_padding += kernel_padding
            kernel_size += kernel_padding
        # write relevant scalar parameters to header file
        where.header.write(f"""
#define {name.upper()}_FILTER_GAIN_A {adaconv.filter_gain_a:f}f
#define {name.upper()}_FILTER_GAIN_B {adaconv.filter_gain_b:f}f
#define {name.upper()}_LOG_GAIN_LIMIT {adaconv.log_gain_limit:f}f
#define {name.upper()}_KERNEL_SIZE {kernel_size}
#define {name.upper()}_LEFT_PADDING {left_padding}
#define {name.upper()}_FRAME_SIZE {adaconv.frame_size}
#define {name.upper()}_OVERLAP_SIZE {adaconv.overlap_size}
#define {name.upper()}_IN_CHANNELS {adaconv.in_channels}
#define {name.upper()}_OUT_CHANNELS {adaconv.out_channels}
#define {name.upper()}_NORM_P {adaconv.norm_p}
#define {name.upper()}_FEATURE_DIM {adaconv.feature_dim}
#define {name.upper()}_MAX_LAG {adaconv.max_lag}
"""
        )

        print_dense_layer(where, name + "_kernel", w_kernel, b_kernel, scale=scale, format='torch', sparse=False, diagonal=False, quantize=quantize)
        print_dense_layer(where, name + "_gain", w_gain, b_gain, format='torch', sparse=False, diagonal=False, quantize=False)
        print_dense_layer(where, name + "_global_gain", w_global_gain, b_global_gain, format='torch', sparse=False, diagonal=False, quantize=False)


    else:
        np.save(where, 'weight_kernel.npy', w_kernel)
        np.save(where, 'bias_kernel.npy', b_kernel)
        np.save(where, 'weight_gain.npy', w_gain)
        np.save(where, 'bias_gain.npy', b_gain)
        np.save(where, 'weight_global_gain.npy', w_global_gain)
        np.save(where, 'bias_global_gain.npy', b_global_gain)

def dump_torch_tdshaper(where, shaper, name='tdshaper', quantize=False, scale=1/128):

    if isinstance(where, CWriter):
        where.header.write(f"""
#define {name.upper()}_FEATURE_DIM {shaper.feature_dim}
#define {name.upper()}_FRAME_SIZE {shaper.frame_size}
#define {name.upper()}_AVG_POOL_K {shaper.avg_pool_k}
#define {name.upper()}_INNOVATE {1 if shaper.innovate else 0}
#define {name.upper()}_POOL_AFTER {1 if shaper.pool_after else 0}
"""
        )

    dump_torch_conv1d_weights(where, shaper.feature_alpha1_f, name + "_alpha1_f", quantize=quantize, scale=scale)
    dump_torch_conv1d_weights(where, shaper.feature_alpha1_t, name + "_alpha1_t")
    dump_torch_conv1d_weights(where, shaper.feature_alpha2, name + "_alpha2")

    if shaper.innovate:
        dump_torch_conv1d_weights(where, shaper.feature_alpha1b, name + "_alpha1b")
        dump_torch_conv1d_weights(where, shaper.feature_alpha1c, name + "_alpha1c")
        dump_torch_conv1d_weights(where, shaper.feature_alpha2b, name + "_alpha2b")
        dump_torch_conv1d_weights(where, shaper.feature_alpha2c, name + "_alpha2c")



def dump_torch_gru_weights(where, gru, name='gru', input_sparse=False, recurrent_sparse=False, quantize=False, scale=1/128, recurrent_scale=1/128):

    assert gru.num_layers == 1
    assert gru.bidirectional == False

    w_ih = gru.weight_ih_l0.detach().cpu().numpy().copy()
    w_hh = gru.weight_hh_l0.detach().cpu().numpy().copy()
    if hasattr(gru, 'bias_ih_l0'):
        b_ih = gru.bias_ih_l0.detach().cpu().numpy().copy()
    else:
        b_ih = None
    if hasattr(gru, 'bias_hh_l0'):
        b_hh = gru.bias_hh_l0.detach().cpu().numpy().copy()
    else:
        b_hh = None

    if isinstance(where, CWriter):
        return print_gru_layer(where, name, w_ih, w_hh, b_ih, b_hh, format='torch', input_sparse=input_sparse, recurrent_sparse=recurrent_sparse, quantize=quantize, scale=scale, recurrent_scale=recurrent_scale)
    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight_ih_rzn.npy'), w_ih)
        np.save(os.path.join(where, 'weight_hh_rzn.npy'), w_hh)
        np.save(os.path.join(where, 'bias_ih_rzn.npy'), b_ih)
        np.save(os.path.join(where, 'bias_hh_rzn.npy'), b_hh)


def dump_torch_grucell_weights(where, gru, name='gru', input_sparse=False, recurrent_sparse=False, quantize=False, scale=1/128, recurrent_scale=1/128):

    w_ih = gru.weight_ih.detach().cpu().numpy().copy()
    w_hh = gru.weight_hh.detach().cpu().numpy().copy()
    if hasattr(gru, 'bias_ih') and gru.bias_ih is not None:
        b_ih = gru.bias_ih.detach().cpu().numpy().copy()
    else:
        b_ih = None
    if hasattr(gru, 'bias_hh') and gru.bias_hh is not None:
        b_hh = gru.bias_hh.detach().cpu().numpy().copy()
    else:
        b_hh = None

    if isinstance(where, CWriter):
        return print_gru_layer(where, name, w_ih, w_hh, b_ih, b_hh, format='torch', input_sparse=input_sparse, recurrent_sparse=recurrent_sparse, quantize=quantize, scale=scale, recurrent_scale=recurrent_scale)
    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight_ih_rzn.npy'), w_ih)
        np.save(os.path.join(where, 'weight_hh_rzn.npy'), w_hh)
        np.save(os.path.join(where, 'bias_ih_rzn.npy'), b_ih)
        np.save(os.path.join(where, 'bias_hh_rzn.npy'), b_hh)



def load_torch_gru_weights(where, gru):

    assert gru.num_layers == 1
    assert gru.bidirectional == False

    w_ih = np.load(os.path.join(where, 'weight_ih_rzn.npy'))
    w_hh = np.load(os.path.join(where, 'weight_hh_rzn.npy'))
    b_ih = np.load(os.path.join(where, 'bias_ih_rzn.npy'))
    b_hh = np.load(os.path.join(where, 'bias_hh_rzn.npy'))

    with torch.no_grad():
        gru.weight_ih_l0.set_(torch.from_numpy(w_ih))
        gru.weight_hh_l0.set_(torch.from_numpy(w_hh))
        gru.bias_ih_l0.set_(torch.from_numpy(b_ih))
        gru.bias_hh_l0.set_(torch.from_numpy(b_hh))


def dump_torch_dense_weights(where, dense, name='dense', scale=1/128, sparse=False, diagonal=False, quantize=False):

    w = dense.weight.detach().cpu().numpy().copy()
    if dense.bias is None:
        b = np.zeros(dense.out_features, dtype=w.dtype)
    else:
        b = dense.bias.detach().cpu().numpy().copy()

    if isinstance(where, CWriter):
        return print_dense_layer(where, name, w, b, scale=scale, format='torch', sparse=sparse, diagonal=diagonal, quantize=quantize)

    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight.npy'), w)
        np.save(os.path.join(where, 'bias.npy'), b)


def load_torch_dense_weights(where, dense):

    w = np.load(os.path.join(where, 'weight.npy'))
    b = np.load(os.path.join(where, 'bias.npy'))

    with torch.no_grad():
        dense.weight.set_(torch.from_numpy(w))
        if dense.bias is not None:
            dense.bias.set_(torch.from_numpy(b))


def dump_torch_conv1d_weights(where, conv, name='conv', scale=1/128, quantize=False, sparse=False):

    w = conv.weight.detach().cpu().numpy().copy()
    if conv.bias is None:
        b = np.zeros(conv.out_channels, dtype=w.dtype)
    else:
        b = conv.bias.detach().cpu().numpy().copy()

    if isinstance(where, CWriter):

        return print_conv1d_layer(where, name, w, b, scale=scale, format='torch', quantize=quantize, sparse=sparse)
    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight_oik.npy'), w)

        np.save(os.path.join(where, 'bias.npy'), b)


def load_torch_conv1d_weights(where, conv):

    with torch.no_grad():
        w = np.load(os.path.join(where, 'weight_oik.npy'))
        conv.weight.set_(torch.from_numpy(w))
        if type(conv.bias) != type(None):
            b = np.load(os.path.join(where, 'bias.npy'))
            if conv.bias is not None:
                conv.bias.set_(torch.from_numpy(b))


def dump_torch_tconv1d_weights(where, conv, name='conv', scale=1/128, quantize=False, sparse=False):

    w = conv.weight.detach().cpu().numpy().copy()
    if conv.bias is None:
        b = np.zeros(conv.out_channels, dtype=w.dtype)
    else:
        b = conv.bias.detach().cpu().numpy().copy()

    if isinstance(where, CWriter):

        return print_tconv1d_layer(where, name, w, b, conv.stride[0], scale=scale, quantize=quantize, sparse=sparse)
    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight_oik.npy'), w)

        np.save(os.path.join(where, 'bias.npy'), b)


def load_torch_tconv1d_weights(where, conv):

    with torch.no_grad():
        w = np.load(os.path.join(where, 'weight_oik.npy'))
        conv.weight.set_(torch.from_numpy(w))
        if type(conv.bias) != type(None):
            b = np.load(os.path.join(where, 'bias.npy'))
            if conv.bias is not None:
                conv.bias.set_(torch.from_numpy(b))


def dump_torch_conv2d_weights(where, conv, name='conv', scale=1/128, quantize=False):
    w = conv.weight.detach().cpu().permute(0, 1, 3, 2).numpy().copy()
    if conv.bias is None:
        b = np.zeros(conv.out_channels, dtype=w.dtype)
    else:
        b = conv.bias.detach().cpu().numpy().copy()

    if isinstance(where, CWriter):
        return print_conv2d_layer(where, name, w, b, scale=scale, quantize=quantize)

    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight_oiwh.npy'), w)

        np.save(os.path.join(where, 'bias.npy'), b)

def load_torch_conv2d_weights(where, conv):
    with torch.no_grad():
        w = np.load(os.path.join(where, 'weight_oiwh.npy'))
        conv.weight.set_(torch.from_numpy(w).permute(0, 1, 3, 2))
        if type(conv.bias) != type(None):
            b = np.load(os.path.join(where, 'bias.npy'))
            if conv.bias is not None:
                conv.bias.set_(torch.from_numpy(b))


def dump_torch_embedding_weights(where, embed, name='embed', scale=1/128, sparse=False, diagonal=False, quantize=False):

    w = embed.weight.detach().cpu().numpy().copy().transpose()
    b = np.zeros(w.shape[0], dtype=w.dtype)

    if isinstance(where, CWriter):
        return print_dense_layer(where, name, w, b, scale=scale, format='torch', sparse=sparse, diagonal=diagonal, quantize=quantize)

    else:
        os.makedirs(where, exist_ok=True)

        np.save(os.path.join(where, 'weight.npy'), w)
        np.save(os.path.join(where, 'bias.npy'), b)


def load_torch_embedding_weights(where, emb):

    w = np.load(os.path.join(where, 'weight.npy'))

    with torch.no_grad():
        emb.weight.set_(torch.from_numpy(w))

def dump_torch_weights(where, module, name=None, verbose=False, **kwargs):
    """ generic function for dumping weights of some torch.nn.Module """
    if verbose and name is not None:
        print(f"printing layer {name} of type {type(module)}...")
    if isinstance(module, torch.nn.Linear):
        return dump_torch_dense_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.GRU):
        return dump_torch_gru_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.GRUCell):
        return dump_torch_grucell_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.Conv1d):
        return dump_torch_conv1d_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.Conv2d):
        return dump_torch_conv2d_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.Embedding):
        return dump_torch_embedding_weights(where, module, name, **kwargs)
    elif isinstance(module, torch.nn.ConvTranspose1d):
        return dump_torch_tconv1d_weights(where, module, name, **kwargs)
    else:
        if has_osce:
            if isinstance(module, LimitedAdaptiveConv1d):
                dump_torch_adaptive_conv1d_weights(where, module, name, **kwargs)
            elif isinstance(module, LimitedAdaptiveComb1d):
                dump_torch_adaptive_comb1d_weights(where, module, name, **kwargs)
            elif isinstance(module, TDShaper):
                dump_torch_tdshaper(where, module, name, **kwargs)
            else:
                raise ValueError(f'dump_torch_weights: layer of type {type(module)} not supported')
        else:
            raise ValueError(f'dump_torch_weights: layer of type {type(module)} not supported')

def load_torch_weights(where, module):
    """ generic function for loading weights of some torch.nn.Module """
    if isinstance(module, torch.nn.Linear):
        load_torch_dense_weights(where, module)
    elif isinstance(module, torch.nn.GRU):
        load_torch_gru_weights(where, module)
    elif isinstance(module, torch.nn.Conv1d):
        load_torch_conv1d_weights(where, module)
    elif isinstance(module, torch.nn.Conv2d):
        load_torch_conv2d_weights(where, module)
    elif isinstance(module, torch.nn.Embedding):
        load_torch_embedding_weights(where, module)
    elif isinstance(module, torch.nn.ConvTranspose1d):
        return load_torch_tconv1d_weights(where, module)
    else:
        raise ValueError(f'load_torch_weights: layer of type {type(module)} not supported')

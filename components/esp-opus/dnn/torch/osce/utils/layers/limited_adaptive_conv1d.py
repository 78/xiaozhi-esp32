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

import torch
from torch import nn
import torch.nn.functional as F

from utils.endoscopy import write_data

from utils.ada_conv import adaconv_kernel
from utils.softquant import soft_quant

class LimitedAdaptiveConv1d(nn.Module):
    COUNTER = 1

    def __init__(self,
                 in_channels,
                 out_channels,
                 kernel_size,
                 feature_dim,
                 frame_size=160,
                 overlap_size=40,
                 padding=None,
                 name=None,
                 gain_limits_db=[-6, 6],
                 shape_gain_db=0,
                 norm_p=2,
                 softquant=False,
                 apply_weight_norm=False,
                 **kwargs):
        """

        Parameters:
        -----------

        in_channels : int
            number of input channels

        out_channels : int
            number of output channels

        feature_dim : int
            dimension of features from which kernels, biases and gains are computed

        frame_size : int
            frame size

        overlap_size : int
            overlap size for filter cross-fade. Cross-fade is done on the first overlap_size samples of every frame

        use_bias : bool
            if true, biases will be added to output channels


        padding : List[int, int]

        """

        super(LimitedAdaptiveConv1d, self).__init__()



        self.in_channels    = in_channels
        self.out_channels   = out_channels
        self.feature_dim    = feature_dim
        self.kernel_size    = kernel_size
        self.frame_size     = frame_size
        self.overlap_size   = overlap_size
        self.gain_limits_db = gain_limits_db
        self.shape_gain_db  = shape_gain_db
        self.norm_p         = norm_p

        if name is None:
            self.name = "limited_adaptive_conv1d_" + str(LimitedAdaptiveConv1d.COUNTER)
            LimitedAdaptiveConv1d.COUNTER += 1
        else:
            self.name = name

        norm = torch.nn.utils.weight_norm if apply_weight_norm else lambda x, name=None: x

        # network for generating convolution weights
        self.conv_kernel = norm(nn.Linear(feature_dim, in_channels * out_channels * kernel_size))
        if softquant:
            self.conv_kernel = soft_quant(self.conv_kernel)

        self.shape_gain = min(1, 10**(shape_gain_db / 20))

        self.filter_gain = norm(nn.Linear(feature_dim, out_channels))
        log_min, log_max = gain_limits_db[0] * 0.11512925464970229, gain_limits_db[1] * 0.11512925464970229
        self.filter_gain_a = (log_max - log_min) / 2
        self.filter_gain_b = (log_max + log_min) / 2

        if type(padding) == type(None):
            self.padding = [kernel_size // 2, kernel_size - 1 - kernel_size // 2]
        else:
            self.padding = padding

        self.overlap_win = nn.Parameter(.5 + .5 * torch.cos((torch.arange(self.overlap_size) + 0.5) * torch.pi / overlap_size), requires_grad=False)


    def flop_count(self, rate):
        frame_rate = rate / self.frame_size
        overlap = self.overlap_size
        overhead = overlap / self.frame_size

        count = 0

        # kernel computation and filtering
        count += 2 * (frame_rate * self.feature_dim * self.kernel_size)
        count += 2 * (self.in_channels * self.out_channels * self.kernel_size * (1 + overhead) * rate)

        # gain computation

        count += 2 * (frame_rate * self.feature_dim * self.out_channels) + rate * (1 + overhead) * self.out_channels

        # windowing
        count += 3 * overlap * frame_rate * self.out_channels

        return count

    def forward(self, x, features, debug=False):
        """ adaptive 1d convolution


        Parameters:
        -----------
        x : torch.tensor
            input signal of shape (batch_size, in_channels, num_samples)

        feathres : torch.tensor
            frame-wise features of shape (batch_size, num_frames, feature_dim)

        """

        batch_size = x.size(0)
        num_frames = features.size(1)
        num_samples = x.size(2)
        frame_size = self.frame_size
        overlap_size = self.overlap_size
        kernel_size = self.kernel_size
        win1 = torch.flip(self.overlap_win, [0])
        win2 = self.overlap_win

        if num_samples // self.frame_size != num_frames:
            raise ValueError('non matching sizes in AdaptiveConv1d.forward')

        conv_kernels = self.conv_kernel(features).reshape((batch_size, num_frames, self.out_channels, self.in_channels, self.kernel_size))

        # normalize kernels (TODO: switch to L1 and normalize over kernel and input channel dimension)
        conv_kernels = conv_kernels / (1e-6 + torch.norm(conv_kernels, p=self.norm_p, dim=[-2, -1], keepdim=True))

        # limit shape
        id_kernels = torch.zeros_like(conv_kernels)
        id_kernels[..., self.padding[1]] = 1

        conv_kernels = self.shape_gain * conv_kernels + (1 - self.shape_gain) * id_kernels

        # calculate gains
        conv_gains   = torch.exp(self.filter_gain_a * torch.tanh(self.filter_gain(features)) + self.filter_gain_b)
        if debug and batch_size == 1:
            key = self.name + "_gains"
            write_data(key, conv_gains.permute(0, 2, 1).detach().squeeze().cpu().numpy(), 16000 // self.frame_size)
            key = self.name + "_kernels"
            write_data(key, conv_kernels.detach().squeeze().cpu().numpy(), 16000 // self.frame_size)


        conv_kernels = conv_kernels * conv_gains.view(batch_size, num_frames, self.out_channels, 1, 1)

        conv_kernels = conv_kernels.permute(0, 2, 3, 1, 4)

        output = adaconv_kernel(x, conv_kernels, win1, fft_size=256)


        return output
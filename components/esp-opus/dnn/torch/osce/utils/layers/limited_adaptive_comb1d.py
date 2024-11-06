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
from utils.softquant import soft_quant

class LimitedAdaptiveComb1d(nn.Module):
    COUNTER = 1

    def __init__(self,
                 kernel_size,
                 feature_dim,
                 frame_size=160,
                 overlap_size=40,
                 padding=None,
                 max_lag=256,
                 name=None,
                 gain_limit_db=10,
                 global_gain_limits_db=[-6, 6],
                 norm_p=2,
                 softquant=False,
                 apply_weight_norm=False,
                 **kwargs):
        """

        Parameters:
        -----------

        feature_dim : int
            dimension of features from which kernels, biases and gains are computed

        frame_size : int, optional
            frame size, defaults to 160

        overlap_size : int, optional
            overlap size for filter cross-fade. Cross-fade is done on the first overlap_size samples of every frame, defaults to 40

        use_bias : bool, optional
            if true, biases will be added to output channels. Defaults to True

        padding : List[int, int], optional
            left and right padding. Defaults to [(kernel_size - 1) // 2, kernel_size - 1 - (kernel_size - 1) // 2]

        max_lag : int, optional
            maximal pitch lag, defaults to 256

        have_a0 : bool, optional
            If true, the filter coefficient a0 will be learned as a positive gain (requires in_channels == out_channels). Otherwise, a0 is set to 0. Defaults to False

        name: str or None, optional
            specifies a name attribute for the module. If None the name is auto generated as comb_1d_COUNT, where COUNT is an instance counter for LimitedAdaptiveComb1d

        """

        super(LimitedAdaptiveComb1d, self).__init__()

        self.in_channels   = 1
        self.out_channels  = 1
        self.feature_dim   = feature_dim
        self.kernel_size   = kernel_size
        self.frame_size    = frame_size
        self.overlap_size  = overlap_size
        self.max_lag       = max_lag
        self.limit_db      = gain_limit_db
        self.norm_p        = norm_p

        if name is None:
            self.name = "limited_adaptive_comb1d_" + str(LimitedAdaptiveComb1d.COUNTER)
            LimitedAdaptiveComb1d.COUNTER += 1
        else:
            self.name = name

        norm = torch.nn.utils.weight_norm if apply_weight_norm else lambda x, name=None: x

        # network for generating convolution weights
        self.conv_kernel = norm(nn.Linear(feature_dim, kernel_size))

        if softquant:
            self.conv_kernel = soft_quant(self.conv_kernel)


        # comb filter gain
        self.filter_gain = norm(nn.Linear(feature_dim, 1))
        self.log_gain_limit = gain_limit_db * 0.11512925464970229
        with torch.no_grad():
            self.filter_gain.bias[:] = max(0.1, 4 + self.log_gain_limit)

        self.global_filter_gain = norm(nn.Linear(feature_dim, 1))
        log_min, log_max = global_gain_limits_db[0] * 0.11512925464970229, global_gain_limits_db[1] * 0.11512925464970229
        self.filter_gain_a = (log_max - log_min) / 2
        self.filter_gain_b = (log_max + log_min) / 2

        if type(padding) == type(None):
            self.padding = [kernel_size // 2, kernel_size - 1 - kernel_size // 2]
        else:
            self.padding = padding

        self.overlap_win = nn.Parameter(.5 + .5 * torch.cos((torch.arange(self.overlap_size) + 0.5) * torch.pi / overlap_size), requires_grad=False)

    def forward(self, x, features, lags, debug=False):
        """ adaptive 1d convolution


        Parameters:
        -----------
        x : torch.tensor
            input signal of shape (batch_size, in_channels, num_samples)

        feathres : torch.tensor
            frame-wise features of shape (batch_size, num_frames, feature_dim)

        lags: torch.LongTensor
            frame-wise lags for comb-filtering

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
        conv_kernels = conv_kernels / (1e-6 + torch.norm(conv_kernels, p=self.norm_p, dim=-1, keepdim=True))

        conv_gains   = torch.exp(- torch.relu(self.filter_gain(features).permute(0, 2, 1)) + self.log_gain_limit)
        # calculate gains
        global_conv_gains   = torch.exp(self.filter_gain_a * torch.tanh(self.global_filter_gain(features).permute(0, 2, 1)) + self.filter_gain_b)

        if debug and batch_size == 1:
            key = self.name + "_gains"
            write_data(key, conv_gains.detach().squeeze().cpu().numpy(), 16000 // self.frame_size)
            key = self.name + "_kernels"
            write_data(key, conv_kernels.detach().squeeze().cpu().numpy(), 16000 // self.frame_size)
            key = self.name + "_lags"
            write_data(key, lags.detach().squeeze().cpu().numpy(), 16000 // self.frame_size)
            key = self.name + "_global_conv_gains"
            write_data(key, global_conv_gains.detach().squeeze().cpu().numpy(), 16000 // self.frame_size)


        # frame-wise convolution with overlap-add
        output_frames = []
        overlap_mem = torch.zeros((batch_size, self.out_channels, self.overlap_size), device=x.device)
        x = F.pad(x, self.padding)
        x = F.pad(x, [self.max_lag, self.overlap_size])

        idx = torch.arange(frame_size + kernel_size - 1 + overlap_size).to(x.device).view(1, 1, -1)
        idx = torch.repeat_interleave(idx, batch_size, 0)
        idx = torch.repeat_interleave(idx, self.in_channels, 1)


        for i in range(num_frames):

            cidx = idx + i * frame_size + self.max_lag - lags[..., i].view(batch_size, 1, 1)
            xx = torch.gather(x, -1, cidx).reshape((1, batch_size * self.in_channels, -1))

            new_chunk = torch.conv1d(xx, conv_kernels[:, i, ...].reshape((batch_size * self.out_channels, self.in_channels, self.kernel_size)), groups=batch_size).reshape(batch_size, self.out_channels, -1)

            offset = self.max_lag + self.padding[0]
            new_chunk = global_conv_gains[:, :, i : i + 1] * (new_chunk * conv_gains[:, :, i : i + 1] + x[..., offset + i * frame_size : offset + (i + 1) * frame_size + overlap_size])

            # overlapping part
            output_frames.append(new_chunk[:, :, : overlap_size] * win1 + overlap_mem * win2)

            # non-overlapping part
            output_frames.append(new_chunk[:, :, overlap_size : frame_size])

            # mem for next frame
            overlap_mem = new_chunk[:, :, frame_size :]

        # concatenate chunks
        output = torch.cat(output_frames, dim=-1)

        return output

    def flop_count(self, rate):
        frame_rate = rate / self.frame_size
        overlap = self.overlap_size
        overhead = overlap / self.frame_size

        count = 0

        # kernel computation and filtering
        count += 2 * (frame_rate * self.feature_dim * self.kernel_size)
        count += 2 * (self.in_channels * self.out_channels * self.kernel_size * (1 + overhead) * rate)
        count += 2 * (frame_rate * self.feature_dim * self.out_channels) + rate * (1 + overhead) * self.out_channels

        # a0 computation
        count += 2 * (frame_rate * self.feature_dim * self.out_channels) + rate * (1 + overhead) * self.out_channels

        # windowing
        count += overlap * frame_rate * 3 * self.out_channels

        return count

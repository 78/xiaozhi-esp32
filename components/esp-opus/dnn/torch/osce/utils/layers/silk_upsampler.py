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

""" This module implements the SILK upsampler from 16kHz to 24 or 48 kHz """

import torch
from torch import nn
import torch.nn.functional as F

import numpy as np

frac_fir = np.array(
    [
        [189, -600, 617, 30567, 2996, -1375, 425, -46],
        [117, -159, -1070, 29704, 5784, -2143, 611, -71],
        [52, 221, -2392, 28276, 8798, -2865, 773, -91],
        [-4, 529, -3350, 26341, 11950, -3487, 896, -103],
        [-48, 758, -3956, 23973, 15143, -3957, 967, -107],
        [-80, 905, -4235, 21254, 18278, -4222, 972, -99],
        [-99, 972, -4222, 18278, 21254, -4235, 905, -80],
        [-107, 967, -3957, 15143, 23973, -3956, 758, -48],
        [-103, 896, -3487, 11950, 26341, -3350, 529, -4],
        [-91, 773, -2865, 8798, 28276, -2392, 221, 52],
        [-71, 611, -2143, 5784, 29704, -1070, -159, 117],
        [-46, 425, -1375, 2996, 30567, 617, -600, 189]
    ],
    dtype=np.float32
) / 2**15


hq_2x_up_c_even = [x / 2**16 for x in [1746, 14986, 39083 - 65536]]
hq_2x_up_c_odd  = [x / 2**16 for x in [6854, 25769, 55542 - 65536]]


def get_impz(coeffs, n):
    s = 3*[0]
    y = np.zeros(n)
    x = 1

    for i in range(n):
        Y = x - s[0]
        X = Y * coeffs[0]
        tmp1 = s[0] + X
        s[0] = x + X

        Y = tmp1 - s[1]
        X = Y * coeffs[1]
        tmp2 = s[1] + X
        s[1] = tmp1 + X

        Y = tmp2 - s[2]
        X = Y * (1 + coeffs[2])
        tmp3 = s[2] + X
        s[2] = tmp2 + X

        y[i] = tmp3
        x = 0

    return y



class SilkUpsampler(nn.Module):
    SUPPORTED_TARGET_RATES = {24000, 48000}
    SUPPORTED_SOURCE_RATES = {16000}
    def __init__(self,
                 fs_in=16000,
                 fs_out=48000):

        super().__init__()
        self.fs_in = fs_in
        self.fs_out = fs_out

        if fs_in not in self.SUPPORTED_SOURCE_RATES:
            raise ValueError(f'SilkUpsampler currently only supports upsampling from {self.SUPPORTED_SOURCE_RATES} Hz')


        if fs_out not in self.SUPPORTED_TARGET_RATES:
            raise ValueError(f'SilkUpsampler currently only supports upsampling to {self.SUPPORTED_TARGET_RATES} Hz')


        # hq 2x upsampler as FIR approximation
        hq_2x_up_even = get_impz(hq_2x_up_c_even, 128)[::-1].copy()
        hq_2x_up_odd  = get_impz(hq_2x_up_c_odd , 128)[::-1].copy()

        self.hq_2x_up_even = nn.Parameter(torch.from_numpy(hq_2x_up_even).float().view(1, 1, -1), requires_grad=False)
        self.hq_2x_up_odd  = nn.Parameter(torch.from_numpy(hq_2x_up_odd ).float().view(1, 1, -1), requires_grad=False)
        self.hq_2x_up_padding = [127, 0]

        # interpolation filters
        frac_01_24 = frac_fir[0]
        frac_17_24 = frac_fir[8]
        frac_09_24 = frac_fir[4]

        self.frac_01_24 = nn.Parameter(torch.from_numpy(frac_01_24).view(1, 1, -1), requires_grad=False)
        self.frac_17_24 = nn.Parameter(torch.from_numpy(frac_17_24).view(1, 1, -1), requires_grad=False)
        self.frac_09_24 = nn.Parameter(torch.from_numpy(frac_09_24).view(1, 1, -1), requires_grad=False)

        self.stride = 1 if fs_out == 48000 else 2

    def hq_2x_up(self, x):

        num_channels = x.size(1)

        weight_even = torch.repeat_interleave(self.hq_2x_up_even, num_channels, 0)
        weight_odd  = torch.repeat_interleave(self.hq_2x_up_odd , num_channels, 0)

        x_pad  = F.pad(x, self.hq_2x_up_padding)
        y_even = F.conv1d(x_pad, weight_even, groups=num_channels)
        y_odd  = F.conv1d(x_pad, weight_odd , groups=num_channels)

        y = torch.cat((y_even.unsqueeze(-1), y_odd.unsqueeze(-1)), dim=-1).flatten(2)

        return y

    def interpolate_3_2(self, x):

        num_channels = x.size(1)

        weight_01_24 = torch.repeat_interleave(self.frac_01_24, num_channels, 0)
        weight_17_24 = torch.repeat_interleave(self.frac_17_24, num_channels, 0)
        weight_09_24 = torch.repeat_interleave(self.frac_09_24, num_channels, 0)

        x_pad = F.pad(x, [8, 0])
        y_01_24     = F.conv1d(x_pad, weight_01_24, stride=2, groups=num_channels)
        y_17_24     = F.conv1d(x_pad, weight_17_24, stride=2, groups=num_channels)
        y_09_24_sh1 = F.conv1d(torch.roll(x_pad, -1, -1), weight_09_24, stride=2, groups=num_channels)


        y = torch.cat(
            (y_01_24.unsqueeze(-1), y_17_24.unsqueeze(-1), y_09_24_sh1.unsqueeze(-1)),
            dim=-1).flatten(2)

        return y[..., :-3]

    def forward(self, x):

        y_2x = self.hq_2x_up(x)
        y_3x = self.interpolate_3_2(y_2x)

        return y_3x[:, :, ::self.stride]

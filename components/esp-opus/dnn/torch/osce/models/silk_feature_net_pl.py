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
import sys
sys.path.append('../dnntools')
import numbers


import torch
from torch import nn
import torch.nn.functional as F
from torch.nn.utils import weight_norm

from utils.complexity import _conv1d_flop_count

from dnntools.quantization.softquant import soft_quant
from dnntools.sparsification import mark_for_sparsification

class SilkFeatureNetPL(nn.Module):
    """ feature net with partial lookahead """
    def __init__(self,
                 feature_dim=47,
                 num_channels=256,
                 hidden_feature_dim=64,
                 softquant=False,
                 sparsify=True,
                 sparsification_density=0.5,
                 apply_weight_norm=False):

        super(SilkFeatureNetPL, self).__init__()

        if isinstance(sparsification_density, numbers.Number):
            sparsification_density = 4 * [sparsification_density]

        self.feature_dim = feature_dim
        self.num_channels = num_channels
        self.hidden_feature_dim = hidden_feature_dim

        norm = weight_norm if apply_weight_norm else lambda x, name=None: x

        self.conv1 = norm(nn.Conv1d(feature_dim, self.hidden_feature_dim, 1))
        self.conv2 = norm(nn.Conv1d(4 * self.hidden_feature_dim, num_channels, 2))
        self.tconv = norm(nn.ConvTranspose1d(num_channels, num_channels, 4, 4))
        self.gru   = norm(norm(nn.GRU(num_channels, num_channels, batch_first=True), name='weight_hh_l0'), name='weight_ih_l0')

        if softquant:
            self.conv2 = soft_quant(self.conv2)
            self.tconv = soft_quant(self.tconv)
            self.gru = soft_quant(self.gru, names=['weight_hh_l0', 'weight_ih_l0'])


        if sparsify:
            mark_for_sparsification(self.conv2, (sparsification_density[0], [8, 4]))
            mark_for_sparsification(self.tconv, (sparsification_density[1], [8, 4]))
            mark_for_sparsification(
                self.gru,
                {
                    'W_ir' : (sparsification_density[2], [8, 4], False),
                    'W_iz' : (sparsification_density[2], [8, 4], False),
                    'W_in' : (sparsification_density[2], [8, 4], False),
                    'W_hr' : (sparsification_density[3], [8, 4], True),
                    'W_hz' : (sparsification_density[3], [8, 4], True),
                    'W_hn' : (sparsification_density[3], [8, 4], True),
                }
            )


    def flop_count(self, rate=200):
        count = 0
        for conv in self.conv1, self.conv2, self.tconv:
            count += _conv1d_flop_count(conv, rate)

        count += 2 * (3 * self.gru.input_size * self.gru.hidden_size + 3 * self.gru.hidden_size * self.gru.hidden_size) * rate

        return count


    def forward(self, features, state=None):
        """ features shape: (batch_size, num_frames, feature_dim) """

        batch_size = features.size(0)
        num_frames = features.size(1)

        if state is None:
            state = torch.zeros((1, batch_size, self.num_channels), device=features.device)

        features = features.permute(0, 2, 1)
        # dimensionality reduction
        c = torch.tanh(self.conv1(features))

        # frame accumulation
        c = c.permute(0, 2, 1)
        c = c.reshape(batch_size, num_frames // 4, -1).permute(0, 2, 1)
        c = torch.tanh(self.conv2(F.pad(c, [1, 0])))

        # upsampling
        c = torch.tanh(self.tconv(c))
        c = c.permute(0, 2, 1)

        c, _ = self.gru(c, state)

        return c
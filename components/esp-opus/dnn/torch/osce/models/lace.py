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

import numpy as np

from utils.layers.limited_adaptive_comb1d import LimitedAdaptiveComb1d
from utils.layers.limited_adaptive_conv1d import LimitedAdaptiveConv1d

from models.nns_base import NNSBase
from models.silk_feature_net_pl import SilkFeatureNetPL
from models.silk_feature_net import SilkFeatureNet
from .scale_embedding import ScaleEmbedding

import sys
sys.path.append('../dnntools')

from dnntools.sparsification import create_sparsifier


class LACE(NNSBase):
    """ Linear-Adaptive Coding Enhancer """
    FRAME_SIZE=80

    def __init__(self,
                 num_features=47,
                 pitch_embedding_dim=64,
                 cond_dim=256,
                 pitch_max=257,
                 kernel_size=15,
                 preemph=0.85,
                 skip=91,
                 comb_gain_limit_db=-6,
                 global_gain_limits_db=[-6, 6],
                 conv_gain_limits_db=[-6, 6],
                 numbits_range=[50, 650],
                 numbits_embedding_dim=8,
                 hidden_feature_dim=64,
                 partial_lookahead=True,
                 norm_p=2,
                 softquant=False,
                 sparsify=False,
                 sparsification_schedule=[10000, 30000, 100],
                 sparsification_density=0.5,
                 apply_weight_norm=False):

        super().__init__(skip=skip, preemph=preemph)


        self.num_features           = num_features
        self.cond_dim               = cond_dim
        self.pitch_max              = pitch_max
        self.pitch_embedding_dim    = pitch_embedding_dim
        self.kernel_size            = kernel_size
        self.preemph                = preemph
        self.skip                   = skip
        self.numbits_range          = numbits_range
        self.numbits_embedding_dim  = numbits_embedding_dim
        self.hidden_feature_dim     = hidden_feature_dim
        self.partial_lookahead      = partial_lookahead

        # pitch embedding
        self.pitch_embedding = nn.Embedding(pitch_max + 1, pitch_embedding_dim)

        # numbits embedding
        self.numbits_embedding = ScaleEmbedding(numbits_embedding_dim, *numbits_range, logscale=True)

        # feature net
        if partial_lookahead:
            self.feature_net = SilkFeatureNetPL(num_features + pitch_embedding_dim + 2 * numbits_embedding_dim, cond_dim, hidden_feature_dim, softquant=softquant, sparsify=sparsify, sparsification_density=sparsification_density, apply_weight_norm=apply_weight_norm)
        else:
            self.feature_net = SilkFeatureNet(num_features + pitch_embedding_dim + 2 * numbits_embedding_dim, cond_dim)

        # comb filters
        left_pad = self.kernel_size // 2
        right_pad = self.kernel_size - 1 - left_pad
        self.cf1 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, use_bias=False, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.cf2 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, use_bias=False, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)

        # spectral shaping
        self.af1 = LimitedAdaptiveConv1d(1, 1, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)

        if sparsify:
            self.sparsifier = create_sparsifier(self, *sparsification_schedule)

    def flop_count(self, rate=16000, verbose=False):

        frame_rate = rate / self.FRAME_SIZE

        # feature net
        feature_net_flops = self.feature_net.flop_count(frame_rate)
        comb_flops = self.cf1.flop_count(rate) + self.cf2.flop_count(rate)
        af_flops = self.af1.flop_count(rate)

        if verbose:
            print(f"feature net: {feature_net_flops / 1e6} MFLOPS")
            print(f"comb filters: {comb_flops / 1e6} MFLOPS")
            print(f"adaptive conv: {af_flops / 1e6} MFLOPS")

        return feature_net_flops + comb_flops + af_flops

    def forward(self, x, features, periods, numbits, debug=False):

        periods         = periods.squeeze(-1)
        pitch_embedding = self.pitch_embedding(periods)
        numbits_embedding = self.numbits_embedding(numbits).flatten(2)

        full_features = torch.cat((features, pitch_embedding, numbits_embedding), dim=-1)
        cf = self.feature_net(full_features)

        y = self.cf1(x, cf, periods, debug=debug)

        y = self.cf2(y, cf, periods, debug=debug)

        y = self.af1(y, cf, debug=debug)

        return y

    def get_impulse_responses(self, features, periods, numbits):
        """ generates impoulse responses on frame centers (input without batch dimension) """

        num_frames = features.size(0)
        batch_size = 32
        max_len = 2 * (self.pitch_max + self.kernel_size) + 10

        # spread out some pulses
        x = np.zeros((batch_size, 1, num_frames * self.FRAME_SIZE))
        for b in range(batch_size):
            x[b, :, self.FRAME_SIZE // 2 + b * self.FRAME_SIZE :: batch_size * self.FRAME_SIZE] = 1

        # prepare input
        x = torch.from_numpy(x).float().to(features.device)
        features = torch.repeat_interleave(features.unsqueeze(0), batch_size, 0)
        periods = torch.repeat_interleave(periods.unsqueeze(0), batch_size, 0)
        numbits = torch.repeat_interleave(numbits.unsqueeze(0), batch_size, 0)

        # run network
        with torch.no_grad():
            periods         = periods.squeeze(-1)
            pitch_embedding = self.pitch_embedding(periods)
            numbits_embedding = self.numbits_embedding(numbits).flatten(2)
            full_features = torch.cat((features, pitch_embedding, numbits_embedding), dim=-1)
            cf = self.feature_net(full_features)
            y = self.cf1(x, cf, periods, debug=False)
            y = self.cf2(y, cf, periods, debug=False)
            y = self.af1(y, cf, debug=False)

        # collect responses
        y = y.detach().squeeze().cpu().numpy()
        cut_frames = (max_len + self.FRAME_SIZE - 1) // self.FRAME_SIZE
        num_responses = num_frames - cut_frames
        responses = np.zeros((num_responses, max_len))

        for i in range(num_responses):
            b = i % batch_size
            start = self.FRAME_SIZE // 2 + i * self.FRAME_SIZE
            stop = start + max_len

            responses[i, :] = y[b, start:stop]

        return responses

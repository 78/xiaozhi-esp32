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

import numbers

import torch
from torch import nn
import torch.nn.functional as F
from torch.nn.utils import weight_norm


import numpy as np

from utils.layers.limited_adaptive_comb1d import LimitedAdaptiveComb1d
from utils.layers.limited_adaptive_conv1d import LimitedAdaptiveConv1d
from utils.layers.td_shaper import TDShaper
from utils.complexity import _conv1d_flop_count

from models.nns_base import NNSBase
from models.silk_feature_net_pl import SilkFeatureNetPL
from models.silk_feature_net import SilkFeatureNet
from .scale_embedding import ScaleEmbedding

import sys
sys.path.append('../dnntools')
from dnntools.quantization import soft_quant
from dnntools.sparsification import create_sparsifier, mark_for_sparsification

class NoLACE(NNSBase):
    """ Non-Linear Adaptive Coding Enhancer """
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
                 avg_pool_k=4,
                 pool_after=False,
                 softquant=False,
                 sparsify=False,
                 sparsification_schedule=[100, 1000, 100],
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

        if isinstance(sparsification_density, numbers.Number):
            sparsification_density = 10 * [sparsification_density]

        norm = weight_norm if apply_weight_norm else lambda x, name=None: x

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
        self.cf1 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.cf2 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)

        # spectral shaping
        self.af1 = LimitedAdaptiveConv1d(1, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)

        # non-linear transforms
        self.tdshape1 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, pool_after=pool_after, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.tdshape2 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, pool_after=pool_after, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.tdshape3 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, pool_after=pool_after, softquant=softquant, apply_weight_norm=apply_weight_norm)

        # combinators
        self.af2 = LimitedAdaptiveConv1d(2, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.af3 = LimitedAdaptiveConv1d(2, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)
        self.af4 = LimitedAdaptiveConv1d(2, 1, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p, softquant=softquant, apply_weight_norm=apply_weight_norm)

        # feature transforms
        self.post_cf1 = norm(nn.Conv1d(cond_dim, cond_dim, 2))
        self.post_cf2 = norm(nn.Conv1d(cond_dim, cond_dim, 2))
        self.post_af1 = norm(nn.Conv1d(cond_dim, cond_dim, 2))
        self.post_af2 = norm(nn.Conv1d(cond_dim, cond_dim, 2))
        self.post_af3 = norm(nn.Conv1d(cond_dim, cond_dim, 2))

        if softquant:
            self.post_cf1 = soft_quant(self.post_cf1)
            self.post_cf2 = soft_quant(self.post_cf2)
            self.post_af1 = soft_quant(self.post_af1)
            self.post_af2 = soft_quant(self.post_af2)
            self.post_af3 = soft_quant(self.post_af3)


        if sparsify:
            mark_for_sparsification(self.post_cf1, (sparsification_density[4], [8, 4]))
            mark_for_sparsification(self.post_cf2, (sparsification_density[5], [8, 4]))
            mark_for_sparsification(self.post_af1, (sparsification_density[6], [8, 4]))
            mark_for_sparsification(self.post_af2, (sparsification_density[7], [8, 4]))
            mark_for_sparsification(self.post_af3, (sparsification_density[8], [8, 4]))

            self.sparsifier = create_sparsifier(self, *sparsification_schedule)

    def flop_count(self, rate=16000, verbose=False):

        frame_rate = rate / self.FRAME_SIZE

        # feature net
        feature_net_flops = self.feature_net.flop_count(frame_rate)
        comb_flops = self.cf1.flop_count(rate) + self.cf2.flop_count(rate)
        af_flops = self.af1.flop_count(rate) + self.af2.flop_count(rate) + self.af3.flop_count(rate) + self.af4.flop_count(rate)
        shape_flops = self.tdshape1.flop_count(rate) + self.tdshape2.flop_count(rate) + self.tdshape3.flop_count(rate)
        feature_flops = (_conv1d_flop_count(self.post_cf1, frame_rate) + _conv1d_flop_count(self.post_cf2, frame_rate)
                         + _conv1d_flop_count(self.post_af1, frame_rate) + _conv1d_flop_count(self.post_af2, frame_rate) + _conv1d_flop_count(self.post_af3, frame_rate))

        if verbose:
            print(f"feature net: {feature_net_flops / 1e6} MFLOPS")
            print(f"comb filters: {comb_flops / 1e6} MFLOPS")
            print(f"adaptive conv: {af_flops / 1e6} MFLOPS")
            print(f"feature transforms: {feature_flops / 1e6} MFLOPS")

        return feature_net_flops + comb_flops + af_flops + feature_flops + shape_flops

    def feature_transform(self, f, layer):
        f0 = f.permute(0, 2, 1)
        f = F.pad(f0, [1, 0])
        f = torch.tanh(layer(f))
        return f.permute(0, 2, 1)

    def forward(self, x, features, periods, numbits, debug=False):

        periods         = periods.squeeze(-1)
        pitch_embedding = self.pitch_embedding(periods)
        numbits_embedding = self.numbits_embedding(numbits).flatten(2)

        full_features = torch.cat((features, pitch_embedding, numbits_embedding), dim=-1)
        cf = self.feature_net(full_features)

        y = self.cf1(x, cf, periods, debug=debug)
        cf = self.feature_transform(cf, self.post_cf1)

        y = self.cf2(y, cf, periods, debug=debug)
        cf = self.feature_transform(cf, self.post_cf2)

        y = self.af1(y, cf, debug=debug)
        cf = self.feature_transform(cf, self.post_af1)

        y1 = y[:, 0:1, :]
        y2 = self.tdshape1(y[:, 1:2, :], cf)
        y = torch.cat((y1, y2), dim=1)
        y = self.af2(y, cf, debug=debug)
        cf = self.feature_transform(cf, self.post_af2)

        y1 = y[:, 0:1, :]
        y2 = self.tdshape2(y[:, 1:2, :], cf)
        y = torch.cat((y1, y2), dim=1)
        y = self.af3(y, cf, debug=debug)
        cf = self.feature_transform(cf, self.post_af3)

        y1 = y[:, 0:1, :]
        y2 = self.tdshape3(y[:, 1:2, :], cf)
        y = torch.cat((y1, y2), dim=1)
        y = self.af4(y, cf, debug=debug)

        return y

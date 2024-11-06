
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

from utils.layers.silk_upsampler import SilkUpsampler
from utils.layers.limited_adaptive_conv1d import LimitedAdaptiveConv1d
from utils.layers.td_shaper import TDShaper
from utils.layers.deemph import Deemph
from utils.misc import freeze_model

from models.nns_base import NNSBase
from models.silk_feature_net_pl import SilkFeatureNetPL
from models.silk_feature_net import SilkFeatureNet
from .scale_embedding import ScaleEmbedding



class ShapeUp48(NNSBase):
    FRAME_SIZE16k=80

    def __init__(self,
                 num_features=47,
                 pitch_embedding_dim=64,
                 cond_dim=256,
                 pitch_max=257,
                 kernel_size=15,
                 preemph=0.85,
                 skip=288,
                 conv_gain_limits_db=[-6, 6],
                 numbits_range=[50, 650],
                 numbits_embedding_dim=8,
                 hidden_feature_dim=64,
                 partial_lookahead=True,
                 norm_p=2,
                 target_fs=48000,
                 noise_amplitude=0,
                 prenet=None,
                 avg_pool_k=4):

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
        self.frame_size48           = int(self.FRAME_SIZE16k * target_fs / 16000 + .1)
        self.frame_size32           = self.FRAME_SIZE16k * 2
        self.noise_amplitude        = noise_amplitude
        self.prenet                 = prenet

        # freeze prenet if given
        if prenet is not None:
            freeze_model(self.prenet)
            try:
                self.deemph = Deemph(prenet.preemph)
            except:
                print("[warning] prenet model is expected to have preemph attribute")
                self.deemph = Deemph(0)



        # upsampler
        self.upsampler = SilkUpsampler()

        # pitch embedding
        self.pitch_embedding = nn.Embedding(pitch_max + 1, pitch_embedding_dim)

        # numbits embedding
        self.numbits_embedding = ScaleEmbedding(numbits_embedding_dim, *numbits_range, logscale=True)

        # feature net
        if partial_lookahead:
            self.feature_net = SilkFeatureNetPL(num_features + pitch_embedding_dim + 2 * numbits_embedding_dim, cond_dim, hidden_feature_dim)
        else:
            self.feature_net = SilkFeatureNet(num_features + pitch_embedding_dim + 2 * numbits_embedding_dim, cond_dim)

        # non-linear transforms
        self.tdshape1 = TDShaper(cond_dim, frame_size=self.frame_size32, avg_pool_k=avg_pool_k)
        self.tdshape2 = TDShaper(cond_dim, frame_size=self.frame_size48, avg_pool_k=avg_pool_k)

        # spectral shaping
        self.af_noise = LimitedAdaptiveConv1d(1, 1, self.kernel_size, cond_dim, frame_size=self.frame_size32, overlap_size=self.frame_size32//2, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=[-30, 0], norm_p=norm_p)
        self.af1 = LimitedAdaptiveConv1d(1, 2, self.kernel_size, cond_dim, frame_size=self.frame_size32, overlap_size=self.frame_size32//2, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)
        self.af2 = LimitedAdaptiveConv1d(3, 2, self.kernel_size, cond_dim, frame_size=self.frame_size32, overlap_size=self.frame_size32//2, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)
        self.af3 = LimitedAdaptiveConv1d(2, 1, self.kernel_size, cond_dim, frame_size=self.frame_size48, overlap_size=self.frame_size48//2, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)


    def flop_count(self, rate=16000, verbose=False):

        frame_rate = rate / self.FRAME_SIZE16k

        # feature net
        feature_net_flops = self.feature_net.flop_count(frame_rate)
        af_flops = self.af1.flop_count(rate) + self.af2.flop_count(2 * rate) + self.af3.flop_count(3 * rate)

        if verbose:
            print(f"feature net: {feature_net_flops / 1e6} MFLOPS")
            print(f"adaptive conv: {af_flops / 1e6} MFLOPS")

        return feature_net_flops + af_flops

    def forward(self, x, features, periods, numbits, debug=False):

        if self.prenet is not None:
            with torch.no_grad():
                x = self.prenet(x, features, periods, numbits)
                x = self.deemph(x)



        periods         = periods.squeeze(-1)
        pitch_embedding = self.pitch_embedding(periods)
        numbits_embedding = self.numbits_embedding(numbits).flatten(2)

        full_features = torch.cat((features, pitch_embedding, numbits_embedding), dim=-1)
        cf = self.feature_net(full_features)

        y32 = self.upsampler.hq_2x_up(x)

        noise = self.noise_amplitude * torch.randn_like(y32)
        noise = self.af_noise(noise, cf)

        y32 = self.af1(y32, cf, debug=debug)

        y32_1 = y32[:, 0:1, :]
        y32_2 = self.tdshape1(y32[:, 1:2, :], cf)
        y32 = torch.cat((y32_1, y32_2, noise), dim=1)

        y32 = self.af2(y32, cf, debug=debug)

        y48 = self.upsampler.interpolate_3_2(y32)

        y48_1 = y48[:, 0:1, :]
        y48_2 = self.tdshape2(y48[:, 1:2, :], cf)
        y48 = torch.cat((y48_1, y48_2), dim=1)

        y48 = self.af3(y48, cf, debug=debug)

        return y48

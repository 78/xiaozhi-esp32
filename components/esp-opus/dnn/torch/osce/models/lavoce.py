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
from utils.layers.td_shaper import TDShaper
from utils.layers.noise_shaper import NoiseShaper
from utils.complexity import _conv1d_flop_count
from utils.endoscopy import write_data

from models.nns_base import NNSBase
from models.lpcnet_feature_net import LPCNetFeatureNet
from .scale_embedding import ScaleEmbedding

def print_channels(y, prefix="", name="", rate=16000):
    num_channels = y.size(1)
    for i in range(num_channels):
        channel_name = f"{prefix}_c{i:02d}"
        if len(name) > 0: channel_name += "_" + name
        ch =  y[0,i,:].detach().cpu().numpy()
        ch = ((2**14) * ch / np.max(ch)).astype(np.int16)
        write_data(channel_name, ch, rate)



class LaVoce(nn.Module):
    """ Linear-Adaptive VOCodEr """
    FEATURE_FRAME_SIZE=160
    FRAME_SIZE=80

    def __init__(self,
                 num_features=20,
                 pitch_embedding_dim=64,
                 cond_dim=256,
                 pitch_max=300,
                 kernel_size=15,
                 preemph=0.85,
                 comb_gain_limit_db=-6,
                 global_gain_limits_db=[-6, 6],
                 conv_gain_limits_db=[-6, 6],
                 norm_p=2,
                 avg_pool_k=4,
                 pulses=False,
                 innovate1=True,
                 innovate2=False,
                 innovate3=False,
                 ftrans_k=2):

        super().__init__()


        self.num_features           = num_features
        self.cond_dim               = cond_dim
        self.pitch_max              = pitch_max
        self.pitch_embedding_dim    = pitch_embedding_dim
        self.kernel_size            = kernel_size
        self.preemph                = preemph
        self.pulses                 = pulses
        self.ftrans_k               = ftrans_k

        assert self.FEATURE_FRAME_SIZE % self.FRAME_SIZE == 0
        self.upsamp_factor =  self.FEATURE_FRAME_SIZE // self.FRAME_SIZE

        # pitch embedding
        self.pitch_embedding = nn.Embedding(pitch_max + 1, pitch_embedding_dim)

        # feature net
        self.feature_net = LPCNetFeatureNet(num_features + pitch_embedding_dim, cond_dim, self.upsamp_factor)

        # noise shaper
        self.noise_shaper = NoiseShaper(cond_dim, self.FRAME_SIZE)

        # comb filters
        left_pad = self.kernel_size // 2
        right_pad = self.kernel_size - 1 - left_pad
        self.cf1 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, use_bias=False, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p)
        self.cf2 = LimitedAdaptiveComb1d(self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, overlap_size=40, use_bias=False, padding=[left_pad, right_pad], max_lag=pitch_max + 1, gain_limit_db=comb_gain_limit_db, global_gain_limits_db=global_gain_limits_db, norm_p=norm_p)


        self.af_prescale = LimitedAdaptiveConv1d(2, 1, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)
        self.af_mix = LimitedAdaptiveConv1d(2, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)

        # spectral shaping
        self.af1 = LimitedAdaptiveConv1d(1, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)

        # non-linear transforms
        self.tdshape1 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, innovate=innovate1)
        self.tdshape2 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, innovate=innovate2)
        self.tdshape3 = TDShaper(cond_dim, frame_size=self.FRAME_SIZE, avg_pool_k=avg_pool_k, innovate=innovate3)

        # combinators
        self.af2 = LimitedAdaptiveConv1d(2, 2, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)
        self.af3 = LimitedAdaptiveConv1d(2, 1, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)
        self.af4 = LimitedAdaptiveConv1d(2, 1, self.kernel_size, cond_dim, frame_size=self.FRAME_SIZE, use_bias=False, padding=[self.kernel_size - 1, 0], gain_limits_db=conv_gain_limits_db, norm_p=norm_p)

        # feature transforms
        self.post_cf1 = nn.Conv1d(cond_dim, cond_dim, ftrans_k)
        self.post_cf2 = nn.Conv1d(cond_dim, cond_dim, ftrans_k)
        self.post_af1 = nn.Conv1d(cond_dim, cond_dim, ftrans_k)
        self.post_af2 = nn.Conv1d(cond_dim, cond_dim, ftrans_k)
        self.post_af3 = nn.Conv1d(cond_dim, cond_dim, ftrans_k)


    def create_phase_signals(self, periods):

        batch_size = periods.size(0)
        progression = torch.arange(1, self.FRAME_SIZE + 1, dtype=periods.dtype, device=periods.device).view((1, -1))
        progression = torch.repeat_interleave(progression, batch_size, 0)

        phase0 = torch.zeros(batch_size, dtype=periods.dtype, device=periods.device).unsqueeze(-1)
        chunks = []
        for sframe in range(periods.size(1)):
            f = (2.0 * torch.pi / periods[:, sframe]).unsqueeze(-1)

            if self.pulses:
                alpha = torch.cos(f).view(batch_size, 1, 1)
                chunk_sin = torch.sin(f  * progression + phase0).view(batch_size, 1, self.FRAME_SIZE)
                pulse_a = torch.relu(chunk_sin - alpha) / (1 - alpha)
                pulse_b = torch.relu(-chunk_sin - alpha) / (1 - alpha)

                chunk = torch.cat((pulse_a, pulse_b), dim = 1)
            else:
                chunk_sin = torch.sin(f  * progression + phase0).view(batch_size, 1, self.FRAME_SIZE)
                chunk_cos = torch.cos(f  * progression + phase0).view(batch_size, 1, self.FRAME_SIZE)

                chunk = torch.cat((chunk_sin, chunk_cos), dim = 1)

            phase0 = phase0 + self.FRAME_SIZE * f

            chunks.append(chunk)

        phase_signals = torch.cat(chunks, dim=-1)

        return phase_signals

    def flop_count(self, rate=16000, verbose=False):

        frame_rate = rate / self.FRAME_SIZE

        # feature net
        feature_net_flops = self.feature_net.flop_count(frame_rate)
        comb_flops = self.cf1.flop_count(rate) + self.cf2.flop_count(rate)
        af_flops = self.af1.flop_count(rate) + self.af2.flop_count(rate) + self.af3.flop_count(rate) + self.af4.flop_count(rate) + self.af_prescale.flop_count(rate) + self.af_mix.flop_count(rate)
        feature_flops = (_conv1d_flop_count(self.post_cf1, frame_rate) + _conv1d_flop_count(self.post_cf2, frame_rate)
                         + _conv1d_flop_count(self.post_af1, frame_rate) + _conv1d_flop_count(self.post_af2, frame_rate) + _conv1d_flop_count(self.post_af3, frame_rate))

        if verbose:
            print(f"feature net: {feature_net_flops / 1e6} MFLOPS")
            print(f"comb filters: {comb_flops / 1e6} MFLOPS")
            print(f"adaptive conv: {af_flops / 1e6} MFLOPS")
            print(f"feature transforms: {feature_flops / 1e6} MFLOPS")

        return feature_net_flops + comb_flops + af_flops + feature_flops

    def feature_transform(self, f, layer):
        f = f.permute(0, 2, 1)
        f = F.pad(f, [self.ftrans_k - 1, 0])
        f = torch.tanh(layer(f))
        return f.permute(0, 2, 1)

    def forward(self, features, periods, debug=False):

        periods         = periods.squeeze(-1)
        pitch_embedding = self.pitch_embedding(periods)

        full_features = torch.cat((features, pitch_embedding), dim=-1)
        cf = self.feature_net(full_features)

        # upsample periods
        periods = torch.repeat_interleave(periods, self.upsamp_factor, 1)

        # pre-net
        ref_phase = torch.tanh(self.create_phase_signals(periods))
        if debug: print_channels(ref_phase, prefix="lavoce_01", name="pulse")
        x = self.af_prescale(ref_phase, cf)
        noise = self.noise_shaper(cf)
        if debug: print_channels(torch.cat((x, noise), dim=1), prefix="lavoce_02", name="inputs")
        y = self.af_mix(torch.cat((x, noise), dim=1), cf)
        if debug: print_channels(y, prefix="lavoce_03", name="postselect1")

        # temporal shaping + innovating
        y1 = y[:, 0:1, :]
        y2 = self.tdshape1(y[:, 1:2, :], cf)
        if debug: print_channels(y2, prefix="lavoce_04", name="postshape1")
        y = torch.cat((y1, y2), dim=1)
        y = self.af2(y, cf, debug=debug)
        if debug: print_channels(y, prefix="lavoce_05", name="postselect2")
        cf = self.feature_transform(cf, self.post_af2)

        y1 = y[:, 0:1, :]
        y2 = self.tdshape2(y[:, 1:2, :], cf)
        if debug: print_channels(y2, prefix="lavoce_06", name="postshape2")
        y = torch.cat((y1, y2), dim=1)
        y = self.af3(y, cf, debug=debug)
        if debug: print_channels(y, prefix="lavoce_07", name="postmix1")
        cf = self.feature_transform(cf, self.post_af3)

        # spectral shaping
        y = self.cf1(y, cf, periods, debug=debug)
        if debug: print_channels(y, prefix="lavoce_08", name="postcomb1")
        cf = self.feature_transform(cf, self.post_cf1)

        y = self.cf2(y, cf, periods, debug=debug)
        if debug: print_channels(y, prefix="lavoce_09", name="postcomb2")
        cf = self.feature_transform(cf, self.post_cf2)

        y = self.af1(y, cf, debug=debug)
        if debug: print_channels(y, prefix="lavoce_10", name="postselect3")
        cf = self.feature_transform(cf, self.post_af1)

        # final temporal env adjustment
        y1 = y[:, 0:1, :]
        y2 = self.tdshape3(y[:, 1:2, :], cf)
        if debug: print_channels(y2, prefix="lavoce_11", name="postshape3")
        y = torch.cat((y1, y2), dim=1)
        y = self.af4(y, cf, debug=debug)
        if debug: print_channels(y, prefix="lavoce_12", name="postmix2")

        return y

    def process(self, features, periods, debug=False):

        self.eval()
        device = next(iter(self.parameters())).device
        with torch.no_grad():

            # run model
            f = features.unsqueeze(0).to(device)
            p = periods.unsqueeze(0).to(device)

            y = self.forward(f, p, debug=debug).squeeze()

            # deemphasis
            if self.preemph > 0:
                for i in range(len(y) - 1):
                    y[i + 1] += self.preemph * y[i]

            # clip to valid range
            out = torch.clip((2**15) * y, -2**15, 2**15 - 1).short()

        return out
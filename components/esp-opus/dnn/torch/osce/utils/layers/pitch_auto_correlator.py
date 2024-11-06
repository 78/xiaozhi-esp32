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


class PitchAutoCorrelator(nn.Module):
    def __init__(self,
                 frame_size=80,
                 pitch_min=32,
                 pitch_max=300,
                 radius=2):

        super().__init__()

        self.frame_size = frame_size
        self.pitch_min = pitch_min
        self.pitch_max = pitch_max
        self.radius = radius


    def forward(self, x, periods):
        # x of shape (batch_size, channels, num_samples)
        # periods of shape (batch_size, num_frames)

        num_frames = periods.size(1)
        batch_size = periods.size(0)
        num_samples = self.frame_size * num_frames
        channels = x.size(1)

        assert num_samples == x.size(-1)

        range = torch.arange(-self.radius, self.radius + 1, device=x.device)
        idx = torch.arange(self.frame_size * num_frames, device=x.device)
        p_up = torch.repeat_interleave(periods, self.frame_size, 1)
        lookup = idx + self.pitch_max -  p_up
        lookup = lookup.unsqueeze(-1) + range
        lookup = lookup.unsqueeze(1)

        # padding
        x_pad = F.pad(x, [self.pitch_max, 0])
        x_ext = torch.repeat_interleave(x_pad.unsqueeze(-1), 2 * self.radius + 1, -1)

        # framing
        x_select = torch.gather(x_ext, 2, lookup)
        x_frames = x_pad[..., self.pitch_max : ].reshape(batch_size, channels, num_frames, self.frame_size, 1)
        lag_frames = x_select.reshape(batch_size, 1, num_frames, self.frame_size, -1)

        # calculate auto-correlation
        dotp = torch.sum(x_frames * lag_frames, dim=-2)
        frame_nrg = torch.sum(x_frames * x_frames, dim=-2)
        lag_frame_nrg  = torch.sum(lag_frames * lag_frames, dim=-2)

        acorr = dotp / torch.sqrt(frame_nrg * lag_frame_nrg + 1e-9)

        return acorr

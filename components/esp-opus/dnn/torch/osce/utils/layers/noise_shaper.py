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

from utils.complexity import _conv1d_flop_count

class NoiseShaper(nn.Module):

    def __init__(self,
                 feature_dim,
                 frame_size=160
    ):
        """

        Parameters:
        -----------

        feature_dim : int
            dimension of input features

        frame_size : int
            frame size

        """

        super().__init__()

        self.feature_dim    = feature_dim
        self.frame_size     = frame_size

        # feature transform
        self.feature_alpha1 = nn.Conv1d(self.feature_dim, frame_size, 2)
        self.feature_alpha2 = nn.Conv1d(frame_size, frame_size, 2)


    def flop_count(self, rate):

        frame_rate = rate / self.frame_size

        shape_flops = sum([_conv1d_flop_count(x, frame_rate) for x in (self.feature_alpha1, self.feature_alpha2)]) + 11 * frame_rate * self.frame_size

        return shape_flops


    def forward(self, features):
        """ creates temporally shaped noise


        Parameters:
        -----------
        features : torch.tensor
            frame-wise features of shape (batch_size, num_frames, feature_dim)

        """

        batch_size = features.size(0)
        num_frames = features.size(1)
        frame_size = self.frame_size
        num_samples = num_frames * frame_size

        # feature path
        f = F.pad(features.permute(0, 2, 1), [1, 0])
        alpha = F.leaky_relu(self.feature_alpha1(f), 0.2)
        alpha = torch.exp(self.feature_alpha2(F.pad(alpha, [1, 0])))
        alpha = alpha.permute(0, 2, 1)

        # signal generation
        y = torch.randn((batch_size, num_frames, frame_size), dtype=features.dtype, device=features.device)
        y = alpha * y

        return y.reshape(batch_size, 1, num_samples)

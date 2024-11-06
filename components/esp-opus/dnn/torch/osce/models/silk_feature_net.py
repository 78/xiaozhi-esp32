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

class SilkFeatureNet(nn.Module):

    def __init__(self,
                 feature_dim=47,
                 num_channels=256,
                 lookahead=False):

        super(SilkFeatureNet, self).__init__()

        self.feature_dim = feature_dim
        self.num_channels = num_channels
        self.lookahead = lookahead

        self.conv1 = nn.Conv1d(feature_dim, num_channels, 3)
        self.conv2 = nn.Conv1d(num_channels, num_channels, 3, dilation=2)

        self.gru = nn.GRU(num_channels, num_channels, batch_first=True)

    def flop_count(self, rate=200):
        count = 0
        for conv in self.conv1, self.conv2:
            count += _conv1d_flop_count(conv, rate)

        count += 2 * (3 * self.gru.input_size * self.gru.hidden_size + 3 * self.gru.hidden_size * self.gru.hidden_size) * rate

        return count


    def forward(self, features, state=None):
        """ features shape: (batch_size, num_frames, feature_dim) """

        batch_size = features.size(0)

        if state is None:
            state = torch.zeros((1, batch_size, self.num_channels), device=features.device)


        features = features.permute(0, 2, 1)
        if self.lookahead:
            c = torch.tanh(self.conv1(F.pad(features, [1, 1])))
            c = torch.tanh(self.conv2(F.pad(c, [2, 2])))
        else:
            c = torch.tanh(self.conv1(F.pad(features, [2, 0])))
            c = torch.tanh(self.conv2(F.pad(c, [4, 0])))

        c = c.permute(0, 2, 1)

        c, _ = self.gru(c, state)

        return c
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

""" module implementing PCM embeddings for LPCNet """

import math as m

import torch
from torch import nn


class PCMEmbedding(nn.Module):
    def __init__(self, embed_dim=128, num_levels=256):
        super(PCMEmbedding, self).__init__()

        self.embed_dim  = embed_dim
        self.num_levels = num_levels

        self.embedding = nn.Embedding(self.num_levels, self.num_dim)

        # initialize
        with torch.no_grad():
            num_rows, num_cols = self.num_levels, self.embed_dim
            a = m.sqrt(12) * (torch.rand(num_rows, num_cols) - 0.5)
            for i in range(num_rows):
                a[i, :] += m.sqrt(12) * (i -  num_rows / 2)
            self.embedding.weight[:, :] = 0.1 * a

    def forward(self, x):
        return self.embeddint(x)


class DifferentiablePCMEmbedding(PCMEmbedding):
    def __init__(self, embed_dim, num_levels=256):
        super(DifferentiablePCMEmbedding, self).__init__(embed_dim, num_levels)

    def forward(self, x):
        x_int = (x - torch.floor(x)).detach().long()
        x_frac = x - x_int
        x_next = torch.minimum(x_int + 1, self.num_levels)

        embed_0 = self.embedding(x_int)
        embed_1 = self.embedding(x_next)

        return (1 - x_frac) * embed_0 + x_frac * embed_1

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

import math as m
import torch
from torch import nn


class ScaleEmbedding(nn.Module):
    def __init__(self,
                 dim,
                 min_val,
                 max_val,
                 logscale=False):

        super().__init__()

        if min_val >= max_val:
            raise ValueError('min_val must be smaller than max_val')

        if min_val <= 0 and logscale:
            raise ValueError('min_val must be positive when logscale is true')

        self.dim = dim
        self.logscale = logscale
        self.min_val = min_val
        self.max_val = max_val

        if logscale:
            self.min_val = m.log(self.min_val)
            self.max_val = m.log(self.max_val)


        self.offset = (self.min_val + self.max_val) / 2
        self.scale_factors = nn.Parameter(
            torch.arange(1, dim+1, dtype=torch.float32) * torch.pi / (self.max_val - self.min_val)
        )

    def forward(self, x):
        if self.logscale: x = torch.log(x)
        x = torch.clip(x, self.min_val, self.max_val) - self.offset
        return torch.sin(x.unsqueeze(-1) * self.scale_factors - 0.5)

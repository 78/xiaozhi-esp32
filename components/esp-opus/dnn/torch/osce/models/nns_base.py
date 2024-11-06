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

class NNSBase(nn.Module):

    def __init__(self, skip=91, preemph=0.85):
        super().__init__()

        self.skip = skip
        self.preemph = preemph

    def process(self, sig, features, periods, numbits, debug=False):

        self.eval()
        has_numbits = 'numbits' in self.forward.__code__.co_varnames
        device = next(iter(self.parameters())).device
        with torch.no_grad():

            # run model
            x = sig.view(1, 1, -1).to(device)
            f = features.unsqueeze(0).to(device)
            p = periods.unsqueeze(0).to(device)
            n = numbits.unsqueeze(0).to(device)

            if has_numbits:
                y = self.forward(x, f, p, n, debug=debug).squeeze()
            else:
                y = self.forward(x, f, p, debug=debug).squeeze()

            # deemphasis
            if self.preemph > 0:
                for i in range(len(y) - 1):
                    y[i + 1] += self.preemph * y[i]

            # delay compensation
            y = torch.cat((y[self.skip:], torch.zeros(self.skip, dtype=y.dtype, device=y.device)))
            out = torch.clip((2**15) * y, -2**15, 2**15 - 1).short()

        return out
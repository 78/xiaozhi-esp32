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



def ulaw2lin(u):
    scale_1 = 32768.0 / 255.0
    u = u - 128
    s = torch.sign(u)
    u = torch.abs(u)
    return s * scale_1 * (torch.exp(u / 128. * m.log(256)) - 1)


def lin2ulawq(x):
    scale = 255.0 / 32768.0
    s = torch.sign(x)
    x = torch.abs(x)
    u = s * (128 * torch.log(1 + scale * x) / m.log(256))
    u = torch.clip(128 + torch.round(u), 0, 255)
    return u

def lin2ulaw(x):
    scale = 255.0 / 32768.0
    s = torch.sign(x)
    x = torch.abs(x)
    u = s * (128 * torch.log(1 + scale * x) / torch.log(256))
    u = torch.clip(128 + u, 0, 255)
    return u
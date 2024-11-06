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


def find(a, v):
    try:
        idx = a.index(v)
    except:
        idx = -1
    return idx

def interleave_tensors(tensors, dim=-2):
    """ interleave list of tensors along sequence dimension """

    x = torch.cat([x.unsqueeze(dim) for x in tensors], dim=dim)
    x = torch.flatten(x, dim - 1, dim)

    return x

def _interleave(x, pcm_levels=256):

    repeats = pcm_levels // (2*x.size(-1))
    x = x.unsqueeze(-1)
    p = torch.flatten(torch.repeat_interleave(torch.cat((x, 1 - x), dim=-1), repeats, dim=-1), -2)

    return p

def get_pdf_from_tree(x):
    pcm_levels = x.size(-1)

    p = _interleave(x[..., 1:2])
    n = 4
    while n <= pcm_levels:
        p = p * _interleave(x[..., n//2:n])
        n *= 2

    return p
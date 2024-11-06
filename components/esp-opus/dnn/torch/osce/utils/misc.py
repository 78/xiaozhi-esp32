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
from torch.nn.utils import remove_weight_norm

def count_parameters(model, verbose=False):
    total = 0
    for name, p in model.named_parameters():
        count = torch.ones_like(p).sum().item()

        if verbose:
            print(f"{name}: {count} parameters")

        total += count

    return total

def count_nonzero_parameters(model, verbose=False):
    total = 0
    for name, p in model.named_parameters():
        count = torch.count_nonzero(p).item()

        if verbose:
            print(f"{name}: {count} non-zero parameters")

        total += count

    return total
def retain_grads(module):
    for p in module.parameters():
        if p.requires_grad:
            p.retain_grad()

def get_grad_norm(module, p=2):
    norm = 0
    for param in module.parameters():
        if param.requires_grad:
            norm = norm + (torch.abs(param.grad) ** p).sum()

    return norm ** (1/p)

def create_weights(s_real, s_gen, alpha):
    weights = []
    with torch.no_grad():
        for sr, sg in zip(s_real, s_gen):
            weight = torch.exp(alpha * (sr[-1] - sg[-1]))
            weights.append(weight)

    return weights


def _get_candidates(module: torch.nn.Module):
    candidates = []
    for key in module.__dict__.keys():
        if hasattr(module, key + '_v'):
            candidates.append(key)
    return candidates

def remove_all_weight_norm(model : torch.nn.Module, verbose=False):
    for name, m in model.named_modules():
        candidates = _get_candidates(m)

        for candidate in candidates:
            try:
                remove_weight_norm(m, name=candidate)
                if verbose: print(f'removed weight norm on weight {name}.{candidate}')
            except:
                pass

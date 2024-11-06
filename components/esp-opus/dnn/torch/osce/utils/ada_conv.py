"""
/* Copyright (c) 2023 Amazon
   Written by Jean-Marc Valin */
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

# x is (batch, nb_in_channels, nb_frames*frame_size)
# kernels is (batch, nb_out_channels, nb_in_channels, nb_frames, coeffs)
def adaconv_kernel(x, kernels, half_window, fft_size=256):
    device=x.device
    overlap_size=half_window.size(-1)
    nb_frames=kernels.size(3)
    nb_batches=kernels.size(0)
    nb_out_channels=kernels.size(1)
    nb_in_channels=kernels.size(2)
    kernel_size = kernels.size(-1)
    x = x.reshape(nb_batches, 1, nb_in_channels, nb_frames, -1)
    frame_size = x.size(-1)
    # build window: [zeros, rising window, ones, falling window, zeros]
    window = torch.cat(
        [
            torch.zeros(frame_size, device=device),
            half_window,
            torch.ones(frame_size - overlap_size, device=device),
            1 - half_window,
            torch.zeros(fft_size - 2 * frame_size - overlap_size,device=device)
        ])
    x_prev = torch.cat([torch.zeros_like(x[:, :, :, :1, :]), x[:, :, :, :-1, :]], dim=-2)
    x_next = torch.cat([x[:, :, :, 1:, :overlap_size], torch.zeros_like(x[:, :, :, -1:, :overlap_size])], dim=-2)
    x_padded = torch.cat([x_prev, x, x_next, torch.zeros(nb_batches, 1, nb_in_channels, nb_frames, fft_size - 2 * frame_size - overlap_size, device=device)], -1)
    k_padded = torch.cat([torch.flip(kernels, [-1]), torch.zeros(nb_batches, nb_out_channels, nb_in_channels, nb_frames, fft_size-kernel_size, device=device)], dim=-1)

    # compute convolution
    X = torch.fft.rfft(x_padded, dim=-1)
    K = torch.fft.rfft(k_padded, dim=-1)

    out = torch.fft.irfft(X * K, dim=-1)
    # combine in channels
    out = torch.sum(out, dim=2)
    # apply the cross-fading
    out = window.reshape(1, 1, 1, -1)*out
    crossfaded = out[:,:,:,frame_size:2*frame_size] + torch.cat([torch.zeros(nb_batches, nb_out_channels, 1, frame_size, device=device), out[:, :, :-1, 2*frame_size:3*frame_size]], dim=-2)

    return crossfaded.reshape(nb_batches, nb_out_channels, -1)
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
import numpy as np
import scipy
import scipy.fftpack
import torch

def erb(f):
    return 24.7 * (4.37 * f + 1)

def inv_erb(e):
    return (e / 24.7 - 1) / 4.37

def bark(f):
    return 6 * m.asinh(f/600)

def inv_bark(b):
    return 600 * m.sinh(b / 6)


scale_dict = {
    'bark': [bark, inv_bark],
    'erb': [erb, inv_erb]
}

def gen_filterbank(N, Fs=16000, keep_size=False):
    in_freq = (np.arange(N+1, dtype='float32')/N*Fs/2)[None,:]
    M = N + 1 if keep_size else N
    out_freq = (np.arange(M, dtype='float32')/N*Fs/2)[:,None]
    #ERB from B.C.J Moore, An Introduction to the Psychology of Hearing, 5th Ed., page 73.
    ERB_N = 24.7 + .108*in_freq
    delta = np.abs(in_freq-out_freq)/ERB_N
    center = (delta<.5).astype('float32')
    R = -12*center*delta**2 + (1-center)*(3-12*delta)
    RE = 10.**(R/10.)
    norm = np.sum(RE, axis=1)
    RE = RE/norm[:, np.newaxis]
    return torch.from_numpy(RE)

def create_filter_bank(num_bands, n_fft=320, fs=16000, scale='bark', round_center_bins=False, return_upper=False, normalize=False):

    f0 = 0
    num_bins = n_fft // 2 + 1
    f1 = fs / n_fft * (num_bins - 1)
    fstep = fs / n_fft

    if scale == 'opus':
        bins_5ms = [0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 34, 40]
        fac = 1000 * n_fft / fs / 5
        if num_bands != 18:
            print("warning: requested Opus filter bank with num_bands != 18. Adjusting num_bands.")
            num_bands = 18
        center_bins = np.array([fac * bin for bin in bins_5ms])
    else:
        to_scale, from_scale = scale_dict[scale]

        s0 = to_scale(f0)
        s1 = to_scale(f1)

        center_freqs = np.array([f0] + [from_scale(s0 + i * (s1 - s0) / (num_bands)) for i in range(1, num_bands - 1)] + [f1])
        center_bins  = (center_freqs - f0) / fstep

    if round_center_bins:
        center_bins = np.round(center_bins)

    filter_bank = np.zeros((num_bands, num_bins))

    band = 0
    for bin in range(num_bins):
        # update band index
        if bin > center_bins[band + 1]:
            band += 1

        # calculate filter coefficients
        frac = (center_bins[band + 1] - bin) / (center_bins[band + 1] - center_bins[band])
        filter_bank[band][bin]     = frac
        filter_bank[band + 1][bin] = 1 - frac

    if return_upper:
        extend = n_fft - num_bins
        filter_bank = np.concatenate((filter_bank, np.fliplr(filter_bank[:, 1:extend+1])), axis=1)

    if normalize:
        filter_bank = filter_bank / np.sum(filter_bank, axis=1).reshape(-1, 1)

    return filter_bank


def compressed_log_spec(pspec):

    lpspec = np.zeros_like(pspec)
    num_bands = pspec.shape[-1]

    log_max = -2
    follow = -2

    for i in range(num_bands):
        tmp = np.log10(pspec[i] + 1e-9)
        tmp = max(log_max, max(follow - 2.5, tmp))
        lpspec[i] = tmp
        log_max = max(log_max, tmp)
        follow = max(follow - 2.5, tmp)

    return lpspec

def log_spectrum_from_lpc(a, fb=None, n_fft=320, eps=1e-9, gamma=1, compress=False, power=1):
    """ calculates cepstrum from SILK lpcs """
    order = a.shape[-1]
    assert order + 1 < n_fft

    a = a * (gamma ** (1 + np.arange(order))).astype(np.float32)

    x = np.zeros((*a.shape[:-1], n_fft ))
    x[..., 0] = 1
    x[..., 1:1 + order] = -a

    X = np.fft.fft(x, axis=-1)
    X = np.abs(X[..., :n_fft//2 + 1]) ** power

    S = 1 / (X + eps)

    if fb is None:
        Sf = S
    else:
        Sf = np.matmul(S, fb.T)

    if compress:
        Sf = np.apply_along_axis(compressed_log_spec, -1, Sf)
    else:
        Sf = np.log(Sf + eps)

    return Sf

def cepstrum_from_lpc(a, fb=None, n_fft=320, eps=1e-9, gamma=1, compress=False):
    """ calculates cepstrum from SILK lpcs """

    Sf = log_spectrum_from_lpc(a, fb, n_fft, eps, gamma, compress)

    cepstrum = scipy.fftpack.dct(Sf, 2, norm='ortho')

    return cepstrum



def log_spectrum(x, frame_size, fb=None, window=None, power=1):
    """ calculate cepstrum on 50% overlapping frames """

    assert(2*len(x)) % frame_size == 0
    assert frame_size % 2 == 0

    n = len(x)
    num_even = n // frame_size
    num_odd  = (n - frame_size // 2) // frame_size
    num_bins = frame_size // 2 + 1

    x_even = x[:num_even * frame_size].reshape(-1, frame_size)
    x_odd  = x[frame_size//2 : frame_size//2 + frame_size *  num_odd].reshape(-1, frame_size)

    x_unfold = np.empty((x_even.size + x_odd.size), dtype=x.dtype).reshape((-1, frame_size))
    x_unfold[::2, :] = x_even
    x_unfold[1::2, :] = x_odd

    if window is not None:
        x_unfold *= window.reshape(1, -1)

    X = np.abs(np.fft.fft(x_unfold, n=frame_size, axis=-1))[:, :num_bins] ** power

    if fb is not None:
        X = np.matmul(X, fb.T)


    return np.log(X + 1e-9)


def cepstrum(x, frame_size, fb=None, window=None):
    """ calculate cepstrum on 50% overlapping frames """

    X = log_spectrum(x, frame_size, fb, window)

    cepstrum = scipy.fftpack.dct(X, 2, norm='ortho')

    return cepstrum
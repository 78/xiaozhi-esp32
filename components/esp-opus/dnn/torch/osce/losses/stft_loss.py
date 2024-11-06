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

"""STFT-based Loss modules."""

import torch
import torch.nn.functional as F
from torch import nn
import numpy as np
import torchaudio


def get_window(win_name, win_length, *args, **kwargs):
    window_dict = {
        'bartlett_window'   : torch.bartlett_window,
        'blackman_window'   : torch.blackman_window,
        'hamming_window'    : torch.hamming_window,
        'hann_window'       : torch.hann_window,
        'kaiser_window'     : torch.kaiser_window
    }

    if not win_name in window_dict:
        raise ValueError()

    return window_dict[win_name](win_length, *args, **kwargs)


def stft(x, fft_size, hop_size, win_length, window):
    """Perform STFT and convert to magnitude spectrogram.
    Args:
        x (Tensor): Input signal tensor (B, T).
        fft_size (int): FFT size.
        hop_size (int): Hop size.
        win_length (int): Window length.
        window (str): Window function type.
    Returns:
        Tensor: Magnitude spectrogram (B, #frames, fft_size // 2 + 1).
    """

    win = get_window(window, win_length).to(x.device)
    x_stft = torch.stft(x, fft_size, hop_size, win_length, win, return_complex=True)


    return torch.clamp(torch.abs(x_stft), min=1e-7)

def spectral_convergence_loss(Y_true, Y_pred):
    dims=list(range(1, len(Y_pred.shape)))
    return torch.mean(torch.norm(torch.abs(Y_true) - torch.abs(Y_pred), p="fro", dim=dims) / (torch.norm(Y_pred, p="fro", dim=dims) + 1e-6))


def log_magnitude_loss(Y_true, Y_pred):
    Y_true_log_abs = torch.log(torch.abs(Y_true) + 1e-15)
    Y_pred_log_abs = torch.log(torch.abs(Y_pred) + 1e-15)

    return torch.mean(torch.abs(Y_true_log_abs - Y_pred_log_abs))

def spectral_xcorr_loss(Y_true, Y_pred):
    Y_true = Y_true.abs()
    Y_pred = Y_pred.abs()
    dims=list(range(1, len(Y_pred.shape)))
    xcorr = torch.sum(Y_true * Y_pred, dim=dims) / torch.sqrt(torch.sum(Y_true ** 2, dim=dims) * torch.sum(Y_pred ** 2, dim=dims) + 1e-9)

    return 1 - xcorr.mean()



class MRLogMelLoss(nn.Module):
    def __init__(self,
                 fft_sizes=[512, 256, 128, 64],
                 overlap=0.5,
                 fs=16000,
                 n_mels=18
                 ):

        self.fft_sizes  = fft_sizes
        self.overlap    = overlap
        self.fs         = fs
        self.n_mels     = n_mels

        super().__init__()

        self.mel_specs = []
        for fft_size in fft_sizes:
            hop_size = int(round(fft_size * (1 - self.overlap)))

            n_mels = self.n_mels
            if fft_size < 128:
                n_mels //= 2

            self.mel_specs.append(torchaudio.transforms.MelSpectrogram(fs, fft_size, hop_length=hop_size, n_mels=n_mels))

        for i, mel_spec in enumerate(self.mel_specs):
            self.add_module(f'mel_spec_{i+1}', mel_spec)

    def forward(self, y_true, y_pred):

        loss = torch.zeros(1, device=y_true.device)

        for mel_spec in self.mel_specs:
            Y_true = mel_spec(y_true)
            Y_pred = mel_spec(y_pred)
            loss = loss + log_magnitude_loss(Y_true, Y_pred)

        loss = loss / len(self.mel_specs)

        return loss

def create_weight_matrix(num_bins, bins_per_band=10):
    m = torch.zeros((num_bins, num_bins), dtype=torch.float32)

    r0 = bins_per_band // 2
    r1 = bins_per_band - r0

    for i in range(num_bins):
        i0 = max(i - r0, 0)
        j0 = min(i + r1, num_bins)

        m[i, i0: j0] += 1

        if i < r0:
            m[i, :r0 - i] += 1

        if i > num_bins - r1:
            m[i, num_bins - r1 - i:] += 1

    return m / bins_per_band

def weighted_spectral_convergence(Y_true, Y_pred, w):

    # calculate sfm based weights
    logY = torch.log(torch.abs(Y_true) + 1e-9)
    Y = torch.abs(Y_true)

    avg_logY = torch.matmul(logY.transpose(1, 2), w)
    avg_Y = torch.matmul(Y.transpose(1, 2), w)

    sfm = torch.exp(avg_logY) / (avg_Y + 1e-9)

    weight = (torch.relu(1 - sfm) ** .5).transpose(1, 2)

    loss = torch.mean(
        torch.mean(weight * torch.abs(torch.abs(Y_true) - torch.abs(Y_pred)), dim=[1, 2])
        / (torch.mean( weight * torch.abs(Y_true), dim=[1, 2]) + 1e-9)
    )

    return loss

def gen_filterbank(N, Fs=16000):
    in_freq = (np.arange(N+1, dtype='float32')/N*Fs/2)[None,:]
    out_freq = (np.arange(N, dtype='float32')/N*Fs/2)[:,None]
    #ERB from B.C.J Moore, An Introduction to the Psychology of Hearing, 5th Ed., page 73.
    ERB_N = 24.7 + .108*in_freq
    delta = np.abs(in_freq-out_freq)/ERB_N
    center = (delta<.5).astype('float32')
    R = -12*center*delta**2 + (1-center)*(3-12*delta)
    RE = 10.**(R/10.)
    norm = np.sum(RE, axis=1)
    RE = RE/norm[:, np.newaxis]
    return torch.from_numpy(RE)

def smooth_log_mag(Y_true, Y_pred, filterbank):
    Y_true_smooth = torch.matmul(filterbank, torch.abs(Y_true))
    Y_pred_smooth = torch.matmul(filterbank, torch.abs(Y_pred))

    loss = torch.abs(
        torch.log(Y_true_smooth + 1e-9) - torch.log(Y_pred_smooth + 1e-9)
    )

    loss = loss.mean()

    return loss

class MRSTFTLoss(nn.Module):
    def __init__(self,
                 fft_sizes=[2048, 1024, 512, 256, 128, 64],
                 overlap=0.5,
                 window='hann_window',
                 fs=16000,
                 log_mag_weight=1,
                 sc_weight=0,
                 wsc_weight=0,
                 smooth_log_mag_weight=0,
                 sxcorr_weight=0):
        super().__init__()

        self.fft_sizes = fft_sizes
        self.overlap = overlap
        self.window = window
        self.log_mag_weight = log_mag_weight
        self.sc_weight = sc_weight
        self.wsc_weight = wsc_weight
        self.smooth_log_mag_weight = smooth_log_mag_weight
        self.sxcorr_weight = sxcorr_weight
        self.fs = fs

        # weights for SFM weighted spectral convergence loss
        self.wsc_weights = torch.nn.ParameterDict()
        for fft_size in fft_sizes:
            width = min(11, int(1000 * fft_size / self.fs + .5))
            width += width % 2
            self.wsc_weights[str(fft_size)] = torch.nn.Parameter(
                create_weight_matrix(fft_size // 2 + 1, width),
                requires_grad=False
            )

        # filterbanks for smooth log magnitude loss
        self.filterbanks = torch.nn.ParameterDict()
        for fft_size in fft_sizes:
            self.filterbanks[str(fft_size)] = torch.nn.Parameter(
                gen_filterbank(fft_size//2),
                requires_grad=False
            )


    def __call__(self, y_true, y_pred):


        lm_loss = torch.zeros(1, device=y_true.device)
        sc_loss = torch.zeros(1, device=y_true.device)
        wsc_loss = torch.zeros(1, device=y_true.device)
        slm_loss = torch.zeros(1, device=y_true.device)
        sxcorr_loss = torch.zeros(1, device=y_true.device)

        for fft_size in self.fft_sizes:
            hop_size = int(round(fft_size * (1 - self.overlap)))
            win_size = fft_size

            Y_true = stft(y_true, fft_size, hop_size, win_size, self.window)
            Y_pred = stft(y_pred, fft_size, hop_size, win_size, self.window)

            if self.log_mag_weight > 0:
                lm_loss = lm_loss + log_magnitude_loss(Y_true, Y_pred)

            if self.sc_weight > 0:
                sc_loss = sc_loss + spectral_convergence_loss(Y_true, Y_pred)

            if self.wsc_weight > 0:
                wsc_loss = wsc_loss + weighted_spectral_convergence(Y_true, Y_pred, self.wsc_weights[str(fft_size)])

            if self.smooth_log_mag_weight > 0:
                slm_loss = slm_loss + smooth_log_mag(Y_true, Y_pred, self.filterbanks[str(fft_size)])

            if self.sxcorr_weight > 0:
                sxcorr_loss = sxcorr_loss + spectral_xcorr_loss(Y_true, Y_pred)


        total_loss = (self.log_mag_weight * lm_loss + self.sc_weight * sc_loss
                + self.wsc_weight * wsc_loss + self.smooth_log_mag_weight * slm_loss
                + self.sxcorr_weight * sxcorr_loss) / len(self.fft_sizes)

        return total_loss
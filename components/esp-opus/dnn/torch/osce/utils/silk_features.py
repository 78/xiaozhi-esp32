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

import os

import numpy as np
import torch

import scipy
import scipy.signal

from utils.pitch import hangover, calculate_acorr_window
from utils.spec import create_filter_bank, cepstrum, log_spectrum, log_spectrum_from_lpc

def spec_from_lpc(a, n_fft=128, eps=1e-9):
    order = a.shape[-1]
    assert order + 1 < n_fft

    x = np.zeros((*a.shape[:-1], n_fft ))
    x[..., 0] = 1
    x[..., 1:1 + order] = -a

    X = np.fft.fft(x, axis=-1)
    X = np.abs(X[..., :n_fft//2 + 1]) ** 2

    S = 1 / (X + eps)

    return S

def silk_feature_factory(no_pitch_value=256,
                         acorr_radius=2,
                         pitch_hangover=8,
                         num_bands_clean_spec=64,
                         num_bands_noisy_spec=18,
                         noisy_spec_scale='opus',
                         noisy_apply_dct=True,
                         add_double_lag_acorr=False
                         ):

    w = scipy.signal.windows.cosine(320)
    fb_clean_spec = create_filter_bank(num_bands_clean_spec, 320, scale='erb', round_center_bins=True, normalize=True)
    fb_noisy_spec = create_filter_bank(num_bands_noisy_spec, 320, scale=noisy_spec_scale, round_center_bins=True, normalize=True)

    def create_features(noisy, noisy_history, lpcs, gains, ltps, periods):

        periods = periods.copy()

        if pitch_hangover > 0:
            periods = hangover(periods, num_frames=pitch_hangover)

        periods[periods == 0] = no_pitch_value

        clean_spectrum = 0.3 * log_spectrum_from_lpc(lpcs, fb=fb_clean_spec, n_fft=320)

        if noisy_apply_dct:
            noisy_cepstrum = np.repeat(
                cepstrum(np.concatenate((noisy_history[-160:], noisy), dtype=np.float32), 320, fb_noisy_spec, w), 2, 0)
        else:
            noisy_cepstrum = np.repeat(
                log_spectrum(np.concatenate((noisy_history[-160:], noisy), dtype=np.float32), 320, fb_noisy_spec, w), 2, 0)

        log_gains = np.log(gains + 1e-9).reshape(-1, 1)

        acorr, _ = calculate_acorr_window(noisy, 80, periods, noisy_history, radius=acorr_radius, add_double_lag_acorr=add_double_lag_acorr)

        features = np.concatenate((clean_spectrum, noisy_cepstrum, acorr, ltps, log_gains), axis=-1, dtype=np.float32)

        return features, periods.astype(np.int64)

    return create_features



def load_inference_data(path,
                        no_pitch_value=256,
                        skip=92,
                        preemph=0.85,
                        acorr_radius=2,
                        pitch_hangover=8,
                        num_bands_clean_spec=64,
                        num_bands_noisy_spec=18,
                        noisy_spec_scale='opus',
                        noisy_apply_dct=True,
                        add_double_lag_acorr=False,
                        **kwargs):

    print(f"[load_inference_data]: ignoring keyword arguments {kwargs.keys()}...")

    lpcs    = np.fromfile(os.path.join(path, 'features_lpc.f32'), dtype=np.float32).reshape(-1, 16)
    ltps    = np.fromfile(os.path.join(path, 'features_ltp.f32'), dtype=np.float32).reshape(-1, 5)
    gains   = np.fromfile(os.path.join(path, 'features_gain.f32'), dtype=np.float32)
    periods = np.fromfile(os.path.join(path, 'features_period.s16'), dtype=np.int16)
    num_bits = np.fromfile(os.path.join(path, 'features_num_bits.s32'), dtype=np.int32).astype(np.float32).reshape(-1, 1)
    num_bits_smooth = np.fromfile(os.path.join(path, 'features_num_bits_smooth.f32'), dtype=np.float32).reshape(-1, 1)

    # load signal, add back delay and pre-emphasize
    signal  = np.fromfile(os.path.join(path, 'noisy.s16'), dtype=np.int16).astype(np.float32) / (2 ** 15)
    signal = np.concatenate((np.zeros(skip, dtype=np.float32), signal), dtype=np.float32)

    create_features = silk_feature_factory(no_pitch_value, acorr_radius, pitch_hangover, num_bands_clean_spec, num_bands_noisy_spec, noisy_spec_scale, noisy_apply_dct, add_double_lag_acorr)

    num_frames = min((len(signal) // 320) * 4, len(lpcs))
    signal = signal[: num_frames * 80]
    lpcs = lpcs[: num_frames]
    ltps = ltps[: num_frames]
    gains = gains[: num_frames]
    periods = periods[: num_frames]
    num_bits = num_bits[: num_frames // 4]
    num_bits_smooth = num_bits[: num_frames // 4]

    numbits = np.repeat(np.concatenate((num_bits, num_bits_smooth), axis=-1, dtype=np.float32), 4, axis=0)

    features, periods = create_features(signal, np.zeros(350, dtype=signal.dtype), lpcs, gains, ltps, periods)

    if preemph > 0:
        signal[1:] -= preemph * signal[:-1]

    return torch.from_numpy(signal), torch.from_numpy(features), torch.from_numpy(periods), torch.from_numpy(numbits)

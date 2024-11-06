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

import numpy as np
import scipy.signal

def compute_vad_mask(x, fs, stop_db=-70):

    frame_length = (fs + 49) // 50
    x = x[: frame_length * (len(x) // frame_length)]

    frames = x.reshape(-1, frame_length)
    frame_energy = np.sum(frames ** 2, axis=1)
    frame_energy_smooth = np.convolve(frame_energy, np.ones(5) / 5, mode='same')

    max_threshold = frame_energy.max() * 10 ** (stop_db/20)
    vactive = np.ones_like(frames)
    vactive[frame_energy_smooth < max_threshold, :] = 0
    vactive = vactive.reshape(-1)

    filter = np.sin(np.arange(frame_length) * np.pi / (frame_length - 1))
    filter = filter / filter.sum()

    mask = np.convolve(vactive, filter, mode='same')

    return x, mask

def convert_mask(mask, num_frames, frame_size=160, hop_size=40):
    num_samples = frame_size + (num_frames - 1) * hop_size
    if len(mask) < num_samples:
        mask = np.concatenate((mask, np.zeros(num_samples - len(mask))), dtype=mask.dtype)
    else:
        mask = mask[:num_samples]

    new_mask = np.array([np.mean(mask[i*hop_size : i*hop_size + frame_size]) for i in range(num_frames)])

    return new_mask

def power_spectrum(x, window_size=160, hop_size=40, window='hamming'):
    num_spectra = (len(x) - window_size - hop_size) // hop_size
    window = scipy.signal.get_window(window, window_size)
    N = window_size // 2

    frames = np.concatenate([x[np.newaxis, i * hop_size : i * hop_size + window_size] for i in range(num_spectra)]) * window
    psd = np.abs(np.fft.fft(frames, axis=1)[:, :N + 1]) ** 2

    return psd


def frequency_mask(num_bands, up_factor, down_factor):

    up_mask = np.zeros((num_bands, num_bands))
    down_mask = np.zeros((num_bands, num_bands))

    for i in range(num_bands):
        up_mask[i, : i + 1] = up_factor ** np.arange(i, -1, -1)
        down_mask[i, i :] = down_factor ** np.arange(num_bands - i)

    return down_mask @ up_mask


def rect_fb(band_limits, num_bins=None):
    num_bands = len(band_limits) - 1
    if num_bins is None:
        num_bins = band_limits[-1]

    fb = np.zeros((num_bands, num_bins))
    for i in range(num_bands):
        fb[i, band_limits[i]:band_limits[i+1]] = 1

    return fb


def compare(x, y, apply_vad=False):
    """ Modified version of opus_compare for 16 kHz mono signals

    Args:
        x (np.ndarray): reference input signal scaled to [-1, 1]
        y (np.ndarray): test signal scaled to [-1, 1]

    Returns:
        float: perceptually weighted error
    """
    # filter bank: bark scale with minimum-2-bin bands and cutoff at 7.5 kHz
    band_limits = [0, 2, 4, 6, 7, 9, 11, 13, 15, 18, 22, 26, 31, 36, 43, 51, 60, 75]
    num_bands = len(band_limits) - 1
    fb = rect_fb(band_limits, num_bins=81)

    # trim samples to same size
    num_samples = min(len(x), len(y))
    x = x[:num_samples] * 2**15
    y = y[:num_samples] * 2**15

    psd_x = power_spectrum(x) + 100000
    psd_y = power_spectrum(y) + 100000

    num_frames = psd_x.shape[0]

    # average band energies
    be_x = (psd_x @ fb.T) / np.sum(fb, axis=1)

    # frequecy masking
    f_mask = frequency_mask(num_bands, 0.1, 0.03)
    mask_x = be_x @ f_mask.T

    # temporal masking
    for i in range(1, num_frames):
        mask_x[i, :] += 0.5 * mask_x[i-1, :]

    # apply mask
    masked_psd_x = psd_x + 0.1 * (mask_x @ fb)
    masked_psd_y = psd_y + 0.1 * (mask_x @ fb)

    # 2-frame average
    masked_psd_x = masked_psd_x[1:] +  masked_psd_x[:-1]
    masked_psd_y = masked_psd_y[1:] +  masked_psd_y[:-1]

    # distortion metric
    re = masked_psd_y / masked_psd_x
    im = np.log(re) ** 2
    Eb = ((im @ fb.T) / np.sum(fb, axis=1))
    Ef = np.mean(Eb , axis=1)

    if apply_vad:
        _, mask = compute_vad_mask(x, 16000)
        mask = convert_mask(mask, Ef.shape[0])
    else:
        mask = np.ones_like(Ef)

    err = np.mean(np.abs(Ef[mask > 1e-6]) ** 3) ** (1/6)

    return float(err)

if __name__ == "__main__":
    import argparse
    from scipy.io import wavfile

    parser = argparse.ArgumentParser()
    parser.add_argument('ref', type=str, help='reference wav file')
    parser.add_argument('deg', type=str, help='degraded wav file')
    parser.add_argument('--apply-vad', action='store_true')
    args = parser.parse_args()


    fs1, x = wavfile.read(args.ref)
    fs2, y = wavfile.read(args.deg)

    if max(fs1, fs2) != 16000:
        raise ValueError('error: encountered sampling frequency diffrent from 16kHz')

    x = x.astype(np.float32) / 2**15
    y = y.astype(np.float32) / 2**15

    err = compare(x, y, apply_vad=args.apply_vad)

    print(f"MOC: {err}")

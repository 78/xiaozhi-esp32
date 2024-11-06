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

def hangover(lags, num_frames=10):
    lags = lags.copy()
    count = 0
    last_lag = 0

    for i in range(len(lags)):
        lag = lags[i]

        if lag == 0:
            if count < num_frames:
                lags[i] = last_lag
                count += 1
        else:
            count = 0
            last_lag = lag

    return lags


def smooth_pitch_lags(lags, d=2):

    assert d < 4

    num_silk_frames = len(lags) // 4

    smoothed_lags = lags.copy()

    tmp = np.arange(1, d+1)
    kernel = np.concatenate((tmp, [d+1], tmp[::-1]), dtype=np.float32)
    kernel = kernel / np.sum(kernel)

    last = lags[0:d][::-1]
    for i in range(num_silk_frames):
        frame = lags[i * 4: (i+1) * 4]

        if np.max(np.abs(frame)) == 0:
            last = frame[4-d:]
            continue

        if i == num_silk_frames - 1:
            next = frame[4-d:][::-1]
        else:
            next = lags[(i+1) * 4 : (i+1) * 4 + d]

        if np.max(np.abs(next)) == 0:
            next = frame[4-d:][::-1]

        if np.max(np.abs(last)) == 0:
            last = frame[0:d][::-1]

        smoothed_frame = np.convolve(np.concatenate((last, frame, next), dtype=np.float32), kernel, mode='valid')

        smoothed_lags[i * 4: (i+1) * 4] = np.round(smoothed_frame)

        last = frame[4-d:]

    return smoothed_lags

def calculate_acorr_window(x, frame_size, lags, history=None, max_lag=300, radius=2, add_double_lag_acorr=False, no_pitch_threshold=32):
    eps = 1e-9

    lag_multiplier = 2 if add_double_lag_acorr else 1

    if history is None:
        history = np.zeros(lag_multiplier * max_lag + radius, dtype=x.dtype)

    offset = len(history)

    assert offset >= max_lag + radius
    assert len(x) % frame_size == 0

    num_frames = len(x) // frame_size
    lags = lags.copy()

    x_ext = np.concatenate((history, x), dtype=x.dtype)

    d = radius
    num_acorrs = 2 * d + 1
    acorrs = np.zeros((num_frames, lag_multiplier * num_acorrs), dtype=x.dtype)

    for idx in range(num_frames):
        lag = lags[idx].item()
        frame = x_ext[offset + idx * frame_size : offset + (idx + 1) * frame_size]

        for k in range(lag_multiplier):
            lag1 = (k + 1) * lag if lag >= no_pitch_threshold else lag
            for j in range(num_acorrs):
                past = x_ext[offset + idx * frame_size - lag1 + j - d : offset + (idx + 1) * frame_size - lag1 + j - d]
                acorrs[idx, j + k * num_acorrs] = np.dot(frame, past) / np.sqrt(np.dot(frame, frame) * np.dot(past, past) + eps)

    return acorrs, lags
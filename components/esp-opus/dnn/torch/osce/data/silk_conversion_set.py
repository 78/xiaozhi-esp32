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

from torch.utils.data import Dataset
import numpy as np

from utils.silk_features import silk_feature_factory
from utils.pitch import hangover, calculate_acorr_window


class SilkEnhancementSet(Dataset):
    def __init__(self,
                 path,
                 frames_per_sample=100,
                 no_pitch_value=9,
                 acorr_radius=2,
                 pitch_hangover=8,
                 num_bands_clean_spec=64,
                 num_bands_noisy_spec=18,
                 noisy_spec_scale='opus',
                 noisy_apply_dct=True,
                 add_offset=False,
                 add_double_lag_acorr=False
                 ):

        assert frames_per_sample % 4 == 0

        self.frame_size = 80
        self.frames_per_sample = frames_per_sample
        self.no_pitch_value = no_pitch_value
        self.acorr_radius = acorr_radius
        self.pitch_hangover = pitch_hangover
        self.num_bands_clean_spec = num_bands_clean_spec
        self.num_bands_noisy_spec = num_bands_noisy_spec
        self.noisy_spec_scale = noisy_spec_scale
        self.add_double_lag_acorr = add_double_lag_acorr

        self.lpcs = np.fromfile(os.path.join(path, 'features_lpc.f32'), dtype=np.float32).reshape(-1, 16)
        self.ltps = np.fromfile(os.path.join(path, 'features_ltp.f32'), dtype=np.float32).reshape(-1, 5)
        self.periods = np.fromfile(os.path.join(path, 'features_period.s16'), dtype=np.int16)
        self.gains = np.fromfile(os.path.join(path, 'features_gain.f32'), dtype=np.float32)
        self.num_bits = np.fromfile(os.path.join(path, 'features_num_bits.s32'), dtype=np.int32)
        self.num_bits_smooth = np.fromfile(os.path.join(path, 'features_num_bits_smooth.f32'), dtype=np.float32)
        self.offsets = np.fromfile(os.path.join(path, 'features_offset.f32'), dtype=np.float32)
        self.lpcnet_features = np.from_file(os.path.join(path, 'features_lpcnet.f32'), dtype=np.float32).reshape(-1, 36)

        self.coded_signal = np.fromfile(os.path.join(path, 'coded.s16'), dtype=np.int16)

        self.create_features = silk_feature_factory(no_pitch_value,
                                                    acorr_radius,
                                                    pitch_hangover,
                                                    num_bands_clean_spec,
                                                    num_bands_noisy_spec,
                                                    noisy_spec_scale,
                                                    noisy_apply_dct,
                                                    add_offset,
                                                    add_double_lag_acorr)

        self.history_len = 700 if add_double_lag_acorr else 350
        # discard some frames to have enough signal history
        self.skip_frames = 4 * ((self.history_len + 319) // 320 + 2)

        num_frames = self.clean_signal.shape[0] // 80 - self.skip_frames

        self.len = num_frames // frames_per_sample

    def __len__(self):
        return self.len

    def __getitem__(self, index):

        frame_start = self.frames_per_sample * index + self.skip_frames
        frame_stop  = frame_start + self.frames_per_sample

        signal_start = frame_start * self.frame_size - self.skip
        signal_stop  = frame_stop  * self.frame_size - self.skip

        coded_signal = self.coded_signal[signal_start : signal_stop].astype(np.float32) / 2**15

        coded_signal_history = self.coded_signal[signal_start - self.history_len : signal_start].astype(np.float32) / 2**15

        features, periods = self.create_features(
              coded_signal,
              coded_signal_history,
              self.lpcs[frame_start : frame_stop],
              self.gains[frame_start : frame_stop],
              self.ltps[frame_start : frame_stop],
              self.periods[frame_start : frame_stop],
              self.offsets[frame_start : frame_stop]
        )

        lpcnet_features = self.lpcnet_features[frame_start // 2 : frame_stop // 2, :20]

        num_bits        = np.repeat(self.num_bits[frame_start // 4 : frame_stop // 4], 4).astype(np.float32).reshape(-1, 1)
        num_bits_smooth = np.repeat(self.num_bits_smooth[frame_start // 4 : frame_stop // 4], 4).astype(np.float32).reshape(-1, 1)

        numbits = np.concatenate((num_bits, num_bits_smooth), axis=-1)

        return {
            'silk_features'   : features,
            'periods'         : periods.astype(np.int64),
            'numbits'         : numbits.astype(np.float32),
            'lpcnet_features' : lpcnet_features
            }

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

""" Dataset for LPCNet training """
import os

import yaml
import torch
import numpy as np
from torch.utils.data import Dataset


scale = 255.0/32768.0
scale_1 = 32768.0/255.0
def ulaw2lin(u):
    u = u - 128
    s = np.sign(u)
    u = np.abs(u)
    return s*scale_1*(np.exp(u/128.*np.log(256))-1)


def lin2ulaw(x):
    s = np.sign(x)
    x = np.abs(x)
    u = (s*(128*np.log(1+scale*x)/np.log(256)))
    u = np.clip(128 + np.round(u), 0, 255)
    return u


def run_lpc(signal, lpcs, frame_length=160):
    num_frames, lpc_order = lpcs.shape

    prediction = np.concatenate(
        [- np.convolve(signal[i * frame_length : (i + 1) * frame_length + lpc_order - 1], lpcs[i], mode='valid') for i in range(num_frames)]
    )
    error = signal[lpc_order :] - prediction

    return prediction, error

class LPCNetVocodingDataset(Dataset):
    def __init__(self,
                 path_to_dataset,
                 features=['cepstrum', 'periods', 'pitch_corr'],
                 target='signal',
                 frames_per_sample=100,
                 feature_history=0,
                 feature_lookahead=0,
                 lpc_gamma=1):

        super().__init__()

        # load dataset info
        self.path_to_dataset = path_to_dataset
        with open(os.path.join(path_to_dataset, 'info.yml'), 'r') as f:
            dataset = yaml.load(f, yaml.FullLoader)

        # dataset version
        self.version = dataset['version']
        if self.version == 1:
            self.getitem = self.getitem_v1
        elif self.version == 2:
            self.getitem = self.getitem_v2
        else:
            raise ValueError(f"dataset version {self.version} unknown")

        # features
        self.feature_history      = feature_history
        self.feature_lookahead    = feature_lookahead
        self.frame_offset         = 2 + self.feature_history
        self.frames_per_sample    = frames_per_sample
        self.input_features       = features
        self.feature_frame_layout = dataset['feature_frame_layout']
        self.lpc_gamma            = lpc_gamma

        # load feature file
        self.feature_file = os.path.join(path_to_dataset, dataset['feature_file'])
        self.features = np.memmap(self.feature_file, dtype=dataset['feature_dtype'])
        self.feature_frame_length = dataset['feature_frame_length']

        assert len(self.features) % self.feature_frame_length == 0
        self.features = self.features.reshape((-1, self.feature_frame_length))

        # derive number of samples is dataset
        self.dataset_length = (len(self.features) - self.frame_offset - self.feature_lookahead - 1 - 2) // self.frames_per_sample

        # signals
        self.frame_length               = dataset['frame_length']
        self.signal_frame_layout        = dataset['signal_frame_layout']
        self.target                     = target

        # load signals
        self.signal_file  = os.path.join(path_to_dataset, dataset['signal_file'])
        self.signals  = np.memmap(self.signal_file, dtype=dataset['signal_dtype'])
        self.signal_frame_length  = dataset['signal_frame_length']
        self.signals = self.signals.reshape((-1, self.signal_frame_length))
        assert len(self.signals) == len(self.features) * self.frame_length


    def __getitem__(self, index):
        return self.getitem(index)

    def getitem_v2(self, index):
        sample = dict()

        # extract features
        frame_start = self.frame_offset + index       * self.frames_per_sample - self.feature_history
        frame_stop  = self.frame_offset + (index + 1) * self.frames_per_sample + self.feature_lookahead

        for feature in self.input_features:
            feature_start, feature_stop = self.feature_frame_layout[feature]
            sample[feature] = self.features[frame_start : frame_stop, feature_start : feature_stop]

        # convert periods
        if 'periods' in self.input_features:
            sample['periods'] = (0.1 + 50 * sample['periods'] + 100).astype('int16')

        signal_start = (self.frame_offset + index       * self.frames_per_sample) * self.frame_length
        signal_stop  = (self.frame_offset + (index + 1) * self.frames_per_sample) * self.frame_length

        # last_signal and signal are always expected to be there
        sample['last_signal'] = self.signals[signal_start : signal_stop, self.signal_frame_layout['last_signal']]
        sample['signal'] = self.signals[signal_start : signal_stop, self.signal_frame_layout['signal']]

        # calculate prediction and error if lpc coefficients present and prediction not given
        if 'lpc' in self.feature_frame_layout and 'prediction' not in self.signal_frame_layout:
            # lpc coefficients with one frame lookahead
            # frame positions (start one frame early for past excitation)
            frame_start = self.frame_offset + self.frames_per_sample * index - 1
            frame_stop  = self.frame_offset + self.frames_per_sample * (index + 1)

            # feature positions
            lpc_start, lpc_stop = self.feature_frame_layout['lpc']
            lpc_order = lpc_stop - lpc_start
            lpcs = self.features[frame_start : frame_stop, lpc_start : lpc_stop]

            # LPC weighting
            lpc_order = lpc_stop - lpc_start
            weights = np.array([self.lpc_gamma ** (i + 1) for i in range(lpc_order)])
            lpcs = lpcs * weights

            # signal position (lpc_order samples as history)
            signal_start = frame_start * self.frame_length - lpc_order + 1
            signal_stop  = frame_stop  * self.frame_length + 1
            noisy_signal = self.signals[signal_start : signal_stop, self.signal_frame_layout['last_signal']]
            clean_signal = self.signals[signal_start - 1 : signal_stop - 1, self.signal_frame_layout['signal']]

            noisy_prediction, noisy_error = run_lpc(noisy_signal, lpcs, frame_length=self.frame_length)

            # extract signals
            offset = self.frame_length
            sample['prediction'] = noisy_prediction[offset : offset + self.frame_length * self.frames_per_sample]
            sample['last_error'] = noisy_error[offset - 1 : offset - 1 + self.frame_length * self.frames_per_sample]
            # calculate error between real signal and noisy prediction


            sample['error'] = sample['signal'] - sample['prediction']


        # concatenate features
        feature_keys = [key for key in self.input_features if not key.startswith("periods")]
        features = torch.concat([torch.FloatTensor(sample[key]) for key in feature_keys], dim=-1)
        target  = torch.FloatTensor(sample[self.target]) / 2**15
        periods = torch.LongTensor(sample['periods'])

        return {'features' : features, 'periods' : periods, 'target' : target}

    def getitem_v1(self, index):
        sample = dict()

        # extract features
        frame_start = self.frame_offset + index       * self.frames_per_sample - self.feature_history
        frame_stop  = self.frame_offset + (index + 1) * self.frames_per_sample + self.feature_lookahead

        for feature in self.input_features:
            feature_start, feature_stop = self.feature_frame_layout[feature]
            sample[feature] = self.features[frame_start : frame_stop, feature_start : feature_stop]

        # convert periods
        if 'periods' in self.input_features:
            sample['periods'] = (0.1 + 50 * sample['periods'] + 100).astype('int16')

        signal_start = (self.frame_offset + index       * self.frames_per_sample) * self.frame_length
        signal_stop  = (self.frame_offset + (index + 1) * self.frames_per_sample) * self.frame_length

        # last_signal and signal are always expected to be there
        for signal_name, index in self.signal_frame_layout.items():
            sample[signal_name] = self.signals[signal_start : signal_stop, index]

        # concatenate features
        feature_keys = [key for key in self.input_features if not key.startswith("periods")]
        features = torch.concat([torch.FloatTensor(sample[key]) for key in feature_keys], dim=-1)
        signals = torch.cat([torch.LongTensor(sample[key]).unsqueeze(-1) for key in self.input_signals], dim=-1)
        target  = torch.LongTensor(sample[self.target])
        periods = torch.LongTensor(sample['periods'])

        return {'features' : features, 'periods' : periods, 'signals' : signals, 'target' : target}

    def __len__(self):
        return self.dataset_length

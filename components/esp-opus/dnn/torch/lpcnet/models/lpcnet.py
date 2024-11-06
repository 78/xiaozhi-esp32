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
from torch import nn
import numpy as np

from utils.ulaw import lin2ulawq, ulaw2lin
from utils.sample import sample_excitation
from utils.pcm import clip_to_int16
from utils.sparsification import GRUSparsifier, calculate_gru_flops_per_step
from utils.layers import DualFC
from utils.misc import get_pdf_from_tree


class LPCNet(nn.Module):
    def __init__(self, config):
        super(LPCNet, self).__init__()

        #
        self.input_layout       = config['input_layout']
        self.feature_history    = config['feature_history']
        self.feature_lookahead  = config['feature_lookahead']

        # frame rate network parameters
        self.feature_dimension          = config['feature_dimension']
        self.period_embedding_dim       = config['period_embedding_dim']
        self.period_levels              = config['period_levels']
        self.feature_channels           = self.feature_dimension + self.period_embedding_dim
        self.feature_conditioning_dim   = config['feature_conditioning_dim']
        self.feature_conv_kernel_size   = config['feature_conv_kernel_size']


        # frame rate network layers
        self.period_embedding   = nn.Embedding(self.period_levels, self.period_embedding_dim)
        self.feature_conv1      = nn.Conv1d(self.feature_channels, self.feature_conditioning_dim, self.feature_conv_kernel_size, padding='valid')
        self.feature_conv2      = nn.Conv1d(self.feature_conditioning_dim, self.feature_conditioning_dim, self.feature_conv_kernel_size, padding='valid')
        self.feature_dense1     = nn.Linear(self.feature_conditioning_dim, self.feature_conditioning_dim)
        self.feature_dense2     = nn.Linear(*(2*[self.feature_conditioning_dim]))

        # sample rate network parameters
        self.frame_size             = config['frame_size']
        self.signal_levels          = config['signal_levels']
        self.signal_embedding_dim   = config['signal_embedding_dim']
        self.gru_a_units            = config['gru_a_units']
        self.gru_b_units            = config['gru_b_units']
        self.output_levels          = config['output_levels']
        self.hsampling              = config.get('hsampling', False)

        self.gru_a_input_dim        = len(self.input_layout['signals']) * self.signal_embedding_dim + self.feature_conditioning_dim
        self.gru_b_input_dim        = self.gru_a_units + self.feature_conditioning_dim

        # sample rate network layers
        self.signal_embedding   = nn.Embedding(self.signal_levels, self.signal_embedding_dim)
        self.gru_a              = nn.GRU(self.gru_a_input_dim, self.gru_a_units, batch_first=True)
        self.gru_b              = nn.GRU(self.gru_b_input_dim, self.gru_b_units, batch_first=True)
        self.dual_fc            = DualFC(self.gru_b_units, self.output_levels)

        # sparsification
        self.sparsifier = []

        # GRU A
        if 'gru_a' in config['sparsification']:
            gru_config  = config['sparsification']['gru_a']
            task_list = [(self.gru_a, gru_config['params'])]
            self.sparsifier.append(GRUSparsifier(task_list,
                                                 gru_config['start'],
                                                 gru_config['stop'],
                                                 gru_config['interval'],
                                                 gru_config['exponent'])
            )
            self.gru_a_flops_per_step = calculate_gru_flops_per_step(self.gru_a,
                                                                      gru_config['params'], drop_input=True)
        else:
            self.gru_a_flops_per_step = calculate_gru_flops_per_step(self.gru_a, drop_input=True)

        # GRU B
        if 'gru_b' in config['sparsification']:
            gru_config  = config['sparsification']['gru_b']
            task_list = [(self.gru_b, gru_config['params'])]
            self.sparsifier.append(GRUSparsifier(task_list,
                                                 gru_config['start'],
                                                 gru_config['stop'],
                                                 gru_config['interval'],
                                                 gru_config['exponent'])
            )
            self.gru_b_flops_per_step = calculate_gru_flops_per_step(self.gru_b,
                                                                      gru_config['params'])
        else:
            self.gru_b_flops_per_step = calculate_gru_flops_per_step(self.gru_b)

        # inference parameters
        self.lpc_gamma = config.get('lpc_gamma', 1)

    def sparsify(self):
        for sparsifier in self.sparsifier:
            sparsifier.step()

    def get_gflops(self, fs, verbose=False):
        gflops = 0

        # frame rate network
        conditioning_dim = self.feature_conditioning_dim
        feature_channels = self.feature_channels
        frame_rate = fs / self.frame_size
        frame_rate_network_complexity = 1e-9 * 2 * (5 * conditioning_dim + 3 * feature_channels) * conditioning_dim * frame_rate
        if verbose:
            print(f"frame rate network: {frame_rate_network_complexity} GFLOPS")
        gflops += frame_rate_network_complexity

        # gru a
        gru_a_rate = fs
        gru_a_complexity = 1e-9 * gru_a_rate * self.gru_a_flops_per_step
        if verbose:
            print(f"gru A: {gru_a_complexity} GFLOPS")
        gflops += gru_a_complexity

        # gru b
        gru_b_rate = fs
        gru_b_complexity = 1e-9 * gru_b_rate * self.gru_b_flops_per_step
        if verbose:
            print(f"gru B: {gru_b_complexity} GFLOPS")
        gflops += gru_b_complexity


        # dual fcs
        fc = self.dual_fc
        rate = fs
        input_size = fc.dense1.in_features
        output_size = fc.dense1.out_features
        dual_fc_complexity = 1e-9 *  (4 * input_size * output_size + 22 * output_size) * rate
        if self.hsampling:
            dual_fc_complexity /= 8
        if verbose:
            print(f"dual_fc: {dual_fc_complexity} GFLOPS")
        gflops += dual_fc_complexity

        if verbose:
            print(f'total: {gflops} GFLOPS')

        return gflops

    def frame_rate_network(self, features, periods):

        embedded_periods = torch.flatten(self.period_embedding(periods), 2, 3)
        features = torch.concat((features, embedded_periods), dim=-1)

        # convert to channels first and calculate conditioning vector
        c = torch.permute(features, [0, 2, 1])

        c = torch.tanh(self.feature_conv1(c))
        c = torch.tanh(self.feature_conv2(c))
        # back to channels last
        c = torch.permute(c, [0, 2, 1])
        c = torch.tanh(self.feature_dense1(c))
        c = torch.tanh(self.feature_dense2(c))

        return c

    def sample_rate_network(self, signals, c, gru_states):
        embedded_signals = torch.flatten(self.signal_embedding(signals), 2, 3)
        c_upsampled      = torch.repeat_interleave(c, self.frame_size, dim=1)

        y = torch.concat((embedded_signals, c_upsampled), dim=-1)
        y, gru_a_state = self.gru_a(y, gru_states[0])
        y = torch.concat((y, c_upsampled), dim=-1)
        y, gru_b_state = self.gru_b(y, gru_states[1])

        y = self.dual_fc(y)

        if self.hsampling:
            y = torch.sigmoid(y)
            log_probs = torch.log(get_pdf_from_tree(y) + 1e-6)
        else:
            log_probs = torch.log_softmax(y, dim=-1)

        return log_probs, (gru_a_state, gru_b_state)

    def decoder(self, signals, c, gru_states):
        embedded_signals = torch.flatten(self.signal_embedding(signals), 2, 3)

        y = torch.concat((embedded_signals, c), dim=-1)
        y, gru_a_state = self.gru_a(y, gru_states[0])
        y = torch.concat((y, c), dim=-1)
        y, gru_b_state = self.gru_b(y, gru_states[1])

        y = self.dual_fc(y)

        if self.hsampling:
            y = torch.sigmoid(y)
            probs = get_pdf_from_tree(y)
        else:
            probs = torch.softmax(y, dim=-1)

        return probs, (gru_a_state, gru_b_state)

    def forward(self, features, periods, signals, gru_states):

        c            = self.frame_rate_network(features, periods)
        log_probs, _ = self.sample_rate_network(signals, c, gru_states)

        return log_probs

    def generate(self, features, periods, lpcs):

        with torch.no_grad():
            device = self.parameters().__next__().device

            num_frames          = features.shape[0] - self.feature_history - self.feature_lookahead
            lpc_order           = lpcs.shape[-1]
            num_input_signals   = len(self.input_layout['signals'])
            pitch_corr_position = self.input_layout['features']['pitch_corr'][0]

            # signal buffers
            pcm    = torch.zeros((num_frames * self.frame_size + lpc_order))
            output = torch.zeros((num_frames * self.frame_size), dtype=torch.int16)
            mem = 0

            # state buffers
            gru_a_state = torch.zeros((1, 1, self.gru_a_units))
            gru_b_state = torch.zeros((1, 1, self.gru_b_units))
            gru_states = [gru_a_state, gru_b_state]

            input_signals = torch.zeros((1, 1, num_input_signals), dtype=torch.long) + 128

            # push data to device
            features = features.to(device)
            periods  = periods.to(device)
            lpcs     = lpcs.to(device)

            # lpc weighting
            weights = torch.FloatTensor([self.lpc_gamma ** (i + 1) for i in range(lpc_order)]).to(device)
            lpcs    = lpcs * weights

            # run feature encoding
            c = self.frame_rate_network(features.unsqueeze(0), periods.unsqueeze(0))

            for frame_index in range(num_frames):
                frame_start = frame_index * self.frame_size
                pitch_corr  = features[frame_index + self.feature_history, pitch_corr_position]
                a           = - torch.flip(lpcs[frame_index + self.feature_history], [0])
                current_c   = c[:, frame_index : frame_index + 1, :]

                for i in range(self.frame_size):
                    pcm_position    = frame_start + i + lpc_order
                    output_position = frame_start + i

                    # prepare input
                    pred = torch.sum(pcm[pcm_position - lpc_order : pcm_position] * a)
                    if 'prediction' in self.input_layout['signals']:
                        input_signals[0, 0, self.input_layout['signals']['prediction']] = lin2ulawq(pred)

                    # run single step of sample rate network
                    probs, gru_states = self.decoder(
                                            input_signals,
                                            current_c,
                                            gru_states
                                    )

                    # sample from output
                    exc_ulaw = sample_excitation(probs, pitch_corr)

                    # signal generation
                    exc = ulaw2lin(exc_ulaw)
                    sig = exc + pred
                    pcm[pcm_position] = sig
                    mem = 0.85 * mem + float(sig)
                    output[output_position] = clip_to_int16(round(mem))

                    # buffer update
                    if 'last_signal' in self.input_layout['signals']:
                        input_signals[0, 0, self.input_layout['signals']['last_signal']] = lin2ulawq(sig)

                    if 'last_error' in self.input_layout['signals']:
                        input_signals[0, 0, self.input_layout['signals']['last_error']]  = lin2ulawq(exc)

        return output

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
from utils.layers.subconditioner import get_subconditioner
from utils.layers import DualFC

from utils.ulaw import lin2ulawq, ulaw2lin
from utils.sample import sample_excitation
from utils.pcm import clip_to_int16
from utils.sparsification import GRUSparsifier, calculate_gru_flops_per_step

from utils.misc import interleave_tensors




# MultiRateLPCNet
class MultiRateLPCNet(nn.Module):
    def __init__(self, config):
        super(MultiRateLPCNet, self).__init__()

        # general parameters
        self.input_layout       = config['input_layout']
        self.feature_history    = config['feature_history']
        self.feature_lookahead  = config['feature_lookahead']
        self.signals            = config['signals']

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

        # subconditioning B
        sub_config = config['subconditioning']['subconditioning_b']
        self.substeps_b = sub_config['number_of_subsamples']
        self.subcondition_signals_b = sub_config['signals']
        self.signals_idx_b = [self.input_layout['signals'][key] for key in sub_config['signals']]
        method = sub_config['method']
        kwargs = sub_config['kwargs']
        if type(kwargs) == type(None):
            kwargs = dict()

        state_size = self.gru_b_units
        self.subconditioner_b = get_subconditioner(method,
            sub_config['number_of_subsamples'], sub_config['pcm_embedding_size'],
            state_size, self.signal_levels, len(sub_config['signals']),
            **sub_config['kwargs'])

         # subconditioning A
        sub_config = config['subconditioning']['subconditioning_a']
        self.substeps_a = sub_config['number_of_subsamples']
        self.subcondition_signals_a = sub_config['signals']
        self.signals_idx_a = [self.input_layout['signals'][key] for key in sub_config['signals']]
        method = sub_config['method']
        kwargs = sub_config['kwargs']
        if type(kwargs) == type(None):
            kwargs = dict()

        state_size = self.gru_a_units
        self.subconditioner_a = get_subconditioner(method,
            sub_config['number_of_subsamples'], sub_config['pcm_embedding_size'],
            state_size, self.signal_levels, self.substeps_b * len(sub_config['signals']),
            **sub_config['kwargs'])


        # wrap up subconditioning, group_size_gru_a holds the number
        # of timesteps that are grouped as sample input for GRU A
        # input and group_size_subcondition_a holds the number of samples that are
        # grouped as input to pre-GRU B subconditioning
        self.group_size_gru_a = self.substeps_a * self.substeps_b
        self.group_size_subcondition_a = self.substeps_b
        self.gru_a_rate_divider = self.group_size_gru_a
        self.gru_b_rate_divider = self.substeps_b

        # gru sizes
        self.gru_a_input_dim        = self.group_size_gru_a * len(self.signals) * self.signal_embedding_dim + self.feature_conditioning_dim
        self.gru_b_input_dim        = self.subconditioner_a.get_output_dim(0) + self.feature_conditioning_dim
        self.signals_idx            = [self.input_layout['signals'][key] for key in self.signals]

        # sample rate network layers
        self.signal_embedding   = nn.Embedding(self.signal_levels, self.signal_embedding_dim)
        self.gru_a              = nn.GRU(self.gru_a_input_dim, self.gru_a_units, batch_first=True)
        self.gru_b              = nn.GRU(self.gru_b_input_dim, self.gru_b_units, batch_first=True)

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



        # dual FCs
        self.dual_fc = []
        for i in range(self.substeps_b):
            dim = self.subconditioner_b.get_output_dim(i)
            self.dual_fc.append(DualFC(dim, self.output_levels))
            self.add_module(f"dual_fc_{i}", self.dual_fc[-1])

    def get_gflops(self, fs, verbose=False, hierarchical_sampling=False):
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
        gru_a_rate = fs / self.group_size_gru_a
        gru_a_complexity = 1e-9 * gru_a_rate * self.gru_a_flops_per_step
        if verbose:
            print(f"gru A: {gru_a_complexity} GFLOPS")
        gflops += gru_a_complexity

        # subconditioning a
        subcond_a_rate = fs / self.substeps_b
        subconditioning_a_complexity = 1e-9 * self.subconditioner_a.get_average_flops_per_step() * subcond_a_rate
        if verbose:
            print(f"subconditioning A: {subconditioning_a_complexity} GFLOPS")
        gflops += subconditioning_a_complexity

        # gru b
        gru_b_rate = fs / self.substeps_b
        gru_b_complexity = 1e-9 * gru_b_rate * self.gru_b_flops_per_step
        if verbose:
            print(f"gru B: {gru_b_complexity} GFLOPS")
        gflops += gru_b_complexity

        # subconditioning b
        subcond_b_rate = fs
        subconditioning_b_complexity = 1e-9 * self.subconditioner_b.get_average_flops_per_step() * subcond_b_rate
        if verbose:
            print(f"subconditioning B: {subconditioning_b_complexity} GFLOPS")
        gflops += subconditioning_b_complexity

        # dual fcs
        for i, fc in enumerate(self.dual_fc):
            rate = fs / len(self.dual_fc)
            input_size = fc.dense1.in_features
            output_size = fc.dense1.out_features
            dual_fc_complexity = 1e-9 *  (4 * input_size * output_size + 22 * output_size) * rate
            if hierarchical_sampling:
                dual_fc_complexity /= 8
            if verbose:
                print(f"dual_fc_{i}: {dual_fc_complexity} GFLOPS")
            gflops += dual_fc_complexity

        if verbose:
            print(f'total: {gflops} GFLOPS')

        return gflops



    def sparsify(self):
        for sparsifier in self.sparsifier:
            sparsifier.step()

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

    def prepare_signals(self, signals, group_size, signal_idx):
        """ extracts, delays and groups signals """

        batch_size, sequence_length, num_signals = signals.shape

        # extract signals according to position
        signals = torch.cat([signals[:, :, i : i + 1] for i in signal_idx],
                            dim=-1)

        # roll back pcm to account for grouping
        signals  = torch.roll(signals, group_size - 1, -2)

        # reshape
        signals = torch.reshape(signals,
            (batch_size, sequence_length // group_size, group_size * len(signal_idx)))

        return signals


    def sample_rate_network(self, signals, c, gru_states):

        signals_a        = self.prepare_signals(signals, self.group_size_gru_a, self.signals_idx)
        embedded_signals = torch.flatten(self.signal_embedding(signals_a), 2, 3)
        # features at GRU A rate
        c_upsampled_a    = torch.repeat_interleave(c, self.frame_size // self.gru_a_rate_divider, dim=1)
        # features at GRU B rate
        c_upsampled_b    = torch.repeat_interleave(c, self.frame_size // self.gru_b_rate_divider, dim=1)

        y = torch.concat((embedded_signals, c_upsampled_a), dim=-1)
        y, gru_a_state = self.gru_a(y, gru_states[0])
        # first round of upsampling and subconditioning
        c_signals_a = self.prepare_signals(signals, self.group_size_subcondition_a, self.signals_idx_a)
        y = self.subconditioner_a(y, c_signals_a)
        y = interleave_tensors(y)

        y = torch.concat((y, c_upsampled_b), dim=-1)
        y, gru_b_state = self.gru_b(y, gru_states[1])
        c_signals_b = self.prepare_signals(signals, 1, self.signals_idx_b)
        y = self.subconditioner_b(y, c_signals_b)

        y = [self.dual_fc[i](y[i]) for i in range(self.substeps_b)]
        y = interleave_tensors(y)

        return y, (gru_a_state, gru_b_state)

    def decoder(self, signals, c, gru_states):
        embedded_signals = torch.flatten(self.signal_embedding(signals), 2, 3)

        y = torch.concat((embedded_signals, c), dim=-1)
        y, gru_a_state = self.gru_a(y, gru_states[0])
        y = torch.concat((y, c), dim=-1)
        y, gru_b_state = self.gru_b(y, gru_states[1])

        y = self.dual_fc(y)

        return torch.softmax(y, dim=-1), (gru_a_state, gru_b_state)

    def forward(self, features, periods, signals, gru_states):

        c           = self.frame_rate_network(features, periods)
        y, _        = self.sample_rate_network(signals, c, gru_states)
        log_probs   = torch.log_softmax(y, dim=-1)

        return log_probs

    def generate(self, features, periods, lpcs):

        with torch.no_grad():
            device = self.parameters().__next__().device

            num_frames          = features.shape[0] - self.feature_history - self.feature_lookahead
            lpc_order           = lpcs.shape[-1]
            num_input_signals   = len(self.signals)
            pitch_corr_position = self.input_layout['features']['pitch_corr'][0]

            # signal buffers
            last_signal       = torch.zeros((num_frames * self.frame_size + lpc_order + 1))
            prediction        = torch.zeros((num_frames * self.frame_size + lpc_order + 1))
            last_error        = torch.zeros((num_frames * self.frame_size + lpc_order + 1))
            output            = torch.zeros((num_frames * self.frame_size), dtype=torch.int16)
            mem = 0

            # state buffers
            gru_a_state = torch.zeros((1, 1, self.gru_a_units))
            gru_b_state = torch.zeros((1, 1, self.gru_b_units))

            input_signals = 128 + torch.zeros(self.group_size_gru_a * num_input_signals, dtype=torch.long)
            # conditioning signals for subconditioner a
            c_signals_a   = 128 + torch.zeros(self.group_size_subcondition_a * len(self.signals_idx_a), dtype=torch.long)
            # conditioning signals for subconditioner b
            c_signals_b   = 128 + torch.zeros(len(self.signals_idx_b), dtype=torch.long)

            # signal dict
            signal_dict = {
                'prediction'    : prediction,
                'last_error'    : last_error,
                'last_signal'   : last_signal
            }

            # push data to device
            features = features.to(device)
            periods  = periods.to(device)
            lpcs     = lpcs.to(device)

            # run feature encoding
            c = self.frame_rate_network(features.unsqueeze(0), periods.unsqueeze(0))

            for frame_index in range(num_frames):
                frame_start = frame_index * self.frame_size
                pitch_corr  = features[frame_index + self.feature_history, pitch_corr_position]
                a           = - torch.flip(lpcs[frame_index + self.feature_history], [0])
                current_c   = c[:, frame_index : frame_index + 1, :]

                for i in range(0, self.frame_size, self.group_size_gru_a):
                    pcm_position    = frame_start + i + lpc_order
                    output_position = frame_start + i

                    # calculate newest prediction
                    prediction[pcm_position] = torch.sum(last_signal[pcm_position - lpc_order + 1: pcm_position + 1] * a)

                    # prepare input
                    for slot in range(self.group_size_gru_a):
                        k = slot - self.group_size_gru_a + 1
                        for idx, name in enumerate(self.signals):
                            input_signals[idx + slot * num_input_signals] = lin2ulawq(
                                signal_dict[name][pcm_position + k]
                            )


                    # run GRU A
                    embed_signals   = self.signal_embedding(input_signals.reshape((1, 1, -1)))
                    embed_signals   = torch.flatten(embed_signals, 2)
                    y               = torch.cat((embed_signals, current_c), dim=-1)
                    h_a, gru_a_state  = self.gru_a(y, gru_a_state)

                    # loop over substeps_a
                    for step_a in range(self.substeps_a):
                        # prepare conditioning input
                        for slot in range(self.group_size_subcondition_a):
                            k = slot - self.group_size_subcondition_a + 1
                            for idx, name in enumerate(self.subcondition_signals_a):
                                c_signals_a[idx + slot * num_input_signals] = lin2ulawq(
                                    signal_dict[name][pcm_position + k]
                                )

                        # subconditioning
                        h_a = self.subconditioner_a.single_step(step_a, h_a, c_signals_a.reshape((1, 1, -1)))

                        # run GRU B
                        y = torch.cat((h_a, current_c), dim=-1)
                        h_b, gru_b_state = self.gru_b(y, gru_b_state)

                        # loop over substeps b
                        for step_b in range(self.substeps_b):
                            # prepare subconditioning input
                            for idx, name in enumerate(self.subcondition_signals_b):
                                c_signals_b[idx] = lin2ulawq(
                                    signal_dict[name][pcm_position]
                                )

                            # subcondition
                            h_b = self.subconditioner_b.single_step(step_b, h_b, c_signals_b.reshape((1, 1, -1)))

                            # run dual FC
                            probs = torch.softmax(self.dual_fc[step_b](h_b), dim=-1)

                            # sample
                            new_exc = ulaw2lin(sample_excitation(probs, pitch_corr))

                            # update signals
                            sig = new_exc + prediction[pcm_position]
                            last_error[pcm_position + 1] = new_exc
                            last_signal[pcm_position + 1] = sig

                            mem = 0.85 * mem + float(sig)
                            output[output_position] = clip_to_int16(round(mem))

                            # increase positions
                            pcm_position += 1
                            output_position += 1

                            # calculate next prediction
                            prediction[pcm_position] = torch.sum(last_signal[pcm_position - lpc_order + 1: pcm_position + 1] * a)

        return output

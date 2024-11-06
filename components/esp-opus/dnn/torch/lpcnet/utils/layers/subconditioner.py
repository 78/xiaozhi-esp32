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

from re import sub
import torch
from torch import nn




def get_subconditioner( method,
                        number_of_subsamples,
                        pcm_embedding_size,
                        state_size,
                        pcm_levels,
                        number_of_signals,
                        **kwargs):

    subconditioner_dict = {
        'additive'      : AdditiveSubconditioner,
        'concatenative' : ConcatenativeSubconditioner,
        'modulative'    : ModulativeSubconditioner
    }

    return subconditioner_dict[method](number_of_subsamples,
        pcm_embedding_size, state_size, pcm_levels, number_of_signals, **kwargs)


class Subconditioner(nn.Module):
    def __init__(self):
        """ upsampling by subconditioning

            Upsamples a sequence of states conditioning on pcm signals and
            optionally a feature vector.
        """
        super(Subconditioner, self).__init__()

    def forward(self, states, signals, features=None):
        raise Exception("Base class should not be called")

    def single_step(self, index, state, signals, features):
        raise Exception("Base class should not be called")

    def get_output_dim(self, index):
        raise Exception("Base class should not be called")


class AdditiveSubconditioner(Subconditioner):
    def __init__(self,
                 number_of_subsamples,
                 pcm_embedding_size,
                 state_size,
                 pcm_levels,
                 number_of_signals,
                 **kwargs):
        """ subconditioning by addition """

        super(AdditiveSubconditioner, self).__init__()

        self.number_of_subsamples    = number_of_subsamples
        self.pcm_embedding_size      = pcm_embedding_size
        self.state_size              = state_size
        self.pcm_levels              = pcm_levels
        self.number_of_signals       = number_of_signals

        if self.pcm_embedding_size != self.state_size:
            raise ValueError('For additive subconditioning state and embedding '
            + f'sizes must match but but got {self.state_size} and {self.pcm_embedding_size}')

        self.embeddings = [None]
        for i in range(1, self.number_of_subsamples):
            embedding = nn.Embedding(self.pcm_levels, self.pcm_embedding_size)
            self.add_module('pcm_embedding_' + str(i), embedding)
            self.embeddings.append(embedding)

    def forward(self, states, signals):
        """ creates list of subconditioned states

            Parameters:
            -----------
            states : torch.tensor
                states of shape (batch, seq_length // s, state_size)
            signals : torch.tensor
                signals of shape (batch, seq_length, number_of_signals)

            Returns:
            --------
            c_states : list of torch.tensor
                list of s subconditioned states
        """

        s = self.number_of_subsamples

        c_states = [states]
        new_states = states
        for i in range(1, self.number_of_subsamples):
            embed = self.embeddings[i](signals[:, i::s])
            # reduce signal dimension
            embed = torch.sum(embed, dim=2)

            new_states = new_states + embed
            c_states.append(new_states)

        return c_states

    def single_step(self, index, state, signals):
        """ carry out single step for inference

            Parameters:
            -----------
            index : int
                position in subconditioning batch

            state : torch.tensor
                state to sub-condition

            signals : torch.tensor
                signals for subconditioning, all but the last dimensions
                must match those of state

            Returns:
            c_state : torch.tensor
                subconditioned state
        """

        if index == 0:
            c_state = state
        else:
            embed_signals = self.embeddings[index](signals)
            c = torch.sum(embed_signals, dim=-2)
            c_state = state + c

        return c_state

    def get_output_dim(self, index):
        return self.state_size

    def get_average_flops_per_step(self):
        s = self.number_of_subsamples
        flops = (s - 1) / s * self.number_of_signals * self.pcm_embedding_size
        return flops


class ConcatenativeSubconditioner(Subconditioner):
    def __init__(self,
                 number_of_subsamples,
                 pcm_embedding_size,
                 state_size,
                 pcm_levels,
                 number_of_signals,
                 recurrent=True,
                 **kwargs):
        """ subconditioning by concatenation """

        super(ConcatenativeSubconditioner, self).__init__()

        self.number_of_subsamples    = number_of_subsamples
        self.pcm_embedding_size      = pcm_embedding_size
        self.state_size              = state_size
        self.pcm_levels              = pcm_levels
        self.number_of_signals       = number_of_signals
        self.recurrent               = recurrent

        self.embeddings = []
        start_index = 0
        if self.recurrent:
            start_index = 1
            self.embeddings.append(None)

        for i in range(start_index, self.number_of_subsamples):
            embedding = nn.Embedding(self.pcm_levels, self.pcm_embedding_size)
            self.add_module('pcm_embedding_' + str(i), embedding)
            self.embeddings.append(embedding)

    def forward(self, states, signals):
        """ creates list of subconditioned states

            Parameters:
            -----------
            states : torch.tensor
                states of shape (batch, seq_length // s, state_size)
            signals : torch.tensor
                signals of shape (batch, seq_length, number_of_signals)

            Returns:
            --------
            c_states : list of torch.tensor
                list of s subconditioned states
        """
        s = self.number_of_subsamples

        if self.recurrent:
            c_states = [states]
            start = 1
        else:
            c_states = []
            start = 0

        new_states = states
        for i in range(start, self.number_of_subsamples):
            embed = self.embeddings[i](signals[:, i::s])
            # reduce signal dimension
            embed = torch.flatten(embed, -2)

            if self.recurrent:
                new_states = torch.cat((new_states, embed), dim=-1)
            else:
                new_states = torch.cat((states, embed), dim=-1)

            c_states.append(new_states)

        return c_states

    def single_step(self, index, state, signals):
        """ carry out single step for inference

            Parameters:
            -----------
            index : int
                position in subconditioning batch

            state : torch.tensor
                state to sub-condition

            signals : torch.tensor
                signals for subconditioning, all but the last dimensions
                must match those of state

            Returns:
            c_state : torch.tensor
                subconditioned state
        """

        if index == 0 and self.recurrent:
            c_state = state
        else:
            embed_signals = self.embeddings[index](signals)
            c = torch.flatten(embed_signals, -2)
            if not self.recurrent and index > 0:
                # overwrite previous conditioning vector
                c_state = torch.cat((state[...,:self.state_size], c), dim=-1)
            else:
                c_state = torch.cat((state, c), dim=-1)
            return c_state

        return c_state

    def get_average_flops_per_step(self):
        return 0

    def get_output_dim(self, index):
        if self.recurrent:
            return self.state_size + index * self.pcm_embedding_size * self.number_of_signals
        else:
            return self.state_size + self.pcm_embedding_size * self.number_of_signals

class ModulativeSubconditioner(Subconditioner):
    def __init__(self,
                 number_of_subsamples,
                 pcm_embedding_size,
                 state_size,
                 pcm_levels,
                 number_of_signals,
                 state_recurrent=False,
                 **kwargs):
        """ subconditioning by modulation """

        super(ModulativeSubconditioner, self).__init__()

        self.number_of_subsamples    = number_of_subsamples
        self.pcm_embedding_size      = pcm_embedding_size
        self.state_size              = state_size
        self.pcm_levels              = pcm_levels
        self.number_of_signals       = number_of_signals
        self.state_recurrent         = state_recurrent

        self.hidden_size = self.pcm_embedding_size * self.number_of_signals

        if self.state_recurrent:
            self.hidden_size += self.pcm_embedding_size
            self.state_transform = nn.Linear(self.state_size, self.pcm_embedding_size)

        self.embeddings = [None]
        self.alphas     = [None]
        self.betas      = [None]

        for i in range(1, self.number_of_subsamples):
            embedding = nn.Embedding(self.pcm_levels, self.pcm_embedding_size)
            self.add_module('pcm_embedding_' + str(i), embedding)
            self.embeddings.append(embedding)

            self.alphas.append(nn.Linear(self.hidden_size, self.state_size))
            self.add_module('alpha_dense_' + str(i), self.alphas[-1])

            self.betas.append(nn.Linear(self.hidden_size, self.state_size))
            self.add_module('beta_dense_' + str(i), self.betas[-1])



    def forward(self, states, signals):
        """ creates list of subconditioned states

            Parameters:
            -----------
            states : torch.tensor
                states of shape (batch, seq_length // s, state_size)
            signals : torch.tensor
                signals of shape (batch, seq_length, number_of_signals)

            Returns:
            --------
            c_states : list of torch.tensor
                list of s subconditioned states
        """
        s = self.number_of_subsamples

        c_states = [states]
        new_states = states
        for i in range(1, self.number_of_subsamples):
            embed = self.embeddings[i](signals[:, i::s])
            # reduce signal dimension
            embed = torch.flatten(embed, -2)

            if self.state_recurrent:
                comp_states = self.state_transform(new_states)
                embed = torch.cat((embed, comp_states), dim=-1)

            alpha = torch.tanh(self.alphas[i](embed))
            beta  = torch.tanh(self.betas[i](embed))

            # new state obtained by modulating previous state
            new_states = torch.tanh((1 + alpha) * new_states + beta)

            c_states.append(new_states)

        return c_states

    def single_step(self, index, state, signals):
        """ carry out single step for inference

            Parameters:
            -----------
            index : int
                position in subconditioning batch

            state : torch.tensor
                state to sub-condition

            signals : torch.tensor
                signals for subconditioning, all but the last dimensions
                must match those of state

            Returns:
            c_state : torch.tensor
                subconditioned state
        """

        if index == 0:
            c_state = state
        else:
            embed_signals = self.embeddings[index](signals)
            c = torch.flatten(embed_signals, -2)
            if self.state_recurrent:
                r_state = self.state_transform(state)
                c = torch.cat((c, r_state), dim=-1)
            alpha = torch.tanh(self.alphas[index](c))
            beta = torch.tanh(self.betas[index](c))
            c_state = torch.tanh((1 + alpha) * state + beta)
            return c_state

        return c_state

    def get_output_dim(self, index):
        return self.state_size

    def get_average_flops_per_step(self):
        s = self.number_of_subsamples

        # estimate activation by 10 flops
        # c_state = torch.tanh((1 + alpha) * state + beta)
        flops = 13 * self.state_size

        # hidden size
        hidden_size = self.number_of_signals * self.pcm_embedding_size
        if self.state_recurrent:
            hidden_size += self.pcm_embedding_size

        # counting 2 * A * B flops for Linear(A, B)
        # alpha = torch.tanh(self.alphas[index](c))
        # beta = torch.tanh(self.betas[index](c))
        flops += 4 * hidden_size * self.state_size + 20 * self.state_size

        # r_state = self.state_transform(state)
        if self.state_recurrent:
            flops += 2 * self.state_size * self.pcm_embedding_size

        # average over steps
        flops *= (s - 1) / s

        return flops

class ComparitiveSubconditioner(Subconditioner):
    def __init__(self,
                 number_of_subsamples,
                 pcm_embedding_size,
                 state_size,
                 pcm_levels,
                 number_of_signals,
                 error_index=-1,
                 apply_gate=True,
                 normalize=False):
        """ subconditioning by comparison """

        super(ComparitiveSubconditioner, self).__init__()

        self.comparison_size = self.pcm_embedding_size
        self.error_position  = error_index
        self.apply_gate      = apply_gate
        self.normalize       = normalize

        self.state_transform = nn.Linear(self.state_size, self.comparison_size)

        self.alpha_dense     = nn.Linear(self.number_of_signales * self.pcm_embedding_size, self.state_size)
        self.beta_dense      = nn.Linear(self.number_of_signales * self.pcm_embedding_size, self.state_size)

        if self.apply_gate:
            self.gate_dense      = nn.Linear(self.pcm_embedding_size, self.state_size)

        # embeddings and state transforms
        self.embeddings   = [None]
        self.alpha_denses = [None]
        self.beta_denses  = [None]
        self.state_transforms = [nn.Linear(self.state_size, self.comparison_size)]
        self.add_module('state_transform_0', self.state_transforms[0])

        for i in range(1, self.number_of_subsamples):
            embedding = nn.Embedding(self.pcm_levels, self.pcm_embedding_size)
            self.add_module('pcm_embedding_' + str(i), embedding)
            self.embeddings.append(embedding)

            state_transform = nn.Linear(self.state_size, self.comparison_size)
            self.add_module('state_transform_' + str(i), state_transform)
            self.state_transforms.append(state_transform)

            self.alpha_denses.append(nn.Linear(self.number_of_signales * self.pcm_embedding_size, self.state_size))
            self.add_module('alpha_dense_' + str(i), self.alpha_denses[-1])

            self.beta_denses.append(nn.Linear(self.number_of_signales * self.pcm_embedding_size, self.state_size))
            self.add_module('beta_dense_' + str(i), self.beta_denses[-1])

    def forward(self, states, signals):
        s = self.number_of_subsamples

        c_states = [states]
        new_states = states
        for i in range(1, self.number_of_subsamples):
            embed = self.embeddings[i](signals[:, i::s])
            # reduce signal dimension
            embed = torch.flatten(embed, -2)

            comp_states = self.state_transforms[i](new_states)

            alpha = torch.tanh(self.alpha_dense(embed))
            beta  = torch.tanh(self.beta_dense(embed))

            # new state obtained by modulating previous state
            new_states = torch.tanh((1 + alpha) * comp_states + beta)

            c_states.append(new_states)

        return c_states

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

class MetaCritic():
    def __init__(self, normalize=False, gamma=0.9, beta=0.0, joint_stats=False):
        """ Class for assessing relevance of discriminator scores

        Args:
            gamma (float, optional): update rate for tracking discriminator stats. Defaults to 0.9.
            beta (float, optional): Miminum confidence related threshold. Defaults to 0.0.
        """
        self.normalize = normalize
        self.gamma = gamma
        self.beta = beta
        self.joint_stats = joint_stats

        self.disc_stats = dict()

    def __call__(self, disc_id, real_scores, generated_scores):
        """ calculates relevance from normalized scores

        Args:
            disc_id (any valid key): id for tracking discriminator statistics
            real_scores (torch.tensor): scores for real data
            generated_scores (torch.tensor): scores for generated data; expecting device to match real_scores.device

        Returns:
            torch.tensor: output-domain relevance
        """

        if self.normalize:
            real_std = torch.std(real_scores.detach()).cpu().item()
            gen_std  = torch.std(generated_scores.detach()).cpu().item()
            std = (real_std**2 + gen_std**2) ** .5
            mean = torch.mean(real_scores.detach()).cpu().item() - torch.mean(generated_scores.detach()).cpu().item()

            key = 0 if self.joint_stats else disc_id

            if key in self.disc_stats:
                self.disc_stats[key]['std'] =  self.gamma * self.disc_stats[key]['std'] + (1 - self.gamma) * std
                self.disc_stats[key]['mean'] =  self.gamma * self.disc_stats[key]['mean'] + (1 - self.gamma) * mean
            else:
                self.disc_stats[key] = {
                    'std': std + 1e-5,
                    'mean': mean
                }

            std = self.disc_stats[key]['std']
            mean = self.disc_stats[key]['mean']
        else:
            mean, std = 0, 1

        relevance = torch.relu((real_scores - generated_scores - mean) / std + mean - self.beta)

        if False: print(f"relevance({disc_id}): {relevance.min()=} {relevance.max()=} {relevance.mean()=}")

        return relevance
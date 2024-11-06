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

from .common import sparsify_matrix


class GRUSparsifier:
    def __init__(self, task_list, start, stop, interval, exponent=3):
        """ Sparsifier for torch.nn.GRUs

            Parameters:
            -----------
            task_list : list
                task_list contains a list of tuples (gru, sparsify_dict), where gru is an instance
                of torch.nn.GRU and sparsify_dic is a dictionary with keys in {'W_ir', 'W_iz', 'W_in',
                'W_hr', 'W_hz', 'W_hn'} corresponding to the input and recurrent weights for the reset,
                update, and new gate. The values of sparsify_dict are tuples (density, [m, n], keep_diagonal),
                where density is the target density in [0, 1], [m, n] is the shape sub-blocks to which
                sparsification is applied and keep_diagonal is a bool variable indicating whether the diagonal
                should be kept.

            start : int
                training step after which sparsification will be started.

            stop : int
                training step after which sparsification will be completed.

            interval : int
                sparsification interval for steps between start and stop. After stop sparsification will be
                carried out after every call to GRUSparsifier.step()

            exponent : float
                Interpolation exponent for sparsification interval. In step i sparsification will be carried out
                with density (alpha + target_density * (1 * alpha)), where
                alpha = ((stop - i) / (start - stop)) ** exponent

            Example:
            --------
            >>> import torch
            >>> gru = torch.nn.GRU(10, 20)
            >>> sparsify_dict = {
            ...         'W_ir' : (0.5, [2, 2], False),
            ...         'W_iz' : (0.6, [2, 2], False),
            ...         'W_in' : (0.7, [2, 2], False),
            ...         'W_hr' : (0.1, [4, 4], True),
            ...         'W_hz' : (0.2, [4, 4], True),
            ...         'W_hn' : (0.3, [4, 4], True),
            ...     }
            >>> sparsifier = GRUSparsifier([(gru, sparsify_dict)], 0, 100, 50)
            >>> for i in range(100):
            ...         sparsifier.step()
        """
        # just copying parameters...
        self.start      = start
        self.stop       = stop
        self.interval   = interval
        self.exponent   = exponent
        self.task_list  = task_list

        # ... and setting counter to 0
        self.step_counter = 0

        self.last_masks = {key : None for key in ['W_ir', 'W_in', 'W_iz', 'W_hr', 'W_hn', 'W_hz']}

    def step(self, verbose=False):
        """ carries out sparsification step

            Call this function after optimizer.step in your
            training loop.

            Parameters:
            ----------
            verbose : bool
                if true, densities are printed out

            Returns:
            --------
            None

        """
        # compute current interpolation factor
        self.step_counter += 1

        if self.step_counter < self.start:
            return
        elif self.step_counter < self.stop:
            # update only every self.interval-th interval
            if self.step_counter % self.interval:
                return

            alpha = ((self.stop - self.step_counter) / (self.stop - self.start)) ** self.exponent
        else:
            alpha = 0


        with torch.no_grad():
            for gru, params in self.task_list:
                hidden_size = gru.hidden_size

                # input weights
                for i, key in enumerate(['W_ir', 'W_iz', 'W_in']):
                    if key in params:
                        density = alpha + (1 - alpha) * params[key][0]
                        if verbose:
                            print(f"[{self.step_counter}]: {key} density: {density}")

                        gru.weight_ih_l0[i * hidden_size : (i+1) * hidden_size, : ], new_mask = sparsify_matrix(
                            gru.weight_ih_l0[i * hidden_size : (i + 1) * hidden_size, : ],
                            density, # density
                            params[key][1], # block_size
                            params[key][2], # keep_diagonal (might want to set this to False)
                            return_mask=True
                        )

                        if type(self.last_masks[key]) != type(None):
                            if not torch.all(self.last_masks[key] == new_mask) and self.step_counter > self.stop:
                                print(f"sparsification mask {key} changed for gru {gru}")

                        self.last_masks[key] = new_mask

                # recurrent weights
                for i, key in enumerate(['W_hr', 'W_hz', 'W_hn']):
                    if key in params:
                        density = alpha + (1 - alpha) * params[key][0]
                        if verbose:
                            print(f"[{self.step_counter}]: {key} density: {density}")
                        gru.weight_hh_l0[i * hidden_size : (i+1) * hidden_size, : ], new_mask = sparsify_matrix(
                            gru.weight_hh_l0[i * hidden_size : (i + 1) * hidden_size, : ],
                            density,
                            params[key][1], # block_size
                            params[key][2], # keep_diagonal (might want to set this to False)
                            return_mask=True
                        )

                        if type(self.last_masks[key]) != type(None):
                            if not torch.all(self.last_masks[key] == new_mask) and self.step_counter > self.stop:
                                print(f"sparsification mask {key} changed for gru {gru}")

                        self.last_masks[key] = new_mask



if __name__ == "__main__":
    print("Testing sparsifier")

    gru = torch.nn.GRU(10, 20)
    sparsify_dict = {
        'W_ir' : (0.5, [2, 2], False),
        'W_iz' : (0.6, [2, 2], False),
        'W_in' : (0.7, [2, 2], False),
        'W_hr' : (0.1, [4, 4], True),
        'W_hz' : (0.2, [4, 4], True),
        'W_hn' : (0.3, [4, 4], True),
    }

    sparsifier = GRUSparsifier([(gru, sparsify_dict)], 0, 100, 10)

    for i in range(100):
        sparsifier.step(verbose=True)

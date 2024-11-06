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
from tqdm import tqdm
import sys

def train_one_epoch(model, criterion, optimizer, dataloader, device, scheduler, log_interval=10):

    model.to(device)
    model.train()

    running_loss = 0
    previous_running_loss = 0

    # gru states
    gru_a_state = torch.zeros(1, dataloader.batch_size, model.gru_a_units, device=device).to(device)
    gru_b_state = torch.zeros(1, dataloader.batch_size, model.gru_b_units, device=device).to(device)
    gru_states = [gru_a_state, gru_b_state]

    with tqdm(dataloader, unit='batch', file=sys.stdout) as tepoch:

        for i, batch in enumerate(tepoch):

            # set gradients to zero
            optimizer.zero_grad()

            # zero out initial gru states
            gru_a_state.zero_()
            gru_b_state.zero_()

            # push batch to device
            for key in batch:
                batch[key] = batch[key].to(device)

            target = batch['target']

            # calculate model output
            output = model(batch['features'], batch['periods'], batch['signals'], gru_states)

            # calculate loss
            loss = criterion(output.permute(0, 2, 1), target)

            # calculate gradients
            loss.backward()

            # update weights
            optimizer.step()

            # update learning rate
            scheduler.step()

            # call sparsifier
            model.sparsify()

            # update running loss
            running_loss += float(loss.cpu())

            # update status bar
            if i % log_interval == 0:
                tepoch.set_postfix(running_loss=f"{running_loss/(i + 1):8.7f}", current_loss=f"{(running_loss - previous_running_loss)/log_interval:8.7f}")
                previous_running_loss = running_loss


    running_loss /= len(dataloader)

    return running_loss

def evaluate(model, criterion, dataloader, device, log_interval=10):

    model.to(device)
    model.eval()

    running_loss = 0
    previous_running_loss = 0

    # gru states
    gru_a_state = torch.zeros(1, dataloader.batch_size, model.gru_a_units, device=device).to(device)
    gru_b_state = torch.zeros(1, dataloader.batch_size, model.gru_b_units, device=device).to(device)
    gru_states = [gru_a_state, gru_b_state]

    with torch.no_grad():
        with tqdm(dataloader, unit='batch', file=sys.stdout) as tepoch:

            for i, batch in enumerate(tepoch):


                # zero out initial gru states
                gru_a_state.zero_()
                gru_b_state.zero_()

                # push batch to device
                for key in batch:
                    batch[key] = batch[key].to(device)

                target = batch['target']

                # calculate model output
                output = model(batch['features'], batch['periods'], batch['signals'], gru_states)

                # calculate loss
                loss = criterion(output.permute(0, 2, 1), target)

                # update running loss
                running_loss += float(loss.cpu())

                # update status bar
                if i % log_interval == 0:
                    tepoch.set_postfix(running_loss=f"{running_loss/(i + 1):8.7f}", current_loss=f"{(running_loss - previous_running_loss)/log_interval:8.7f}")
                    previous_running_loss = running_loss


        running_loss /= len(dataloader)

        return running_loss
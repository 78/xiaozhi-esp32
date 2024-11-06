#!/usr/bin/python3
'''Copyright (c) 2021-2022 Amazon

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
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
'''

import numpy as np
from tensorflow.keras.utils import Sequence

class PLCLoader(Sequence):
    def __init__(self, features, lost, nb_burg_features, batch_size):
        self.batch_size = batch_size
        self.nb_batches = features.shape[0]//self.batch_size
        self.features = features[:self.nb_batches*self.batch_size, :, :]
        self.lost = lost.astype('float')
        self.lost = self.lost[:(len(self.lost)//features.shape[1]-1)*features.shape[1]]
        self.nb_burg_features = nb_burg_features
        self.on_epoch_end()

    def on_epoch_end(self):
        self.indices = np.arange(self.nb_batches*self.batch_size)
        np.random.shuffle(self.indices)
        offset = np.random.randint(0, high=self.features.shape[1])
        self.lost_offset = np.reshape(self.lost[offset:-self.features.shape[1]+offset], (-1, self.features.shape[1]))
        self.lost_indices = np.random.randint(0, high=self.lost_offset.shape[0], size=self.nb_batches*self.batch_size)

    def __getitem__(self, index):
        features = self.features[self.indices[index*self.batch_size:(index+1)*self.batch_size], :, :]
        burg_lost = (np.random.rand(features.shape[0], features.shape[1]) > .1).astype('float')
        burg_lost = np.reshape(burg_lost, (features.shape[0], features.shape[1], 1))
        burg_mask = np.tile(burg_lost, (1,1,self.nb_burg_features))

        lost = self.lost_offset[self.lost_indices[index*self.batch_size:(index+1)*self.batch_size], :]
        lost = np.reshape(lost, (features.shape[0], features.shape[1], 1))
        lost_mask = np.tile(lost, (1,1,features.shape[2]))
        in_features = features*lost_mask
        in_features[:,:,:self.nb_burg_features] = in_features[:,:,:self.nb_burg_features]*burg_mask

        #For the first frame after a loss, we don't have valid features, but the Burg estimate is valid.
        #in_features[:,1:,self.nb_burg_features:] = in_features[:,1:,self.nb_burg_features:]*lost_mask[:,:-1,self.nb_burg_features:]
        out_lost = np.copy(lost)
        #out_lost[:,1:,:] = out_lost[:,1:,:]*out_lost[:,:-1,:]

        out_features = np.concatenate([features[:,:,self.nb_burg_features:], 1.-out_lost], axis=-1)
        burg_sign = 2*burg_lost - 1
        # last dim is 1 for received packet, 0 for lost packet, and -1 when just the Burg info is missing
        inputs = [in_features*lost_mask, lost*burg_sign]
        outputs = [out_features]
        return (inputs, outputs)

    def __len__(self):
        return self.nb_batches

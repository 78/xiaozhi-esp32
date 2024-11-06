import torch
import numpy as np
import fargan

class FARGANDataset(torch.utils.data.Dataset):
    def __init__(self,
                feature_file,
                signal_file,
                frame_size=160,
                sequence_length=15,
                lookahead=1,
                nb_used_features=20,
                nb_features=36):

        self.frame_size = frame_size
        self.sequence_length = sequence_length
        self.lookahead = lookahead
        self.nb_features = nb_features
        self.nb_used_features = nb_used_features
        pcm_chunk_size = self.frame_size*self.sequence_length

        self.data = np.memmap(signal_file, dtype='int16', mode='r')
        #self.data = self.data[1::2]
        self.nb_sequences = len(self.data)//(pcm_chunk_size)-4
        self.data = self.data[(4-self.lookahead)*self.frame_size:]
        self.data = self.data[:self.nb_sequences*pcm_chunk_size]


        #self.data = np.reshape(self.data, (self.nb_sequences, pcm_chunk_size))
        sizeof = self.data.strides[-1]
        self.data = np.lib.stride_tricks.as_strided(self.data, shape=(self.nb_sequences, pcm_chunk_size*2),
                                           strides=(pcm_chunk_size*sizeof, sizeof))

        self.features = np.reshape(np.memmap(feature_file, dtype='float32', mode='r'), (-1, nb_features))
        sizeof = self.features.strides[-1]
        self.features = np.lib.stride_tricks.as_strided(self.features, shape=(self.nb_sequences, self.sequence_length*2+4, nb_features),
                                           strides=(self.sequence_length*self.nb_features*sizeof, self.nb_features*sizeof, sizeof))
        #self.periods = np.round(50*self.features[:,:,self.nb_used_features-2]+100).astype('int')
        self.periods = np.round(np.clip(256./2**(self.features[:,:,self.nb_used_features-2]+1.5), 32, 255)).astype('int')

        self.lpc = self.features[:, :, self.nb_used_features:]
        self.features = self.features[:, :, :self.nb_used_features]
        print("lpc_size:", self.lpc.shape)

    def __len__(self):
        return self.nb_sequences

    def __getitem__(self, index):
        features = self.features[index, :, :].copy()
        if self.lookahead != 0:
            lpc = self.lpc[index, 4-self.lookahead:-self.lookahead, :].copy()
        else:
            lpc = self.lpc[index, 4:, :].copy()
        data = self.data[index, :].copy().astype(np.float32) / 2**15
        periods = self.periods[index, :].copy()
        #lpc = lpc*(self.gamma**np.arange(1,17))
        #lpc=lpc[None,:,:]
        #lpc = fargan.interp_lpc(lpc, 4)
        #lpc=lpc[0,:,:]

        return features, periods, data, lpc

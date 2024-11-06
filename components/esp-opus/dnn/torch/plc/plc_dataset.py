import torch
import numpy as np

class PLCDataset(torch.utils.data.Dataset):
    def __init__(self,
                feature_file,
                loss_file,
                sequence_length=1000,
                nb_features=20,
                nb_burg_features=36,
                lpc_order=16):

        self.features_in = nb_features + nb_burg_features
        self.nb_burg_features = nb_burg_features
        total_features = self.features_in + lpc_order
        self.sequence_length = sequence_length
        self.nb_features = nb_features

        self.features = np.memmap(feature_file, dtype='float32', mode='r')
        self.lost = np.memmap(loss_file, dtype='int8', mode='r')
        self.lost = self.lost.astype('float32')

        self.nb_sequences = self.features.shape[0]//self.sequence_length//total_features

        self.features = self.features[:self.nb_sequences*self.sequence_length*total_features]
        self.features = self.features.reshape((self.nb_sequences, self.sequence_length, total_features))
        self.features = self.features[:,:,:self.features_in]

        #self.lost = self.lost[:(len(self.lost)//features.shape[1]-1)*features.shape[1]]
        #self.lost = self.lost.reshape((-1, self.sequence_length))

    def __len__(self):
        return self.nb_sequences

    def __getitem__(self, index):
        features = self.features[index, :, :]
        burg_lost = (np.random.rand(features.shape[0]) > .1).astype('float32')
        burg_lost = np.reshape(burg_lost, (features.shape[0], 1))
        burg_mask = np.tile(burg_lost, (1,self.nb_burg_features))

        lost_offset = np.random.randint(0, high=self.lost.shape[0]-self.sequence_length)
        lost = self.lost[lost_offset:lost_offset+self.sequence_length]
        #randomly add a few 10-ms losses so that the model learns to handle them
        lost = lost * (np.random.rand(lost.shape[-1]) > .02).astype('float32')
        #randomly break long consecutive losses so we don't try too hard to predict them
        lost = 1 - ((1-lost) * (np.random.rand(lost.shape[-1]) > .1).astype('float32'))
        lost = np.reshape(lost, (features.shape[0], 1))
        lost_mask = np.tile(lost, (1,features.shape[-1]))
        in_features = features*lost_mask
        in_features[:,:self.nb_burg_features] = in_features[:,:self.nb_burg_features]*burg_mask

        #For the first frame after a loss, we don't have valid features, but the Burg estimate is valid.
        #in_features[:,1:,self.nb_burg_features:] = in_features[:,1:,self.nb_burg_features:]*lost_mask[:,:-1,self.nb_burg_features:]
        out_lost = np.copy(lost)
        #out_lost[:,1:,:] = out_lost[:,1:,:]*out_lost[:,:-1,:]

        out_features = np.concatenate([features[:,self.nb_burg_features:], 1.-out_lost], axis=-1)
        burg_sign = 2*burg_lost - 1
        # last dim is 1 for received packet, 0 for lost packet, and -1 when just the Burg info is missing
        return in_features*lost_mask, lost*burg_sign, out_features

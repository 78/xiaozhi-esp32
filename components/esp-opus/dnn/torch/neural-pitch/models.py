"""
Pitch Estimation Models and dataloaders
    - Classification Based (Input features, output logits)
"""

import torch
import numpy as np

class PitchDNNIF(torch.nn.Module):

    def __init__(self, input_dim=88, gru_dim=64, output_dim=192):
        super().__init__()

        self.activation = torch.nn.Tanh()
        self.initial = torch.nn.Linear(input_dim, gru_dim)
        self.hidden = torch.nn.Linear(gru_dim, gru_dim)
        self.gru = torch.nn.GRU(input_size=gru_dim, hidden_size=gru_dim, batch_first=True)
        self.upsample = torch.nn.Linear(gru_dim, output_dim)

    def forward(self, x):

        x = self.initial(x)
        x = self.activation(x)
        x = self.hidden(x)
        x = self.activation(x)
        x,_ = self.gru(x)
        x = self.upsample(x)
        x = self.activation(x)
        x = x.permute(0,2,1)

        return x

class PitchDNNXcorr(torch.nn.Module):

    def __init__(self, input_dim=90, gru_dim=64, output_dim=192):
        super().__init__()

        self.activation = torch.nn.Tanh()

        self.conv = torch.nn.Sequential(
            torch.nn.ZeroPad2d((2, 0, 1, 1)),
            torch.nn.Conv2d(1, 8, 3, bias=True),
            self.activation,
            torch.nn.ZeroPad2d((2,0,1,1)),
            torch.nn.Conv2d(8, 8, 3, bias=True),
            self.activation,
            torch.nn.ZeroPad2d((2,0,1,1)),
            torch.nn.Conv2d(8, 1, 3, bias=True),
            self.activation,
        )

        self.downsample = torch.nn.Sequential(
            torch.nn.Linear(input_dim, gru_dim),
            self.activation
        )
        self.GRU = torch.nn.GRU(input_size=gru_dim, hidden_size=gru_dim, num_layers=1, batch_first=True)
        self.upsample = torch.nn.Sequential(
            torch.nn.Linear(gru_dim,output_dim),
            self.activation
        )

    def forward(self, x):
        x = self.conv(x.unsqueeze(-1).permute(0,3,2,1)).squeeze(1)
        x,_ = self.GRU(self.downsample(x.permute(0,2,1)))
        x = self.upsample(x).permute(0,2,1)

        return x

class PitchDNN(torch.nn.Module):
    """
    Joint IF-xcorr
    1D CNN on IF, merge with xcorr, 2D CNN on merged + GRU
    """

    def __init__(self,input_IF_dim=88, input_xcorr_dim=224, gru_dim=64, output_dim=192):
        super().__init__()

        self.activation = torch.nn.Tanh()

        self.if_upsample = torch.nn.Sequential(
            torch.nn.Linear(input_IF_dim,64),
            self.activation,
            torch.nn.Linear(64,64),
            self.activation,
        )

        self.conv = torch.nn.Sequential(
            torch.nn.ZeroPad2d((2,0,1,1)),
            torch.nn.Conv2d(1, 4, 3, bias=True),
            self.activation,
            torch.nn.ZeroPad2d((2,0,1,1)),
            torch.nn.Conv2d(4, 1, 3, bias=True),
            self.activation,
        )

        self.downsample = torch.nn.Sequential(
            torch.nn.Linear(64 + input_xcorr_dim, gru_dim),
            self.activation
        )
        self.GRU = torch.nn.GRU(input_size=gru_dim, hidden_size=gru_dim, num_layers=1, batch_first=True)
        self.upsample = torch.nn.Sequential(
            torch.nn.Linear(gru_dim, output_dim)
        )

    def forward(self, x):
        xcorr_feat = x[:,:,:224]
        if_feat = x[:,:,224:]
        xcorr_feat = self.conv(xcorr_feat.unsqueeze(-1).permute(0,3,2,1)).squeeze(1).permute(0,2,1)
        if_feat = self.if_upsample(if_feat)
        x = torch.cat([xcorr_feat,if_feat],axis = - 1)
        x,_ = self.GRU(self.downsample(x))
        x = self.upsample(x).permute(0,2,1)

        return x


# Dataloaders
class Loader(torch.utils.data.Dataset):
      def __init__(self, features_if, file_pitch, confidence_threshold=0.4, dimension_if=30, context=100):
            self.if_feat = np.memmap(features_if, dtype=np.float32).reshape(-1,3*dimension_if)

            # Resolution of 20 cents
            self.cents = np.rint(np.load(file_pitch)[0,:]/20)
            self.cents = np.clip(self.cents,0,179)
            self.confidence = np.load(file_pitch)[1,:]

            # Filter confidence for CREPE
            self.confidence[self.confidence < confidence_threshold] = 0
            self.context = context
            # Clip both to same size
            size_common = min(self.if_feat.shape[0], self.cents.shape[0])
            self.if_feat = self.if_feat[:size_common,:]
            self.cents = self.cents[:size_common]
            self.confidence = self.confidence[:size_common]

            frame_max = self.if_feat.shape[0]//context
            self.if_feat = np.reshape(self.if_feat[:frame_max*context, :],(frame_max, context,3*dimension_if))
            self.cents = np.reshape(self.cents[:frame_max * context],(frame_max, context))
            self.confidence = np.reshape(self.confidence[:frame_max*context],(frame_max, context))

      def __len__(self):
            return self.if_feat.shape[0]

      def __getitem__(self, index):
            return torch.from_numpy(self.if_feat[index,:,:]), torch.from_numpy(self.cents[index]), torch.from_numpy(self.confidence[index])

class PitchDNNDataloader(torch.utils.data.Dataset):
      def __init__(self, features, file_pitch, confidence_threshold=0.4, context=100, choice_data='both'):
            self.feat = np.memmap(features, mode='r', dtype=np.int8).reshape(-1,312)
            self.xcorr = self.feat[:,:224]
            self.if_feat = self.feat[:,224:]
            ground_truth = np.memmap(file_pitch, mode='r', dtype=np.float32).reshape(-1,2)
            self.cents = np.rint(60*np.log2(ground_truth[:,0]/62.5))
            mask = (self.cents>=0).astype('float32') * (self.cents<=180).astype('float32')
            self.cents = np.clip(self.cents,0,179)
            self.confidence = ground_truth[:,1] * mask
            # Filter confidence for CREPE
            self.confidence[self.confidence < confidence_threshold] = 0
            self.context = context

            self.choice_data = choice_data

            frame_max = self.if_feat.shape[0]//context
            self.if_feat = np.reshape(self.if_feat[:frame_max*context,:], (frame_max, context, 88))
            self.cents = np.reshape(self.cents[:frame_max*context], (frame_max,context))
            self.xcorr = np.reshape(self.xcorr[:frame_max*context,:], (frame_max,context, 224))
            self.confidence = np.reshape(self.confidence[:frame_max*context], (frame_max, context))

      def __len__(self):
            return self.if_feat.shape[0]

      def __getitem__(self, index):
            if self.choice_data == 'both':
                return torch.cat([torch.from_numpy((1./127)*self.xcorr[index,:,:]), torch.from_numpy((1./127)*self.if_feat[index,:,:])], dim=-1), torch.from_numpy(self.cents[index]), torch.from_numpy(self.confidence[index])
            elif self.choice_data == 'if':
                return torch.from_numpy((1./127)*self.if_feat[index,:,:]),torch.from_numpy(self.cents[index]),torch.from_numpy(self.confidence[index])
            else:
                return torch.from_numpy((1./127)*self.xcorr[index,:,:]),torch.from_numpy(self.cents[index]),torch.from_numpy(self.confidence[index])

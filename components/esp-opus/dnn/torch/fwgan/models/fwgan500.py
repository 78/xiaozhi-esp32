import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.nn.utils import weight_norm
import numpy as np


which_norm = weight_norm

#################### Definition of basic model components ####################

#Convolutional layer with 1 frame look-ahead (used for feature PreCondNet)
class ConvLookahead(nn.Module):
    def __init__(self, in_ch, out_ch, kernel_size, dilation=1, groups=1, bias= False):
        super(ConvLookahead, self).__init__()
        torch.manual_seed(5)

        self.padding_left = (kernel_size - 2) * dilation
        self.padding_right = 1 * dilation

        self.conv = which_norm(nn.Conv1d(in_ch,out_ch,kernel_size,dilation=dilation, groups=groups, bias= bias))

        self.init_weights()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d) or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x):

        x = F.pad(x,(self.padding_left, self.padding_right))
        conv_out = self.conv(x)
        return conv_out

#(modified) GLU Activation layer definition
class GLU(nn.Module):
    def __init__(self, feat_size):
        super(GLU, self).__init__()

        torch.manual_seed(5)

        self.gate = which_norm(nn.Linear(feat_size, feat_size, bias=False))

        self.init_weights()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d)\
            or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x):

        out = torch.tanh(x) * torch.sigmoid(self.gate(x))

        return out

#GRU layer definition
class ContForwardGRU(nn.Module):
    def __init__(self, input_size, hidden_size, num_layers=1):
        super(ContForwardGRU, self).__init__()

        torch.manual_seed(5)

        self.hidden_size = hidden_size

        #This is to initialize the layer with history audio samples for continuation.
        self.cont_fc = nn.Sequential(which_norm(nn.Linear(320, self.hidden_size, bias=False)),
                        nn.Tanh())

        self.gru = nn.GRU(input_size=input_size, hidden_size=hidden_size, num_layers=num_layers, batch_first=True,\
                          bias=False)

        self.nl = GLU(self.hidden_size)

        self.init_weights()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d) or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x, x0):

        self.gru.flatten_parameters()

        h0 = self.cont_fc(x0).unsqueeze(0)

        output, h0 = self.gru(x, h0)

        return self.nl(output)

# Framewise convolution layer definition
class ContFramewiseConv(torch.nn.Module):

    def __init__(self, frame_len, out_dim, frame_kernel_size=3, act='glu', causal=True):

        super(ContFramewiseConv, self).__init__()
        torch.manual_seed(5)

        self.frame_kernel_size = frame_kernel_size
        self.frame_len = frame_len

        if (causal == True) or (self.frame_kernel_size == 2):

            self.required_pad_left = (self.frame_kernel_size - 1) * self.frame_len
            self.required_pad_right = 0

            #This is to initialize the layer with history audio samples for continuation.
            self.cont_fc = nn.Sequential(which_norm(nn.Linear(320, self.required_pad_left, bias=False)),
                                    nn.Tanh()
                                   )

        else:
            #This means non-causal frame-wise convolution. We don't use it at the moment
            self.required_pad_left = (self.frame_kernel_size - 1)//2 * self.frame_len
            self.required_pad_right = (self.frame_kernel_size - 1)//2 * self.frame_len

        self.fc_input_dim = self.frame_kernel_size * self.frame_len
        self.fc_out_dim = out_dim

        if act=='glu':
            self.fc = nn.Sequential(which_norm(nn.Linear(self.fc_input_dim, self.fc_out_dim, bias=False)),
                                    GLU(self.fc_out_dim)
                                   )
        if act=='tanh':
            self.fc = nn.Sequential(which_norm(nn.Linear(self.fc_input_dim, self.fc_out_dim, bias=False)),
                                    nn.Tanh()
                                   )

        self.init_weights()


    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d) or isinstance(m, nn.Linear) or\
            isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x, x0):

        if self.frame_kernel_size == 1:
            return self.fc(x)

        x_flat = x.reshape(x.size(0),1,-1)
        pad = self.cont_fc(x0).view(x0.size(0),1,-1)
        x_flat_padded = torch.cat((pad, x_flat), dim=-1).unsqueeze(2)

        x_flat_padded_unfolded = F.unfold(x_flat_padded,\
                    kernel_size= (1,self.fc_input_dim), stride=self.frame_len).permute(0,2,1).contiguous()

        out = self.fc(x_flat_padded_unfolded)
        return out

########################### The complete model definition #################################

class FWGAN500Cont(nn.Module):
    def __init__(self):
        super().__init__()
        torch.manual_seed(5)

        #PrecondNet:
        self.bfcc_with_corr_upsampler = nn.Sequential(nn.ConvTranspose1d(19,64,kernel_size=5,stride=5,padding=0,\
                                                      bias=False),
                                                      nn.Tanh())

        self.feat_in_conv = ConvLookahead(128,256,kernel_size=5)
        self.feat_in_nl = GLU(256)

        #GRU:
        self.rnn = ContForwardGRU(256,256)

        #Frame-wise convolution stack:
        self.fwc1 = ContFramewiseConv(256, 256)
        self.fwc2 = ContFramewiseConv(256, 128)
        self.fwc3 = ContFramewiseConv(128, 128)
        self.fwc4 = ContFramewiseConv(128, 64)
        self.fwc5 = ContFramewiseConv(64, 64)
        self.fwc6 = ContFramewiseConv(64, 32)
        self.fwc7 = ContFramewiseConv(32, 32, act='tanh')

        self.init_weights()
        self.count_parameters()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d) or isinstance(m, nn.Linear) or\
            isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def count_parameters(self):
        num_params =  sum(p.numel() for p in self.parameters() if p.requires_grad)
        print(f"Total number of {self.__class__.__name__} network parameters = {num_params}\n")

    def create_phase_signals(self, periods):

        batch_size = periods.size(0)
        progression = torch.arange(1, 160 + 1, dtype=periods.dtype, device=periods.device).view((1, -1))
        progression = torch.repeat_interleave(progression, batch_size, 0)

        phase0 = torch.zeros(batch_size, dtype=periods.dtype, device=periods.device).unsqueeze(-1)
        chunks = []
        for sframe in range(periods.size(1)):
            f = (2.0 * torch.pi / periods[:, sframe]).unsqueeze(-1)

            chunk_sin = torch.sin(f  * progression + phase0)
            chunk_sin = chunk_sin.reshape(chunk_sin.size(0),-1,32)

            chunk_cos = torch.cos(f  * progression + phase0)
            chunk_cos = chunk_cos.reshape(chunk_cos.size(0),-1,32)

            chunk = torch.cat((chunk_sin, chunk_cos), dim = -1)

            phase0 = phase0 + 160 * f

            chunks.append(chunk)

        phase_signals = torch.cat(chunks, dim=1)

        return phase_signals


    def gain_multiply(self, x, c0):

        gain = 10**(0.5*c0/np.sqrt(18.0))
        gain = torch.repeat_interleave(gain, 160, dim=-1)
        gain  = gain.reshape(gain.size(0),1,-1).squeeze(1)

        return x * gain

    def forward(self, pitch_period, bfcc_with_corr, x0):

        #This should create a latent representation of shape [Batch_dim, 500 frames, 256 elemets per frame]
        p_embed = self.create_phase_signals(pitch_period).permute(0, 2, 1).contiguous()
        envelope = self.bfcc_with_corr_upsampler(bfcc_with_corr.permute(0,2,1).contiguous())
        feat_in = torch.cat((p_embed , envelope), dim=1)
        wav_latent = self.feat_in_nl(self.feat_in_conv(feat_in).permute(0,2,1).contiguous())

        #Generation with continuation using history samples x0 starts from here:

        rnn_out = self.rnn(wav_latent, x0)

        fwc1_out = self.fwc1(rnn_out, x0)
        fwc2_out = self.fwc2(fwc1_out, x0)
        fwc3_out = self.fwc3(fwc2_out, x0)
        fwc4_out = self.fwc4(fwc3_out, x0)
        fwc5_out = self.fwc5(fwc4_out, x0)
        fwc6_out = self.fwc6(fwc5_out, x0)
        fwc7_out = self.fwc7(fwc6_out, x0)

        waveform_unscaled = fwc7_out.reshape(fwc7_out.size(0),1,-1).squeeze(1)
        waveform = self.gain_multiply(waveform_unscaled,bfcc_with_corr[:,:,:1])

        return  waveform

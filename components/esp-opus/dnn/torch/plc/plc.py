import numpy as np
import torch
from torch import nn
import torch.nn.functional as F
from torch.nn.utils import weight_norm
import math

fid_dict = {}
def dump_signal(x, filename):
    return
    if filename in fid_dict:
        fid = fid_dict[filename]
    else:
        fid = open(filename, "w")
        fid_dict[filename] = fid
    x = x.detach().numpy().astype('float32')
    x.tofile(fid)


class IDCT(nn.Module):
    def __init__(self, N, device=None):
        super(IDCT, self).__init__()

        self.N = N
        n = torch.arange(N, device=device)
        k = torch.arange(N, device=device)
        self.table = torch.cos(torch.pi/N * (n[:,None]+.5) * k[None,:])
        self.table[:,0] = self.table[:,0] * math.sqrt(.5)
        self.table = self.table / math.sqrt(N/2)

    def forward(self, x):
        return F.linear(x, self.table, None)

def plc_loss(N, device=None, alpha=1.0, bias=1.):
    idct = IDCT(18, device=device)
    def loss(y_true,y_pred):
        mask = y_true[:,:,-1:]
        y_true = y_true[:,:,:-1]
        e = (y_pred - y_true)*mask
        e_bands = idct(e[:,:,:-2])
        bias_mask = torch.clamp(4*y_true[:,:,-1:], min=0., max=1.)
        l1_loss = torch.mean(torch.abs(e))
        ceps_loss = torch.mean(torch.abs(e[:,:,:-2]))
        band_loss = torch.mean(torch.abs(e_bands))
        biased_loss = torch.mean(bias_mask*torch.clamp(e_bands, min=0.))
        pitch_loss1 = torch.mean(torch.clamp(torch.abs(e[:,:,18:19]),max=1.))
        pitch_loss = torch.mean(torch.clamp(torch.abs(e[:,:,18:19]),max=.4))
        voice_bias = torch.mean(torch.clamp(-e[:,:,-1:], min=0.))
        tot = l1_loss + 0.1*voice_bias + alpha*(band_loss + bias*biased_loss) + pitch_loss1 + 8*pitch_loss
        return tot, l1_loss, ceps_loss, band_loss, pitch_loss
    return loss


# weight initialization and clipping
def init_weights(module):
    if isinstance(module, nn.GRU):
        for p in module.named_parameters():
            if p[0].startswith('weight_hh_'):
                nn.init.orthogonal_(p[1])


class GLU(nn.Module):
    def __init__(self, feat_size):
        super(GLU, self).__init__()

        torch.manual_seed(5)

        self.gate = weight_norm(nn.Linear(feat_size, feat_size, bias=False))

        self.init_weights()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d)\
            or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x):

        out = x * torch.sigmoid(self.gate(x))

        return out

class FWConv(nn.Module):
    def __init__(self, in_size, out_size, kernel_size=2):
        super(FWConv, self).__init__()

        torch.manual_seed(5)

        self.in_size = in_size
        self.kernel_size = kernel_size
        self.conv = weight_norm(nn.Linear(in_size*self.kernel_size, out_size, bias=False))
        self.glu = GLU(out_size)

        self.init_weights()

    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d)\
            or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)

    def forward(self, x, state):
        xcat = torch.cat((state, x), -1)
        out = self.glu(torch.tanh(self.conv(xcat)))
        return out, xcat[:,self.in_size:]

def n(x):
    return torch.clamp(x + (1./127.)*(torch.rand_like(x)-.5), min=-1., max=1.)

class PLC(nn.Module):
    def __init__(self, features_in=57, features_out=20, cond_size=128, gru_size=128):
        super(PLC, self).__init__()

        self.features_in = features_in
        self.features_out = features_out
        self.cond_size = cond_size
        self.gru_size = gru_size

        self.dense_in = nn.Linear(self.features_in, self.cond_size)
        self.gru1 = nn.GRU(self.cond_size, self.gru_size, batch_first=True)
        self.gru2 = nn.GRU(self.gru_size, self.gru_size, batch_first=True)
        self.dense_out = nn.Linear(self.gru_size, features_out)

        self.apply(init_weights)
        nb_params = sum(p.numel() for p in self.parameters())
        print(f"plc model: {nb_params} weights")

    def forward(self, features, lost, states=None):
        device = features.device
        batch_size = features.size(0)
        if states is None:
            gru1_state = torch.zeros((1, batch_size, self.gru_size), device=device)
            gru2_state = torch.zeros((1, batch_size, self.gru_size), device=device)
        else:
            gru1_state = states[0]
            gru2_state = states[1]
        x = torch.cat([features, lost], dim=-1)
        x = torch.tanh(self.dense_in(x))
        gru1_out, gru1_state = self.gru1(x, gru1_state)
        gru2_out, gru2_state = self.gru2(gru1_out, gru2_state)
        return self.dense_out(gru2_out), [gru1_state, gru2_state]

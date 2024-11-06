import numpy as np
import torch
from torch import nn
import torch.nn.functional as F
import filters
from torch.nn.utils import weight_norm
#from convert_lsp import lpc_to_lsp, lsp_to_lpc
from rc import lpc2rc, rc2lpc

Fs = 16000

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


def sig_l1(y_true, y_pred):
    return torch.mean(abs(y_true-y_pred))/torch.mean(abs(y_true))

def sig_loss(y_true, y_pred):
    t = y_true/(1e-15+torch.norm(y_true, dim=-1, p=2, keepdim=True))
    p = y_pred/(1e-15+torch.norm(y_pred, dim=-1, p=2, keepdim=True))
    return torch.mean(1.-torch.sum(p*t, dim=-1))

def interp_lpc(lpc, factor):
    #print(lpc.shape)
    #f = (np.arange(factor)+.5*((factor+1)%2))/factor
    lsp = torch.atanh(lpc2rc(lpc))
    #print("lsp0:")
    #print(lsp)
    shape = lsp.shape
    #print("shape is", shape)
    shape = (shape[0], shape[1]*factor, shape[2])
    interp_lsp = torch.zeros(shape, device=lpc.device)
    for k in range(factor):
        f = (k+.5*((factor+1)%2))/factor
        interp = (1-f)*lsp[:,:-1,:] + f*lsp[:,1:,:]
        interp_lsp[:,factor//2+k:-(factor//2):factor,:] = interp
    for k in range(factor//2):
        interp_lsp[:,k,:] = interp_lsp[:,factor//2,:]
    for k in range((factor+1)//2):
        interp_lsp[:,-k-1,:] = interp_lsp[:,-(factor+3)//2,:]
    #print("lsp:")
    #print(interp_lsp)
    return rc2lpc(torch.tanh(interp_lsp))

def analysis_filter(x, lpc, nb_subframes=4, subframe_size=40, gamma=.9):
    device = x.device
    batch_size = lpc.size(0)

    nb_frames = lpc.shape[1]


    sig = torch.zeros(batch_size, subframe_size+16, device=device)
    x = torch.reshape(x, (batch_size, nb_frames*nb_subframes, subframe_size))
    out = torch.zeros((batch_size, 0), device=device)

    #if gamma is not None:
    #    bw = gamma**(torch.arange(1, 17, device=device))
    #    lpc = lpc*bw[None,None,:]
    ones = torch.ones((*(lpc.shape[:-1]), 1), device=device)
    zeros = torch.zeros((*(lpc.shape[:-1]), subframe_size-1), device=device)
    a = torch.cat([ones, lpc], -1)
    a_big = torch.cat([a, zeros], -1)
    fir_mat_big = filters.toeplitz_from_filter(a_big)

    #print(a_big[:,0,:])
    for n in range(nb_frames):
        for k in range(nb_subframes):

            sig = torch.cat([sig[:,subframe_size:], x[:,n*nb_subframes + k, :]], 1)
            exc = torch.bmm(fir_mat_big[:,n,:,:], sig[:,:,None])
            out = torch.cat([out, exc[:,-subframe_size:,0]], 1)

    return out


# weight initialization and clipping
def init_weights(module):
    if isinstance(module, nn.GRU):
        for p in module.named_parameters():
            if p[0].startswith('weight_hh_'):
                nn.init.orthogonal_(p[1])

def gen_phase_embedding(periods, frame_size):
    device = periods.device
    batch_size = periods.size(0)
    nb_frames = periods.size(1)
    w0 = 2*torch.pi/periods
    w0_shift = torch.cat([2*torch.pi*torch.rand((batch_size, 1), device=device)/frame_size, w0[:,:-1]], 1)
    cum_phase = frame_size*torch.cumsum(w0_shift, 1)
    fine_phase = w0[:,:,None]*torch.broadcast_to(torch.arange(frame_size, device=device), (batch_size, nb_frames, frame_size))
    embed = torch.unsqueeze(cum_phase, 2) + fine_phase
    embed = torch.reshape(embed, (batch_size, -1))
    return torch.cos(embed), torch.sin(embed)

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
        #print(x.shape, state.shape, xcat.shape, self.in_size, self.kernel_size)
        out = self.glu(torch.tanh(self.conv(xcat)))
        return out, xcat[:,self.in_size:]

def n(x):
    return torch.clamp(x + (1./127.)*(torch.rand_like(x)-.5), min=-1., max=1.)

class FARGANCond(nn.Module):
    def __init__(self, feature_dim=20, cond_size=256, pembed_dims=12):
        super(FARGANCond, self).__init__()

        self.feature_dim = feature_dim
        self.cond_size = cond_size

        self.pembed = nn.Embedding(224, pembed_dims)
        self.fdense1 = nn.Linear(self.feature_dim + pembed_dims, 64, bias=False)
        self.fconv1 = nn.Conv1d(64, 128, kernel_size=3, padding='valid', bias=False)
        self.fdense2 = nn.Linear(128, 80*4, bias=False)

        self.apply(init_weights)
        nb_params = sum(p.numel() for p in self.parameters())
        print(f"cond model: {nb_params} weights")

    def forward(self, features, period):
        features = features[:,2:,:]
        period = period[:,2:]
        p = self.pembed(period-32)
        features = torch.cat((features, p), -1)
        tmp = torch.tanh(self.fdense1(features))
        tmp = tmp.permute(0, 2, 1)
        tmp = torch.tanh(self.fconv1(tmp))
        tmp = tmp.permute(0, 2, 1)
        tmp = torch.tanh(self.fdense2(tmp))
        #tmp = torch.tanh(self.fdense2(tmp))
        return tmp

class FARGANSub(nn.Module):
    def __init__(self, subframe_size=40, nb_subframes=4, cond_size=256):
        super(FARGANSub, self).__init__()

        self.subframe_size = subframe_size
        self.nb_subframes = nb_subframes
        self.cond_size = cond_size
        self.cond_gain_dense = nn.Linear(80, 1)

        #self.sig_dense1 = nn.Linear(4*self.subframe_size+self.passthrough_size+self.cond_size, self.cond_size, bias=False)
        self.fwc0 = FWConv(2*self.subframe_size+80+4, 192)
        self.gru1 = nn.GRUCell(192+2*self.subframe_size, 160, bias=False)
        self.gru2 = nn.GRUCell(160+2*self.subframe_size, 128, bias=False)
        self.gru3 = nn.GRUCell(128+2*self.subframe_size, 128, bias=False)

        self.gru1_glu = GLU(160)
        self.gru2_glu = GLU(128)
        self.gru3_glu = GLU(128)
        self.skip_glu = GLU(128)
        #self.ptaps_dense = nn.Linear(4*self.cond_size, 5)

        self.skip_dense = nn.Linear(192+160+2*128+2*self.subframe_size, 128, bias=False)
        self.sig_dense_out = nn.Linear(128, self.subframe_size, bias=False)
        self.gain_dense_out = nn.Linear(192, 4)


        self.apply(init_weights)
        nb_params = sum(p.numel() for p in self.parameters())
        print(f"subframe model: {nb_params} weights")

    def forward(self, cond, prev_pred, exc_mem, period, states, gain=None):
        device = exc_mem.device
        #print(cond.shape, prev.shape)

        cond = n(cond)
        dump_signal(gain, 'gain0.f32')
        gain = torch.exp(self.cond_gain_dense(cond))
        dump_signal(gain, 'gain1.f32')
        idx = 256-period[:,None]
        rng = torch.arange(self.subframe_size+4, device=device)
        idx = idx + rng[None,:] - 2
        mask = idx >= 256
        idx = idx - mask*period[:,None]
        pred = torch.gather(exc_mem, 1, idx)
        pred = n(pred/(1e-5+gain))

        prev = exc_mem[:,-self.subframe_size:]
        dump_signal(prev, 'prev_in.f32')
        prev = n(prev/(1e-5+gain))
        dump_signal(prev, 'pitch_exc.f32')
        dump_signal(exc_mem, 'exc_mem.f32')

        tmp = torch.cat((cond, pred, prev), 1)
        #fpitch = taps[:,0:1]*pred[:,:-4] + taps[:,1:2]*pred[:,1:-3] + taps[:,2:3]*pred[:,2:-2] + taps[:,3:4]*pred[:,3:-1] + taps[:,4:]*pred[:,4:]
        fpitch = pred[:,2:-2]

        #tmp = self.dense1_glu(torch.tanh(self.sig_dense1(tmp)))
        fwc0_out, fwc0_state = self.fwc0(tmp, states[3])
        fwc0_out = n(fwc0_out)
        pitch_gain = torch.sigmoid(self.gain_dense_out(fwc0_out))

        gru1_state = self.gru1(torch.cat([fwc0_out, pitch_gain[:,0:1]*fpitch, prev], 1), states[0])
        gru1_out = self.gru1_glu(n(gru1_state))
        gru1_out = n(gru1_out)
        gru2_state = self.gru2(torch.cat([gru1_out, pitch_gain[:,1:2]*fpitch, prev], 1), states[1])
        gru2_out = self.gru2_glu(n(gru2_state))
        gru2_out = n(gru2_out)
        gru3_state = self.gru3(torch.cat([gru2_out, pitch_gain[:,2:3]*fpitch, prev], 1), states[2])
        gru3_out = self.gru3_glu(n(gru3_state))
        gru3_out = n(gru3_out)
        gru3_out = torch.cat([gru1_out, gru2_out, gru3_out, fwc0_out], 1)
        skip_out = torch.tanh(self.skip_dense(torch.cat([gru3_out, pitch_gain[:,3:4]*fpitch, prev], 1)))
        skip_out = self.skip_glu(n(skip_out))
        sig_out = torch.tanh(self.sig_dense_out(skip_out))
        dump_signal(sig_out, 'exc_out.f32')
        #taps = self.ptaps_dense(gru3_out)
        #taps = .2*taps + torch.exp(taps)
        #taps = taps / (1e-2 + torch.sum(torch.abs(taps), dim=-1, keepdim=True))
        #dump_signal(taps, 'taps.f32')

        dump_signal(pitch_gain, 'pgain.f32')
        #sig_out = (sig_out + pitch_gain*fpitch) * gain
        sig_out = sig_out * gain
        exc_mem = torch.cat([exc_mem[:,self.subframe_size:], sig_out], 1)
        prev_pred = torch.cat([prev_pred[:,self.subframe_size:], fpitch], 1)
        dump_signal(sig_out, 'sig_out.f32')
        return sig_out, exc_mem, prev_pred, (gru1_state, gru2_state, gru3_state, fwc0_state)

class FARGAN(nn.Module):
    def __init__(self, subframe_size=40, nb_subframes=4, feature_dim=20, cond_size=256, passthrough_size=0, has_gain=False, gamma=None):
        super(FARGAN, self).__init__()

        self.subframe_size = subframe_size
        self.nb_subframes = nb_subframes
        self.frame_size = self.subframe_size*self.nb_subframes
        self.feature_dim = feature_dim
        self.cond_size = cond_size

        self.cond_net = FARGANCond(feature_dim=feature_dim, cond_size=cond_size)
        self.sig_net = FARGANSub(subframe_size=subframe_size, nb_subframes=nb_subframes, cond_size=cond_size)

    def forward(self, features, period, nb_frames, pre=None, states=None):
        device = features.device
        batch_size = features.size(0)

        prev = torch.zeros(batch_size, 256, device=device)
        exc_mem = torch.zeros(batch_size, 256, device=device)
        nb_pre_frames = pre.size(1)//self.frame_size if pre is not None else 0

        states = (
            torch.zeros(batch_size, 160, device=device),
            torch.zeros(batch_size, 128, device=device),
            torch.zeros(batch_size, 128, device=device),
            torch.zeros(batch_size, (2*self.subframe_size+80+4)*1, device=device)
        )

        sig = torch.zeros((batch_size, 0), device=device)
        cond = self.cond_net(features, period)
        if pre is not None:
            exc_mem[:,-self.frame_size:] = pre[:, :self.frame_size]
        start = 1 if nb_pre_frames>0 else 0
        for n in range(start, nb_frames+nb_pre_frames):
            for k in range(self.nb_subframes):
                pos = n*self.frame_size + k*self.subframe_size
                #print("now: ", preal.shape, prev.shape, sig_in.shape)
                pitch = period[:, 3+n]
                gain = .03*10**(0.5*features[:, 3+n, 0:1]/np.sqrt(18.0))
                #gain = gain[:,:,None]
                out, exc_mem, prev, states = self.sig_net(cond[:, n, k*80:(k+1)*80], prev, exc_mem, pitch, states, gain=gain)

                if n < nb_pre_frames:
                    out = pre[:, pos:pos+self.subframe_size]
                    exc_mem[:,-self.subframe_size:] = out
                else:
                    sig = torch.cat([sig, out], 1)

        states = [s.detach() for s in states]
        return sig, states

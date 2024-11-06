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

import math as m
import copy

import torch
import torch.nn.functional as F
from torch import nn
from torch.nn.utils import weight_norm, spectral_norm
import torchaudio

from utils.spec import gen_filterbank

# auxiliary functions

def remove_all_weight_norms(module):
    for m in module.modules():
        if hasattr(m, 'weight_v'):
            nn.utils.remove_weight_norm(m)


def create_smoothing_kernel(h, w, gamma=1.5):

    ch = h / 2 - 0.5
    cw = w / 2 - 0.5

    sh = gamma * ch
    sw = gamma * cw

    vx = ((torch.arange(h) - ch) / sh) ** 2
    vy = ((torch.arange(w) - cw) / sw) ** 2
    vals = vx.view(-1, 1) + vy.view(1, -1)
    kernel = torch.exp(- vals)
    kernel = kernel / kernel.sum()

    return kernel


def create_kernel(h, w, sh, sw):
    # proto kernel gives disjoint partition of 1
    proto_kernel = torch.ones((sh, sw))

    # create smoothing kernel eta
    h_eta, w_eta = h - sh + 1, w - sw + 1
    assert h_eta > 0 and w_eta > 0
    eta = create_smoothing_kernel(h_eta, w_eta).view(1, 1, h_eta, w_eta)

    kernel0 = F.pad(proto_kernel, [w_eta - 1, w_eta - 1, h_eta - 1, h_eta - 1]).unsqueeze(0).unsqueeze(0)
    kernel = F.conv2d(kernel0, eta)

    return kernel

# positional embeddings
class FrequencyPositionalEmbedding(nn.Module):
    def __init__(self):

        super().__init__()

    def forward(self, x):

        N = x.size(2)
        args = torch.arange(0, N, dtype=x.dtype, device=x.device) * torch.pi * 2 / N
        cos = torch.cos(args).reshape(1, 1, -1, 1)
        sin = torch.sin(args).reshape(1, 1, -1, 1)
        zeros = torch.zeros_like(x[:, 0:1, :, :])

        y = torch.cat((x, zeros + sin, zeros + cos), dim=1)

        return y


class PositionalEmbedding2D(nn.Module):
    def __init__(self, d=5):

        super().__init__()

        self.d = d

    def forward(self, x):

        N = x.size(2)
        M = x.size(3)

        h_args = torch.arange(0, N, dtype=x.dtype, device=x.device).reshape(1, 1, -1, 1)
        w_args = torch.arange(0, M, dtype=x.dtype, device=x.device).reshape(1, 1, 1, -1)
        coeffs = (10000 ** (-2 * torch.arange(0, self.d, dtype=x.dtype, device=x.device) / self.d)).reshape(1, -1, 1, 1)

        h_sin = torch.sin(coeffs * h_args)
        h_cos = torch.sin(coeffs * h_args)
        w_sin = torch.sin(coeffs * w_args)
        w_cos = torch.sin(coeffs * w_args)

        zeros = torch.zeros_like(x[:, 0:1, :, :])

        y = torch.cat((x, zeros + h_sin, zeros + h_cos, zeros + w_sin, zeros + w_cos), dim=1)

        return y


# spectral discriminator base class
class SpecDiscriminatorBase(nn.Module):
    RECEPTIVE_FIELD_MAX_WIDTH=10000
    def __init__(self,
                 layers,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7000],
                 noise_gain=1e-3,
                 fmap_start_index=0
                 ):
        super().__init__()


        self.layers = nn.ModuleList(layers)
        self.resolution = resolution
        self.fs = fs
        self.noise_gain = noise_gain
        self.fmap_start_index = fmap_start_index

        if fmap_start_index >= len(layers):
            raise ValueError(f'fmap_start_index is larger than number of layers')

        # filter bank for noise shaping
        n_fft = resolution[0]

        self.filterbank = nn.Parameter(
            gen_filterbank(n_fft // 2, fs, keep_size=True),
            requires_grad=False
        )

        # roi bins
        f_step = fs / n_fft
        self.start_bin = int(m.ceil(freq_roi[0] / f_step - 0.01))
        self.stop_bin = min(int(m.floor(freq_roi[1] / f_step + 0.01)), n_fft//2 + 1)

        self.init_weights()

        # determine receptive field size, offsets and strides

        hw = 1000
        while True:
            x = torch.zeros((1, hw, hw))
            with torch.no_grad():
                y = self.run_layer_stack(x)[-1]

            pos0 = [y.size(-2) // 2, y.size(-1) // 2]
            pos1 = [t + 1 for t in pos0]

            hs0, ws0 = self._receptive_field((hw, hw), pos0)
            hs1, ws1 = self._receptive_field((hw, hw), pos1)

            h0 = hs0[1] - hs0[0] + 1
            h1 = hs1[1] - hs1[0] + 1
            w0 = ws0[1] - ws0[0] + 1
            w1 = ws1[1] - ws1[0] + 1

            if h0 != h1 or w0 != w1:
                hw = 2 * hw
            else:

                # strides
                sh = hs1[0] - hs0[0]
                sw = ws1[0] - ws0[0]

                if sh == 0 or sw == 0: continue

                # offsets
                oh = hs0[0] - sh * pos0[0]
                ow = ws0[0] - sw * pos0[1]

                # overlap factor
                overlap = w0 / sw + h0 / sh

                #print(f"{w0=} {h0=} {sw=} {sh=} {overlap=}")
                self.receptive_field_params = {'width': [sw, ow, w0], 'height': [sh, oh, h0], 'overlap': overlap}

                break

            if hw > self.RECEPTIVE_FIELD_MAX_WIDTH:
                print("warning: exceeded max size while trying to determine receptive field")

        # create transposed convolutional kernel
        #self.tconv_kernel = nn.Parameter(create_kernel(h0, w0, sw, sw), requires_grad=False)

    def run_layer_stack(self, spec):

        output = []

        x = spec.unsqueeze(1)

        for layer in self.layers:
            x = layer(x)
            output.append(x)

        return output

    def forward(self, x):
        """ returns array with feature maps and final score at index -1 """

        output = []

        x = self.spectrogram(x)

        output = self.run_layer_stack(x)

        return output[self.fmap_start_index:]

    def receptive_field(self, output_pos):

        if self.receptive_field_params is not None:
            s, o, h = self.receptive_field_params['height']
            h_min = output_pos[0] * s + o + self.start_bin
            h_max = h_min + h
            h_min = max(h_min, self.start_bin)
            h_max = min(h_max, self.stop_bin)

            s, o, w = self.receptive_field_params['width']
            w_min = output_pos[1] * s + o
            w_max = w_min + w

            return (h_min, h_max), (w_min, w_max)

        else:
            return None, None


    def _receptive_field(self, input_dims, output_pos):
        """ determines receptive field probabilistically via autograd (slow) """

        x = torch.randn((1,) + input_dims, requires_grad=True)

        # run input through layers
        y = self.run_layer_stack(x)[-1]
        b, c, h, w = y.shape

        if output_pos[0] >= h or output_pos[1] >= w:
            raise ValueError("position out of range")

        mask = torch.zeros((b, c, h, w))
        mask[0, 0, output_pos[0], output_pos[1]] = 1

        (mask * y).sum().backward()

        hs, ws = torch.nonzero(x.grad[0], as_tuple=True)

        h_min, h_max = hs.min().item(), hs.max().item()
        w_min, w_max = ws.min().item(), ws.max().item()

        return [h_min, h_max], [w_min, w_max]



    def init_weights(self):

        for m in self.modules():
            if isinstance(m, nn.Conv1d) or isinstance(m, nn.ConvTranspose1d) or isinstance(m, nn.Linear) or isinstance(m, nn.Embedding):
                nn.init.orthogonal_(m.weight.data)


    def spectrogram(self, x):
        n_fft, hop_length, win_length = self.resolution
        x = x.squeeze(1)
        window = getattr(torch, 'hann_window')(win_length).to(x.device)

        x = torch.stft(x, n_fft=n_fft, hop_length=hop_length, win_length=win_length,\
                       window=window, return_complex=True) #[B, F, T]
        x = torch.abs(x)

        # noise floor following spectral envelope
        smoothed_x = torch.matmul(self.filterbank, x)
        noise = torch.randn_like(x) * smoothed_x * self.noise_gain
        x = x + noise

        # frequency ROI
        x = x[:, self.start_bin : self.stop_bin + 1, ...]

        return torchaudio.functional.amplitude_to_DB(x,db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80)#torch.sqrt(x)

    def grad_map(self, x):
        self.zero_grad()

        n_fft, hop_length, win_length = self.resolution

        window = getattr(torch, 'hann_window')(win_length).to(x.device)

        y = torch.stft(x.squeeze(1), n_fft=n_fft, hop_length=hop_length, win_length=win_length,
                       window=window, return_complex=True) #[B, F, T]
        y = torch.abs(y)

        specgram  = torchaudio.functional.amplitude_to_DB(y,db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80)

        specgram.requires_grad = True
        specgram.retain_grad()

        if specgram.grad is not None:
            specgram.grad.zero_()

        y = specgram[:, self.start_bin : self.stop_bin + 1, ...]

        scores = self.run_layer_stack(y)[-1]

        loss = torch.mean((1 - scores) ** 2)
        loss.backward()

        return specgram.data[0], torch.abs(specgram.grad)[0]

    def relevance_map(self, x):

        n_fft, hop_length, win_length = self.resolution
        y = x.view(-1)
        window = getattr(torch, 'hann_window')(win_length).to(x.device)

        y = torch.stft(y, n_fft=n_fft, hop_length=hop_length, win_length=win_length,\
                       window=window, return_complex=True) #[B, F, T]
        y = torch.abs(y)

        specgram  = torchaudio.functional.amplitude_to_DB(y,db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80)


        scores = self.forward(x)[-1]

        sh, _, h = self.receptive_field_params['height']
        sw, _, w = self.receptive_field_params['width']
        kernel = create_kernel(h, w, sh, sw).float().to(scores.device)
        with torch.no_grad():
            pad_w = (w + sw - 1) // sw
            pad_h = (h + sh - 1) // sh
            padded_scores = F.pad(scores, (pad_w, pad_w, pad_h, pad_h), mode='replicate')
            # CAVE: padding should be derived from offsets
            rv = F.conv_transpose2d(padded_scores, kernel, bias=None, stride=(sh, sw), padding=(h//2, w//2))
            rv = rv[..., pad_h * sh : - pad_h * sh,  pad_w * sw : -pad_w * sw]

            relevance = torch.zeros_like(specgram)
            relevance[..., self.start_bin : self.start_bin + rv.size(-2), : rv.size(-1)] = rv


        return specgram, relevance


    def lrp(self, x, eps=1e-9, label='both', threshold=0.5, low=None, high=None, verbose=False):
        """ layer-wise relevance propagation (https://git.tu-berlin.de/gmontavon/lrp-tutorial) """

        # ToDo: this code is highly unsafe as it assumes that layers are nn.Sequential with suitable activations

        def newconv2d(layer,g):

            new_layer = nn.Conv2d(layer.in_channels,
                                  layer.out_channels,
                                  layer.kernel_size,
                                  stride=layer.stride,
                                  padding=layer.padding,
                                  dilation=layer.dilation,
                                  groups=layer.groups)

            try: new_layer.weight = nn.Parameter(g(layer.weight.data.clone()))
            except AttributeError: pass

            try: new_layer.bias   = nn.Parameter(g(layer.bias.data.clone()))
            except AttributeError: pass

            return new_layer

        bounds = {
            64: [-85.82449722290039, 2.1755014657974243],
            128: [-84.49211349487305, 3.5078893899917607],
            256: [-80.33127822875977, 7.6687201976776125],
            512: [-73.79328079223633, 14.20672025680542],
            1024: [-67.59239501953125, 20.40760498046875],
            2048: [-62.31902580261231, 25.680974197387698],
        }

        nfft = self.resolution[0]
        if low is None: low = bounds[nfft][0]
        if high is None: high = bounds[nfft][1]

        remove_all_weight_norms(self)

        for p in self.parameters():
            if p.grad is not None:
                p.grad.zero_()

        num_layers = len(self.layers)
        X = self.spectrogram(x). detach()


        # forward pass
        A = [X.unsqueeze(1)] + [None] * len(self.layers)

        for i in range(num_layers - 1):
            A[i + 1] = self.layers[i](A[i])

        # initial relevance is last layer without activation
        r = A[-2]
        last_layer_rs = [r]
        layer = self.layers[-1]
        for sublayer in list(layer)[:-1]:
            r = sublayer(r)
            last_layer_rs.append(r)


        mask = torch.zeros_like(r)
        mask.requires_grad_(False)
        if verbose:
            print(r.min(), r.max())
        if label in {'both', 'fake'}:
            mask[r < -threshold] = 1
        if label in {'both', 'real'}:
            mask[r > threshold] = 1
        r = r * mask

        # backward pass
        R = [None] * num_layers + [r]

        for l in range(1, num_layers)[::-1]:
            A[l] = (A[l]).data.requires_grad_(True)

            layer = nn.Sequential(*(list(self.layers[l])[:-1]))
            z = layer(A[l]) + eps
            s = (R[l+1] / z).data
            (z*s).sum().backward()
            c = A[l].grad
            R[l] = (A[l] * c).data

        # first layer
        A[0] = (A[0].data).requires_grad_(True)

        Xl = (torch.zeros_like(A[0].data) + low).requires_grad_(True)
        Xh = (torch.zeros_like(A[0].data) + high).requires_grad_(True)

        if len(list(self.layers)) > 2:
            # unsafe way to check for embedding layer
            embed = list(self.layers[0])[0]
            conv  = list(self.layers[0])[1]

            layer = nn.Sequential(embed, conv)
            layerl = nn.Sequential(embed, newconv2d(conv, lambda p: p.clamp(min=0)))
            layerh = nn.Sequential(embed, newconv2d(conv, lambda p: p.clamp(max=0)))

        else:
            layer = list(self.layers[0])[0]
            layerl = newconv2d(layer, lambda p: p.clamp(min=0))
            layerh = newconv2d(layer, lambda p: p.clamp(max=0))


        z = layer(A[0])
        z -= layerl(Xl) + layerh(Xh)
        s = (R[1] / z).data
        (z * s).sum().backward()
        c, cp, cm = A[0].grad, Xl.grad, Xh.grad

        R[0] = (A[0] * c + Xl * cp + Xh * cm)
        #R[0] = (A[0] * c).data

        return X, R[0].mean(dim=1)










def create_3x3_conv_plan(num_layers : int,
                         f_stretch : int,
                         f_down : int,
                         t_stretch : int,
                         t_down : int
                         ):


    """ creates a stride, dilation, padding plan for a 2d conv network

    Args:
        num_layers (int): number of layers
        f_stretch (int): log_2 of stretching factor along frequency axis
        f_down (int): log_2 of downsampling factor along frequency axis
        t_stretch (int): log_2 of stretching factor along time axis
        t_down (int): log_2 of downsampling factor along time axis

    Returns:
        list(list(tuple)): list containing entries [(stride_t, stride_f), (dilation_t, dilation_f), (padding_t, padding_f)]
    """

    assert num_layers > 0 and t_stretch >= 0 and t_down >= 0 and f_stretch >= 0 and f_down >= 0
    assert f_stretch < num_layers and t_stretch < num_layers

    def process_dimension(n_layers, stretch, down):

        stack_layers = n_layers - 1

        stride_layers = min(min(down, stretch) , stack_layers)
        dilation_layers = max(min(stack_layers - stride_layers - 1, stretch - stride_layers), 0)
        final_stride = 2 ** (max(down - stride_layers, 0))

        final_dilation = 1
        if stride_layers < stack_layers and stretch - stride_layers - dilation_layers > 0:
                final_dilation = 2

        strides, dilations, paddings = [], [], []
        processed_layers = 0
        current_dilation = 1

        for _ in range(stride_layers):
            # increase receptive field and downsample via stride = 2
            strides.append(2)
            dilations.append(1)
            paddings.append(1)
            processed_layers += 1

        if processed_layers < stack_layers:
            strides.append(1)
            dilations.append(1)
            paddings.append(1)
            processed_layers += 1

        for _ in range(dilation_layers):
            # increase receptive field via dilation = 2
            strides.append(1)
            current_dilation *= 2
            dilations.append(current_dilation)
            paddings.append(current_dilation)
            processed_layers += 1

        while processed_layers < n_layers - 1:
            # fill up with std layers
            strides.append(1)
            dilations.append(current_dilation)
            paddings.append(current_dilation)
            processed_layers += 1

        # final layer
        strides.append(final_stride)
        current_dilation * final_dilation
        dilations.append(current_dilation)
        paddings.append(current_dilation)
        processed_layers += 1

        assert processed_layers == n_layers

        return strides, dilations, paddings

    t_strides, t_dilations, t_paddings = process_dimension(num_layers, t_stretch, t_down)
    f_strides, f_dilations, f_paddings = process_dimension(num_layers, f_stretch, f_down)

    plan = []

    for i in range(num_layers):
        plan.append([
            (f_strides[i], t_strides[i]),
            (f_dilations[i], t_dilations[i]),
            (f_paddings[i], t_paddings[i]),
            ])

    return plan


class DiscriminatorExperimental(SpecDiscriminatorBase):

    def __init__(self,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 num_channels=16,
                 max_channels=512,
                 num_layers=5,
                 use_spectral_norm=False):

        norm_f = weight_norm if use_spectral_norm == False else spectral_norm

        self.num_channels = num_channels
        self.num_channels_max = max_channels
        self.num_layers = num_layers

        layers = []
        stride = (2, 1)
        padding= (1, 1)
        in_channels = 1 + 2
        out_channels = self.num_channels
        for _ in range(self.num_layers):
            layers.append(
                nn.Sequential(
                    FrequencyPositionalEmbedding(),
                    norm_f(nn.Conv2d(in_channels, out_channels, (3, 3), stride=stride, padding=padding)),
                    nn.ReLU(inplace=True)
                )
            )
            in_channels = out_channels + 2
            out_channels = min(2 * out_channels, self.num_channels_max)

        layers.append(
            nn.Sequential(
                FrequencyPositionalEmbedding(),
                norm_f(nn.Conv2d(in_channels, 1, (3, 3), padding=padding)),
                nn.Sigmoid()
            )
        )

        super().__init__(layers=layers, resolution=resolution, fs=fs, freq_roi=freq_roi, noise_gain=noise_gain)

        # bias biases
        bias_val = 0.1
        with torch.no_grad():
            for name, weight in self.named_parameters():
                if 'bias' in name:
                    weight = weight + bias_val


configs = {
    'f_down': {
        'stretch' : {
            64 : (0, 0),
            128: (1, 0),
            256: (2, 0),
            512: (3, 0),
            1024: (4, 0),
            2048: (5, 0)
        },
        'down' : {
            64 : (0, 0),
            128: (1, 0),
            256: (2, 0),
            512: (3, 0),
            1024: (4, 0),
            2048: (5, 0)
        }
    },
    'ft_down': {
        'stretch' : {
            64 : (0, 4),
            128: (1, 3),
            256: (2, 2),
            512: (3, 1),
            1024: (4, 0),
            2048: (5, 0)
        },
        'down' : {
            64 : (0, 4),
            128: (1, 3),
            256: (2, 2),
            512: (3, 1),
            1024: (4, 0),
            2048: (5, 0)
        }
    },
    'dilated': {
        'stretch' : {
            64 : (0, 4),
            128: (1, 3),
            256: (2, 2),
            512: (3, 1),
            1024: (4, 0),
            2048: (5, 0)
        },
        'down' : {
            64 : (0, 0),
            128: (0, 0),
            256: (0, 0),
            512: (0, 0),
            1024: (0, 0),
            2048: (0, 0)
        }
    },
    'mixed': {
        'stretch' : {
            64 : (0, 4),
            128: (1, 3),
            256: (2, 2),
            512: (3, 1),
            1024: (4, 0),
            2048: (5, 0)
        },
        'down' : {
            64 : (0, 0),
            128: (1, 0),
            256: (2, 0),
            512: (3, 0),
            1024: (4, 0),
            2048: (5, 0)
        }
    },
}


class DiscriminatorMagFree(SpecDiscriminatorBase):

    def __init__(self,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 num_channels=16,
                 max_channels=256,
                 num_layers=5,
                 use_spectral_norm=False,
                 design=None):

        if design is None:
            raise ValueError('error: arch required in DiscriminatorMagFree')

        norm_f = weight_norm if use_spectral_norm == False else spectral_norm

        stretch = configs[design]['stretch'][resolution[0]]
        down = configs[design]['down'][resolution[0]]

        self.num_channels = num_channels
        self.num_channels_max = max_channels
        self.num_layers = num_layers
        self.stretch = stretch
        self.down = down

        layers = []
        plan = create_3x3_conv_plan(num_layers + 1, stretch[0], down[0], stretch[1], down[1])
        in_channels = 1 + 2
        out_channels = self.num_channels
        for i in range(self.num_layers):
            layers.append(
                nn.Sequential(
                    FrequencyPositionalEmbedding(),
                    norm_f(nn.Conv2d(in_channels, out_channels, (3, 3), stride=plan[i][0], dilation=plan[i][1], padding=plan[i][2])),
                    nn.ReLU(inplace=True)
                )
            )
            in_channels = out_channels + 2
            # product over strides
            channel_factor = plan[i][0][0] * plan[i][0][1]
            out_channels = min(channel_factor * out_channels, self.num_channels_max)

        layers.append(
            nn.Sequential(
                FrequencyPositionalEmbedding(),
                norm_f(nn.Conv2d(in_channels, 1, (3, 3), stride=plan[-1][0], dilation=plan[-1][1], padding=plan[-1][2])),
                nn.Sigmoid()
            )
        )



        # for layer in layers:
        #     print(layer)

        # print("end\n\n")

        super().__init__(layers=layers, resolution=resolution, fs=fs, freq_roi=freq_roi, noise_gain=noise_gain)

        # bias biases
        bias_val = 0.1
        with torch.no_grad():
            for name, weight in self.named_parameters():
                if 'bias' in name:
                    weight = weight + bias_val

class DiscriminatorMagFreqPosition(SpecDiscriminatorBase):

    def __init__(self,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 num_channels=16,
                 max_channels=512,
                 num_layers=5,
                 use_spectral_norm=False):

        norm_f = weight_norm if use_spectral_norm == False else spectral_norm

        self.num_channels = num_channels
        self.num_channels_max = max_channels
        self.num_layers = num_layers

        layers = []
        stride = (2, 1)
        padding= (1, 1)
        in_channels = 1 + 2
        out_channels = self.num_channels
        for _ in range(self.num_layers):
            layers.append(
                nn.Sequential(
                    FrequencyPositionalEmbedding(),
                    norm_f(nn.Conv2d(in_channels, out_channels, (3, 3), stride=stride, padding=padding)),
                    nn.LeakyReLU(0.2, inplace=True)
                )
            )
            in_channels = out_channels + 2
            out_channels = min(2 * out_channels, self.num_channels_max)

        layers.append(
            nn.Sequential(
                FrequencyPositionalEmbedding(),
                norm_f(nn.Conv2d(in_channels, 1, (3, 3), padding=padding))
            )
        )

        super().__init__(layers=layers, resolution=resolution, fs=fs, freq_roi=freq_roi, noise_gain=noise_gain)



class DiscriminatorMag2dPositional(SpecDiscriminatorBase):

    def __init__(self,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 num_channels=16,
                 max_channels=512,
                 num_layers=5,
                 d=5,
                 use_spectral_norm=False):

        norm_f = weight_norm if use_spectral_norm == False else spectral_norm
        self.resolution = resolution
        self.num_channels = num_channels
        self.num_channels_max = max_channels
        self.num_layers = num_layers
        self.d = d
        embedding_dim = 4 * d


        layers = []
        stride = (2, 2)
        padding= (1, 1)
        in_channels = 1 + embedding_dim
        out_channels = self.num_channels
        for _ in range(self.num_layers):
            layers.append(
                nn.Sequential(
                    PositionalEmbedding2D(d),
                    norm_f(nn.Conv2d(in_channels, out_channels, (3, 3), stride=stride, padding=padding)),
                    nn.LeakyReLU(0.2, inplace=True)
                )
            )
            in_channels = out_channels + embedding_dim
            out_channels = min(2 * out_channels, self.num_channels_max)


        layers.append(
            nn.Sequential(
                PositionalEmbedding2D(),
                norm_f(nn.Conv2d(in_channels, 1, (3, 3), padding=padding))
            )
        )

        super().__init__(layers=layers, resolution=resolution, fs=fs, freq_roi=freq_roi, noise_gain=noise_gain)



class DiscriminatorMag(SpecDiscriminatorBase):
    def __init__(self,
                 resolution,
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 num_channels=32,
                 num_layers=5,
                 use_spectral_norm=False):

        norm_f = weight_norm if use_spectral_norm == False else spectral_norm

        self.num_channels = num_channels
        self.num_layers = num_layers

        layers = []
        stride = (1, 1)
        padding= (1, 1)
        in_channels = 1
        out_channels = self.num_channels
        for _ in range(self.num_layers):
            layers.append(
                nn.Sequential(
                    norm_f(nn.Conv2d(in_channels, out_channels, (3, 3), stride=stride, padding=padding)),
                    nn.LeakyReLU(0.2, inplace=True)
                )
            )
            in_channels = out_channels

        layers.append(norm_f(nn.Conv2d(in_channels, 1, (3, 3), padding=padding)))

        super().__init__(layers=layers, resolution=resolution, fs=fs, freq_roi=freq_roi, noise_gain=noise_gain)


discriminators = {
    'mag': DiscriminatorMag,
    'freqpos': DiscriminatorMagFreqPosition,
    '2dpos': DiscriminatorMag2dPositional,
    'experimental': DiscriminatorExperimental,
    'free': DiscriminatorMagFree
}

class TFDMultiResolutionDiscriminator(torch.nn.Module):
    def __init__(self,
                 fft_sizes_16k=[64, 128, 256, 512, 1024, 2048],
                 architecture='mag',
                 fs=16000,
                 freq_roi=[50, 7400],
                 noise_gain=0,
                 use_spectral_norm=False,
                 **kwargs):

        super().__init__()


        fft_sizes = [int(round(fft_size_16k * fs / 16000)) for fft_size_16k in fft_sizes_16k]

        resolutions = [[n_fft, n_fft // 4, n_fft] for n_fft in fft_sizes]


        Disc = discriminators[architecture]

        discs = [Disc(resolutions[i], fs=fs, freq_roi=freq_roi, noise_gain=noise_gain, use_spectral_norm=use_spectral_norm, **kwargs) for i in range(len(resolutions))]

        self.discriminators = nn.ModuleList(discs)

    def forward(self, y):
        outputs = []

        for  disc in self.discriminators:
            outputs.append(disc(y))

        return outputs


class FWGAN_disc_wrapper(nn.Module):
    def __init__(self, disc):
        super().__init__()

        self.disc = disc

    def forward(self, y, y_hat):

        out_real = self.disc(y)
        out_fake = self.disc(y_hat)

        y_d_rs = []
        y_d_gs = []
        fmap_rs = []
        fmap_gs = []

        for y_real, y_fake in zip(out_real, out_fake):
            y_d_rs.append(y_real[-1])
            y_d_gs.append(y_fake[-1])
            fmap_rs.append(y_real[:-1])
            fmap_gs.append(y_fake[:-1])

        return y_d_rs, y_d_gs, fmap_rs, fmap_gs

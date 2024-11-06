"""STFT-based Loss modules."""

import torch
import torch.nn.functional as F
import numpy as np
import torchaudio


def stft(x, fft_size, hop_size, win_length, window):
    """Perform STFT and convert to magnitude spectrogram.
    Args:
        x (Tensor): Input signal tensor (B, T).
        fft_size (int): FFT size.
        hop_size (int): Hop size.
        win_length (int): Window length.
        window (str): Window function type.
    Returns:
        Tensor: Magnitude spectrogram (B, #frames, fft_size // 2 + 1).
    """

    #x_stft = torch.stft(x, fft_size, hop_size, win_length, window, return_complex=False)
    #real = x_stft[..., 0]
    #imag = x_stft[..., 1]

    # (kan-bayashi): clamp is needed to avoid nan or inf
    #return torchaudio.functional.amplitude_to_DB(torch.abs(x_stft),db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80)
    #return torch.clamp(torch.abs(x_stft), min=1e-7)

    x_stft = torch.stft(x, fft_size, hop_size, win_length, window, return_complex=True)
    return torch.clamp(torch.abs(x_stft), min=1e-7)

class SpectralConvergenceLoss(torch.nn.Module):
    """Spectral convergence loss module."""

    def __init__(self):
        """Initilize spectral convergence loss module."""
        super(SpectralConvergenceLoss, self).__init__()

    def forward(self, x_mag, y_mag):
        """Calculate forward propagation.
        Args:
            x_mag (Tensor): Magnitude spectrogram of predicted signal (B, #frames, #freq_bins).
            y_mag (Tensor): Magnitude spectrogram of groundtruth signal (B, #frames, #freq_bins).
        Returns:
            Tensor: Spectral convergence loss value.
        """
        x_mag = torch.sqrt(x_mag)
        y_mag = torch.sqrt(y_mag)
        return torch.norm(y_mag - x_mag, p=1) / torch.norm(y_mag, p=1)

class LogSTFTMagnitudeLoss(torch.nn.Module):
    """Log STFT magnitude loss module."""

    def __init__(self):
        """Initilize los STFT magnitude loss module."""
        super(LogSTFTMagnitudeLoss, self).__init__()

    def forward(self, x, y):
        """Calculate forward propagation.
        Args:
            x_mag (Tensor): Magnitude spectrogram of predicted signal (B, #frames, #freq_bins).
            y_mag (Tensor): Magnitude spectrogram of groundtruth signal (B, #frames, #freq_bins).
        Returns:
            Tensor: Log STFT magnitude loss value.
        """
        #F.l1_loss(torch.sqrt(y_mag), torch.sqrt(x_mag)) +
        #F.l1_loss(torchaudio.functional.amplitude_to_DB(y_mag,db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80),\
        #torchaudio.functional.amplitude_to_DB(x_mag,db_multiplier=0.0, multiplier=20,amin=1e-05,top_db=80))

        #y_mag[:,:y_mag.size(1)//2,:] = y_mag[:,:y_mag.size(1)//2,:] *0.0

        #return F.l1_loss(torch.log(y_mag) + torch.sqrt(y_mag), torch.log(x_mag) + torch.sqrt(x_mag))

        #return F.l1_loss(y_mag, x_mag)

        error_loss =  F.l1_loss(y, x) #+ F.l1_loss(torch.sqrt(y), torch.sqrt(x))#F.l1_loss(torch.log(y), torch.log(x))#

        #x = torch.log(x)
        #y = torch.log(y)
        #x = x.permute(0,2,1).contiguous()
        #y = y.permute(0,2,1).contiguous()

        '''mean_x = torch.mean(x, dim=1, keepdim=True)
        mean_y = torch.mean(y, dim=1, keepdim=True)

        var_x = torch.var(x, dim=1, keepdim=True)
        var_y = torch.var(y, dim=1, keepdim=True)

        std_x = torch.std(x, dim=1, keepdim=True)
        std_y = torch.std(y, dim=1, keepdim=True)

        x_minus_mean = x - mean_x
        y_minus_mean = y - mean_y

        pearson_corr = torch.sum(x_minus_mean * y_minus_mean, dim=1, keepdim=True) / \
                    (torch.sqrt(torch.sum(x_minus_mean ** 2, dim=1, keepdim=True) + 1e-7) * \
                    torch.sqrt(torch.sum(y_minus_mean ** 2, dim=1, keepdim=True) + 1e-7))

        numerator = 2.0 * pearson_corr * std_x * std_y
        denominator = var_x + var_y + (mean_y - mean_x)**2

        ccc = numerator/denominator

        ccc_loss = F.l1_loss(1.0 - ccc, torch.zeros_like(ccc))'''

        return error_loss #+ ccc_loss#+ ccc_loss


class STFTLoss(torch.nn.Module):
    """STFT loss module."""

    def __init__(self, device, fft_size=1024, shift_size=120, win_length=600, window="hann_window"):
        """Initialize STFT loss module."""
        super(STFTLoss, self).__init__()
        self.fft_size = fft_size
        self.shift_size = shift_size
        self.win_length = win_length
        self.window = getattr(torch, window)(win_length).to(device)
        self.spectral_convergenge_loss = SpectralConvergenceLoss()
        self.log_stft_magnitude_loss = LogSTFTMagnitudeLoss()

    def forward(self, x, y):
        """Calculate forward propagation.
        Args:
            x (Tensor): Predicted signal (B, T).
            y (Tensor): Groundtruth signal (B, T).
        Returns:
            Tensor: Spectral convergence loss value.
            Tensor: Log STFT magnitude loss value.
        """
        x_mag = stft(x, self.fft_size, self.shift_size, self.win_length, self.window)
        y_mag = stft(y, self.fft_size, self.shift_size, self.win_length, self.window)
        sc_loss = self.spectral_convergenge_loss(x_mag, y_mag)
        mag_loss = self.log_stft_magnitude_loss(x_mag, y_mag)

        return sc_loss, mag_loss


class MultiResolutionSTFTLoss(torch.nn.Module):

    '''def __init__(self,
                 device,
                 fft_sizes=[2048, 1024, 512, 256, 128, 64],
                 hop_sizes=[512, 256, 128, 64, 32, 16],
                 win_lengths=[2048, 1024, 512, 256, 128, 64],
                 window="hann_window"):'''

    '''def __init__(self,
                 device,
                 fft_sizes=[2048, 1024, 512, 256, 128, 64],
                 hop_sizes=[256, 128, 64, 32, 16, 8],
                 win_lengths=[1024, 512, 256, 128, 64, 32],
                 window="hann_window"):'''

    def __init__(self,
                 device,
                 fft_sizes=[2560, 1280, 640, 320, 160, 80],
                 hop_sizes=[640, 320, 160, 80, 40, 20],
                 win_lengths=[2560, 1280, 640, 320, 160, 80],
                 window="hann_window"):

        super(MultiResolutionSTFTLoss, self).__init__()
        assert len(fft_sizes) == len(hop_sizes) == len(win_lengths)
        self.stft_losses = torch.nn.ModuleList()
        for fs, ss, wl in zip(fft_sizes, hop_sizes, win_lengths):
            self.stft_losses += [STFTLoss(device, fs, ss, wl, window)]

    def forward(self, x, y):
        """Calculate forward propagation.
        Args:
            x (Tensor): Predicted signal (B, T).
            y (Tensor): Groundtruth signal (B, T).
        Returns:
            Tensor: Multi resolution spectral convergence loss value.
            Tensor: Multi resolution log STFT magnitude loss value.
        """
        sc_loss = 0.0
        mag_loss = 0.0
        for f in self.stft_losses:
            sc_l, mag_l = f(x, y)
            sc_loss += sc_l
            #mag_loss += mag_l
        sc_loss /= len(self.stft_losses)
        mag_loss /= len(self.stft_losses)

        return  sc_loss #mag_loss #+

import numpy as np
import scipy.signal
import torch
from torch import nn
import torch.nn.functional as F


class FIR(nn.Module):
    def __init__(self, numtaps, bands, desired, fs=2):
        super().__init__()
        
        if numtaps % 2 == 0:
            print(f"warning: numtaps must be odd, increasing numtaps to {numtaps + 1}")
            numtaps += 1
        
        a = scipy.signal.firls(numtaps, bands, desired, fs=fs)
        
        self.weight = torch.from_numpy(a.astype(np.float32))
        
    def forward(self, x):
        num_channels = x.size(1)
        
        weight = torch.repeat_interleave(self.weight.view(1, 1, -1), num_channels, 0)
        
        y = F.conv1d(x, weight, groups=num_channels)
        
        return y
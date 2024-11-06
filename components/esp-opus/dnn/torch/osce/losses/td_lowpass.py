import torch
import scipy.signal


from utils.layers.fir import FIR

class TDLowpass(torch.nn.Module):
    def __init__(self, numtaps, cutoff, power=2):
        super().__init__()
        
        self.b = scipy.signal.firwin(numtaps, cutoff)
        self.weight = torch.from_numpy(self.b).float().view(1, 1, -1)
        self.power = power
        
    def forward(self, y_true, y_pred):
        
        assert len(y_true.shape) == 3 and len(y_pred.shape) == 3
        
        diff = y_true - y_pred
        diff_lp = torch.nn.functional.conv1d(diff, self.weight)
        
        loss = torch.mean(torch.abs(diff_lp ** self.power))
        
        return loss, diff_lp
    
    def get_freqz(self):
        freq, response = scipy.signal.freqz(self.b)
        
        return freq, response
        
        
        
        
        
import torch
from torch import nn
import torch.nn.functional as F

class LossGen(nn.Module):
    def __init__(self, gru1_size=16, gru2_size=16):
        super(LossGen, self).__init__()

        self.gru1_size = gru1_size
        self.gru2_size = gru2_size
        self.dense_in = nn.Linear(2, 8)
        self.gru1 = nn.GRU(8, self.gru1_size, batch_first=True)
        self.gru2 = nn.GRU(self.gru1_size, self.gru2_size, batch_first=True)
        self.dense_out = nn.Linear(self.gru2_size, 1)

    def forward(self, loss, perc, states=None):
        #print(states)
        device = loss.device
        batch_size = loss.size(0)
        if states is None:
            gru1_state = torch.zeros((1, batch_size, self.gru1_size), device=device)
            gru2_state = torch.zeros((1, batch_size, self.gru2_size), device=device)
        else:
            gru1_state = states[0]
            gru2_state = states[1]
        x = torch.tanh(self.dense_in(torch.cat([loss, perc], dim=-1)))
        gru1_out, gru1_state = self.gru1(x, gru1_state)
        gru2_out, gru2_state = self.gru2(gru1_out, gru2_state)
        return self.dense_out(gru2_out), [gru1_state, gru2_state]

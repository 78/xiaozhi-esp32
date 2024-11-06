import lossgen
import os
import argparse
import torch
import numpy as np


parser = argparse.ArgumentParser()

parser.add_argument('model', type=str, help='CELPNet model')
parser.add_argument('percentage', type=float, help='percentage loss')
parser.add_argument('output', type=str, help='path to output file (ascii)')

parser.add_argument('--length', type=int, help="length of sequence to generate", default=500)

args = parser.parse_args()



checkpoint = torch.load(args.model, map_location='cpu')
model = lossgen.LossGen(*checkpoint['model_args'], **checkpoint['model_kwargs'])
model.load_state_dict(checkpoint['state_dict'], strict=False)

states=None
last = torch.zeros((1,1,1))
perc = torch.tensor((args.percentage,))[None,None,:]
seq = torch.zeros((0,1,1))

one = torch.ones((1,1,1))
zero = torch.zeros((1,1,1))

if __name__ == '__main__':
    for i in range(args.length):
        prob, states = model(last, perc, states=states)
        prob = torch.sigmoid(prob)
        states[0] = states[0].detach()
        states[1] = states[1].detach()
        loss = one if np.random.rand() < prob else zero
        last = loss
        seq = torch.cat([seq, loss])

np.savetxt(args.output, seq[:,:,0].numpy().astype('int'), fmt='%d')

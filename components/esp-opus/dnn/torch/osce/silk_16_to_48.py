import argparse

from scipy.io import wavfile
import torch
import numpy as np

from utils.layers.silk_upsampler import SilkUpsampler

parser = argparse.ArgumentParser()
parser.add_argument("input", type=str, help="input wave file")
parser.add_argument("output", type=str, help="output wave file")

if __name__ == "__main__":
    args = parser.parse_args()
    
    fs, x = wavfile.read(args.input)

    # being lazy for now
    assert fs == 16000 and x.dtype == np.int16
    
    x = torch.from_numpy(x.astype(np.float32)).view(1, 1, -1)
    
    upsampler = SilkUpsampler()
    y = upsampler(x)
    
    y = y.squeeze().numpy().astype(np.int16)
    
    wavfile.write(args.output, 48000, y[13:])
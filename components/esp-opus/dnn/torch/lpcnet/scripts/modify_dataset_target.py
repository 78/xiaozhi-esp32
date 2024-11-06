import argparse

import numpy as np


parser = argparse.ArgumentParser(description="sets s_t to augmented_s_t")

parser.add_argument('datafile', type=str, help='data.s16 file path')

args = parser.parse_args()

data = np.memmap(args.datafile, dtype='int16', mode='readwrite')

# signal is in data[1::2]
# last augmented signal is in data[0::2]

data[1 : - 1 : 2] = data[2 : : 2]

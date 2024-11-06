#!/usr/bin/python

from __future__ import print_function

import numpy as np
import h5py
import sys

data = np.loadtxt(sys.argv[1], dtype='float32')
h5f = h5py.File(sys.argv[2], 'w');
h5f.create_dataset('data', data=data)
h5f.close()

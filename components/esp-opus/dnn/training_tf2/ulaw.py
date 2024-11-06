
import numpy as np
import math

scale = 255.0/32768.0
scale_1 = 32768.0/255.0
def ulaw2lin(u):
    u = u - 128
    s = np.sign(u)
    u = np.abs(u)
    return s*scale_1*(np.exp(u/128.*math.log(256))-1)


def lin2ulaw(x):
    s = np.sign(x)
    x = np.abs(x)
    u = (s*(128*np.log(1+scale*x)/math.log(256)))
    u = np.clip(128 + np.round(u), 0, 255)
    return u.astype('int16')

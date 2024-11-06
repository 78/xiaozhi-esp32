import torch
from torch import nn
import torch.nn.functional as F
import math

def toeplitz_from_filter(a):
    device = a.device
    L = a.size(-1)
    size0 = (*(a.shape[:-1]), L, L+1)
    size = (*(a.shape[:-1]), L, L)
    rnge = torch.arange(0, L, dtype=torch.int64, device=device)
    z = torch.tensor(0, device=device)
    idx = torch.maximum(rnge[:,None] - rnge[None,:] + 1, z)
    a = torch.cat([a[...,:1]*0, a], -1)
    #print(a)
    a = a[...,None,:]
    #print(idx)
    a = torch.broadcast_to(a, size0)
    idx = torch.broadcast_to(idx, size)
    #print(idx)
    return torch.gather(a, -1, idx)

def filter_iir_response(a, N):
    device = a.device
    L = a.size(-1)
    ar = a.flip(dims=(2,))
    size = (*(a.shape[:-1]), N)
    R = torch.zeros(size, device=device)
    R[:,:,0] = torch.ones((a.shape[:-1]), device=device)
    for i in range(1, L):
        R[:,:,i] = - torch.sum(ar[:,:,L-i-1:-1] * R[:,:,:i], axis=-1)
        #R[:,:,i] = - torch.einsum('ijk,ijk->ij', ar[:,:,L-i-1:-1], R[:,:,:i])
    for i in range(L, N):
        R[:,:,i] = - torch.sum(ar[:,:,:-1] * R[:,:,i-L+1:i], axis=-1)
        #R[:,:,i] = - torch.einsum('ijk,ijk->ij', ar[:,:,:-1], R[:,:,i-L+1:i])
    return R

if __name__ == '__main__':
    #a = torch.tensor([ [[1, -.9, 0.02], [1, -.8, .01]], [[1, .9, 0], [1, .8, 0]]])
    a = torch.tensor([ [[1, -.9, 0.02], [1, -.8, .01]]])
    A = toeplitz_from_filter(a)
    #print(A)
    R = filter_iir_response(a, 5)

    RA = toeplitz_from_filter(R)
    print(RA)

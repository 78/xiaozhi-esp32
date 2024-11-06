import torch



def rc2lpc(rc):
    order = rc.shape[-1]
    lpc=rc[...,0:1]
    for i in range(1, order):
        lpc = torch.cat([lpc + rc[...,i:i+1]*torch.flip(lpc,dims=(-1,)), rc[...,i:i+1]], -1)
        #print("to:", lpc)
    return lpc

def lpc2rc(lpc):
    order = lpc.shape[-1]
    rc = lpc[...,-1:]
    for i in range(order-1, 0, -1):
        ki = lpc[...,-1:]
        lpc = lpc[...,:-1]
        lpc = (lpc - ki*torch.flip(lpc,dims=(-1,)))/(1 - ki*ki)
        rc = torch.cat([lpc[...,-1:] , rc], -1)
    return rc

if __name__ == "__main__":
    rc = torch.tensor([[.5, -.5, .6, -.6]])
    print(rc)
    lpc = rc2lpc(rc)
    print(lpc)
    rc2 = lpc2rc(lpc)
    print(rc2)

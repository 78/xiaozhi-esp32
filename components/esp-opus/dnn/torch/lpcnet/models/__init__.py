from .lpcnet import LPCNet
from .multi_rate_lpcnet import MultiRateLPCNet


model_dict = {
    'lpcnet'     : LPCNet,
    'multi_rate' : MultiRateLPCNet
}
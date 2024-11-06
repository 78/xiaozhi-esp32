

def _conv1d_flop_count(layer, rate):
    return 2 * ((layer.in_channels + 1) * layer.out_channels * rate / layer.stride[0] ) * layer.kernel_size[0]


def _dense_flop_count(layer, rate):
    return 2 * ((layer.in_features + 1) * layer.out_features * rate )
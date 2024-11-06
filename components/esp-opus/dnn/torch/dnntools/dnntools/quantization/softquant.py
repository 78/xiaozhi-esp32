import torch

@torch.no_grad()
def compute_optimal_scale(weight):
    with torch.no_grad():
        n_out, n_in = weight.shape
        assert n_in % 4 == 0
        if n_out % 8:
            # add padding
            pad = n_out - n_out % 8
            weight = torch.cat((weight, torch.zeros((pad, n_in), dtype=weight.dtype, device=weight.device)), dim=0)

        weight_max_abs, _ = torch.max(torch.abs(weight), dim=1)
        weight_max_sum, _ = torch.max(torch.abs(weight[:, : n_in : 2] + weight[:, 1 : n_in : 2]), dim=1)
        scale_max = weight_max_abs / 127
        scale_sum = weight_max_sum / 129

        scale = torch.maximum(scale_max, scale_sum)

    return scale[:n_out]

@torch.no_grad()
def q_scaled_noise(module, weight):
    if isinstance(module, torch.nn.Conv1d):
        w = weight.permute(0, 2, 1).flatten(1)
        noise = torch.rand_like(w) - 0.5
        noise[w == 0] = 0 # ignore zero entries from sparsification
        scale = compute_optimal_scale(w)
        noise = noise * scale.unsqueeze(-1)
        noise = noise.reshape(weight.size(0), weight.size(2), weight.size(1)).permute(0, 2, 1)
    elif isinstance(module, torch.nn.ConvTranspose1d):
        i, o, k = weight.shape
        w = weight.permute(2, 1, 0).reshape(k * o, i)
        noise = torch.rand_like(w) - 0.5
        noise[w == 0] = 0 # ignore zero entries from sparsification
        scale = compute_optimal_scale(w)
        noise = noise * scale.unsqueeze(-1)
        noise = noise.reshape(k, o, i).permute(2, 1, 0)
    elif len(weight.shape) == 2:
        noise = torch.rand_like(weight) - 0.5
        noise[weight == 0] = 0 # ignore zero entries from sparsification
        scale = compute_optimal_scale(weight)
        noise = noise * scale.unsqueeze(-1)
    else:
        raise ValueError('unknown quantization setting')

    return noise

class SoftQuant:
    name: str

    def __init__(self, names: str, scale: float) -> None:
        self.names = names
        self.quantization_noise = None
        self.scale = scale

    def __call__(self, module, inputs, *args, before=True):
        if not module.training: return

        if before:
            self.quantization_noise = dict()
            for name in self.names:
                weight = getattr(module, name)
                if self.scale is None:
                    self.quantization_noise[name] = q_scaled_noise(module, weight)
                else:
                    self.quantization_noise[name] = \
                        self.scale * (torch.rand_like(weight) - 0.5)
                with torch.no_grad():
                    weight.data[:] = weight + self.quantization_noise[name]
        else:
            for name in self.names:
                weight = getattr(module, name)
                with torch.no_grad():
                    weight.data[:] = weight - self.quantization_noise[name]
            self.quantization_noise = None

    def apply(module, names=['weight'], scale=None):
        fn = SoftQuant(names, scale)

        for name in names:
            if not hasattr(module, name):
                raise ValueError("")

        fn_before = lambda *x : fn(*x, before=True)
        fn_after = lambda *x : fn(*x, before=False)
        setattr(fn_before, 'sqm', fn)
        setattr(fn_after, 'sqm', fn)


        module.register_forward_pre_hook(fn_before)
        module.register_forward_hook(fn_after)

        module

        return fn


def soft_quant(module, names=['weight'], scale=None):
    fn = SoftQuant.apply(module, names, scale)
    return module

def remove_soft_quant(module, names=['weight']):
    for k, hook in module._forward_pre_hooks.items():
        if hasattr(hook, 'sqm'):
            if isinstance(hook.sqm, SoftQuant) and hook.sqm.names == names:
                del module._forward_pre_hooks[k]
    for k, hook in module._forward_hooks.items():
        if hasattr(hook, 'sqm'):
            if isinstance(hook.sqm, SoftQuant) and hook.sqm.names == names:
                del module._forward_hooks[k]

    return module
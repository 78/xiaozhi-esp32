import torch

from dnntools.sparsification import GRUSparsifier, LinearSparsifier, Conv1dSparsifier, ConvTranspose1dSparsifier

def mark_for_sparsification(module, params):
    setattr(module, 'sparsify', True)
    setattr(module, 'sparsification_params', params)
    return module

def create_sparsifier(module, start, stop, interval):
    sparsifier_list = []
    for m in module.modules():
        if hasattr(m, 'sparsify'):
            if isinstance(m, torch.nn.GRU):
                sparsifier_list.append(
                    GRUSparsifier([(m, m.sparsification_params)], start, stop, interval)
                )
            elif isinstance(m, torch.nn.Linear):
                sparsifier_list.append(
                    LinearSparsifier([(m, m.sparsification_params)], start, stop, interval)
                )
            elif isinstance(m, torch.nn.Conv1d):
                sparsifier_list.append(
                    Conv1dSparsifier([(m, m.sparsification_params)], start, stop, interval)
                )
            elif isinstance(m, torch.nn.ConvTranspose1d):
                sparsifier_list.append(
                    ConvTranspose1dSparsifier([(m, m.sparsification_params)], start, stop, interval)
                )
            else:
                print(f"[create_sparsifier] warning: module {m} marked for sparsification but no suitable sparsifier exists.")

    def sparsify(verbose=False):
        for sparsifier in sparsifier_list:
            sparsifier.step(verbose)

    return sparsify


def count_parameters(model, verbose=False):
    total = 0
    for name, p in model.named_parameters():
        count = torch.ones_like(p).sum().item()

        if verbose:
            print(f"{name}: {count} parameters")

        total += count

    return total

def estimate_nonzero_parameters(module):
    num_zero_parameters = 0
    if hasattr(module, 'sparsify'):
        params = module.sparsification_params
        if isinstance(module, torch.nn.Conv1d) or isinstance(module, torch.nn.ConvTranspose1d):
            num_zero_parameters = torch.ones_like(module.weight).sum().item() * (1 - params[0])
        elif isinstance(module, torch.nn.GRU):
            num_zero_parameters = module.input_size * module.hidden_size * (3 - params['W_ir'][0] - params['W_iz'][0] - params['W_in'][0])
            num_zero_parameters += module.hidden_size * module.hidden_size * (3 - params['W_hr'][0] - params['W_hz'][0] - params['W_hn'][0])
        elif isinstance(module, torch.nn.Linear):
            num_zero_parameters = module.in_features * module.out_features * params[0]
        else:
            raise ValueError(f'unknown sparsification method for module of type {type(module)}')

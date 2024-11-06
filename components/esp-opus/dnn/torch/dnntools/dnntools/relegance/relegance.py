"""
/* Copyright (c) 2023 Amazon
   Written by Jan Buethe */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import torch
import torch.nn.functional as F


def view_one_hot(index, length):
    vec = length * [1]
    vec[index] = -1
    return vec

def create_smoothing_kernel(widths, gamma=1.5):
    """ creates a truncated gaussian smoothing kernel for the given widths

        Parameters:
        -----------
        widths: list[Int] or torch.LongTensor
            specifies the shape of the smoothing kernel, entries must be > 0.

        gamma: float, optional
            decay factor for gaussian relative to kernel size

        Returns:
        --------
        kernel: torch.FloatTensor
    """

    widths = torch.LongTensor(widths)
    num_dims = len(widths)

    assert(widths.min() > 0)

    centers = widths.float() / 2 - 0.5
    sigmas  = gamma * (centers + 1)

    vals = []

    vals= [((torch.arange(widths[i]) - centers[i]) / sigmas[i]) ** 2 for i in range(num_dims)]
    vals = sum([vals[i].view(view_one_hot(i, num_dims)) for i in range(num_dims)])

    kernel = torch.exp(- vals)
    kernel = kernel / kernel.sum()

    return kernel


def create_partition_kernel(widths, strides):
    """ creates a partition kernel for mapping a convolutional network output back to the input domain

        Given a fully convolutional network with receptive field of shape widths and the given strides, this
        function construncts an intorpolation kernel whose tranlations by multiples of the given strides form
        a partition of one on the input domain.

        Parameter:
        ----------
        widths: list[Int] or torch.LongTensor
            shape of receptive field

        strides: list[Int] or torch.LongTensor
            total strides of convolutional network

        Returns:
        kernel: torch.FloatTensor
    """

    num_dims = len(widths)
    assert num_dims == len(strides) and num_dims in {1, 2, 3}

    convs = {1 : F.conv1d, 2 : F.conv2d, 3 : F.conv3d}

    widths = torch.LongTensor(widths)
    strides = torch.LongTensor(strides)

    proto_kernel = torch.ones(torch.minimum(strides, widths).tolist())

    # create interpolation kernel eta
    eta_widths = widths - strides + 1
    if eta_widths.min() <= 0:
        print("[create_partition_kernel] warning: receptive field does not cover input domain")
        eta_widths = torch.maximum(eta_widths, torch.ones_like(eta_widths))


    eta = create_smoothing_kernel(eta_widths).view(1, 1, *eta_widths.tolist())

    padding = torch.repeat_interleave(eta_widths - 1, 2, 0).tolist()[::-1] # ordering of dimensions for padding and convolution functions is reversed in torch
    padded_proto_kernel = F.pad(proto_kernel, padding)
    padded_proto_kernel = padded_proto_kernel.view(1, 1, *padded_proto_kernel.shape)
    kernel = convs[num_dims](padded_proto_kernel, eta)

    return kernel


def receptive_field(conv_model, input_shape, output_position):
    """ estimates boundaries of receptive field connected to output_position via autograd

        Parameters:
        -----------
        conv_model: nn.Module or autograd function
            function or model implementing fully convolutional model

        input_shape: List[Int]
            input shape ignoring batch dimension, i.e. [num_channels, dim1, dim2, ...]

        output_position: List[Int]
            output position for which the receptive field is determined; the function raises an exception
            if output_position is out of bounds for the given input_shape.

        Returns:
        --------
        low: List[Int]
            start indices of receptive field

        high: List[Int]
            stop indices of receptive field

    """

    x = torch.randn((1,) + tuple(input_shape), requires_grad=True)
    y = conv_model(x)

    # collapse channels and remove batch dimension
    y = torch.sum(y, 1)[0]

    # create mask
    mask = torch.zeros_like(y)
    index = [torch.tensor(i) for i in output_position]
    try:
        mask.index_put_(index, torch.tensor(1, dtype=mask.dtype))
    except IndexError:
        raise ValueError('output_position out of bounds')

    (mask * y).sum().backward()

    # sum over channels and remove batch dimension
    grad = torch.sum(x.grad, dim=1)[0]
    tmp = torch.nonzero(grad, as_tuple=True)
    low  = [t.min().item() for t in tmp]
    high = [t.max().item() for t in tmp]

    return low, high

def estimate_conv_parameters(model, num_channels, num_dims, width, max_stride=10):
    """ attempts to estimate receptive field size, strides and left paddings for given model


        Parameters:
        -----------
        model: nn.Module or autograd function
            fully convolutional model for which parameters are estimated

        num_channels: Int
            number of input channels for model

        num_dims: Int
            number of input dimensions for model (without channel dimension)

        width: Int
            width of the input tensor (a hyper-square) on which the receptive fields are derived via autograd

        max_stride: Int, optional
            assumed maximal stride of the model for any dimension, when set too low the function may fail for
            any value of width

        Returns:
        --------
        receptive_field_size: List[Int]
            receptive field size in all dimension

        strides: List[Int]
            stride in all dimensions

        left_paddings: List[Int]
            left padding in all dimensions; this is relevant for aligning the receptive field on the input plane

        Raises:
        -------
        ValueError, KeyError

    """

    input_shape = [num_channels] + num_dims * [width]
    output_position1 = num_dims * [width // (2 * max_stride)]
    output_position2 = num_dims * [width // (2 * max_stride) + 1]

    low1, high1 = receptive_field(model, input_shape, output_position1)
    low2, high2 = receptive_field(model, input_shape, output_position2)

    widths1 = [h - l + 1 for l, h in zip(low1, high1)]
    widths2 = [h - l + 1 for l, h in zip(low2, high2)]

    if not all([w1 - w2 == 0 for w1, w2 in zip(widths1, widths2)]) or not all([l1 != l2 for l1, l2 in zip(low1, low2)]):
        raise ValueError("[estimate_strides]: widths to small to determine strides")

    receptive_field_size = widths1
    strides              = [l2 - l1 for l1, l2 in zip(low1, low2)]
    left_paddings        = [s * p - l for l, s, p in zip(low1, strides, output_position1)]

    return receptive_field_size, strides, left_paddings

def inspect_conv_model(model, num_channels, num_dims, max_width=10000, width_hint=None, stride_hint=None, verbose=False):
    """ determines size of receptive field, strides and padding probabilistically


        Parameters:
        -----------
        model: nn.Module or autograd function
            fully convolutional model for which parameters are estimated

        num_channels: Int
            number of input channels for model

        num_dims: Int
            number of input dimensions for model (without channel dimension)

        max_width: Int
            maximum width of the input tensor (a hyper-square) on which the receptive fields are derived via autograd

        verbose: bool, optional
            if true, the function prints parameters for individual trials

        Returns:
        --------
        receptive_field_size: List[Int]
            receptive field size in all dimension

        strides: List[Int]
            stride in all dimensions

        left_paddings: List[Int]
            left padding in all dimensions; this is relevant for aligning the receptive field on the input plane

        Raises:
        -------
        ValueError

    """

    max_stride = max_width // 2
    stride = max_stride // 100
    width = max_width // 100

    if width_hint is not None: width = 2 * width_hint
    if stride_hint is not None: stride = stride_hint

    did_it = False
    while width < max_width and stride < max_stride:
        try:
            if verbose: print(f"[inspect_conv_model] trying parameters {width=}, {stride=}")
            receptive_field_size, strides, left_paddings = estimate_conv_parameters(model, num_channels, num_dims, width, stride)
            did_it = True
        except:
            pass

        if did_it: break

        width *= 2
        if width >= max_width and stride < max_stride:
            stride *= 2
            width = 2 * stride

    if not did_it:
        raise ValueError(f'could not determine conv parameter with given max_width={max_width}')

    return receptive_field_size, strides, left_paddings


class GradWeight(torch.autograd.Function):
    def __init__(self):
        super().__init__()

    @staticmethod
    def forward(ctx, x, weight):
        ctx.save_for_backward(weight)
        return x.clone()

    @staticmethod
    def backward(ctx, grad_output):
        weight, = ctx.saved_tensors

        grad_input = grad_output * weight

        return grad_input, None


# API

def relegance_gradient_weighting(x, weight):
    """

    Args:
        x (torch.tensor): input tensor
        weight (torch.tensor or None): weight tensor for gradients of x; if None, no gradient weighting will be applied in backward pass

    Returns:
        torch.tensor: the unmodified input tensor x

    Raises:
        RuntimeError: if estimation of parameters fails due to exceeded compute budget
    """
    if weight is None:
        return x
    else:
        return GradWeight.apply(x, weight)



def relegance_create_tconv_kernel(model, num_channels, num_dims, width_hint=None, stride_hint=None, verbose=False):
    """ creates parameters for mapping back output domain relevance to input tomain

    Args:
        model (nn.Module or autograd.Function): fully convolutional model
        num_channels (int): number of input channels to model
        num_dims (int): number of input dimensions of model (without channel and batch dimension)
        width_hint(int or None): optional hint at maximal width of receptive field
        stride_hint(int or None): optional hint at maximal stride

    Returns:
        dict: contains kernel, kernel dimensions, strides and left paddings for transposed convolution
    """

    max_width = int(100000 / (10 ** num_dims))

    did_it = False
    try:
        receptive_field_size, strides, left_paddings = inspect_conv_model(model, num_channels, num_dims, max_width=max_width, width_hint=width_hint, stride_hint=stride_hint, verbose=verbose)
        did_it = True
    except:
        # try once again with larger max_width
        max_width *= 10

    # crash if exception is raised
    try:
        if not did_it: receptive_field_size, strides, left_paddings = inspect_conv_model(model, num_channels, num_dims, max_width=max_width, width_hint=width_hint, stride_hint=stride_hint, verbose=verbose)
    except:
        raise RuntimeError("could not determine parameters within given compute budget")

    partition_kernel = create_partition_kernel(receptive_field_size, strides)
    partition_kernel = torch.repeat_interleave(partition_kernel, num_channels, 1)

    tconv_parameters = {
        'kernel': partition_kernel,
        'receptive_field_shape': receptive_field_size,
        'stride': strides,
        'left_padding': left_paddings,
        'num_dims': num_dims
    }

    return tconv_parameters



def relegance_map_relevance_to_input_domain(od_relevance, tconv_parameters):
    """ maps output-domain relevance to input-domain relevance via transpose convolution

    Args:
        od_relevance (torch.tensor): output-domain relevance
        tconv_parameters (dict): parameter dict as created by relegance_create_tconv_kernel

    Returns:
        torch.tensor: input-domain relevance. The tensor is left aligned, i.e. the all-zero index of the output corresponds to the all-zero index of the discriminator input.
                      Otherwise, the size of the output tensor does not need to match the size of the discriminator input. Use relegance_resize_relevance_to_input_size for a
                      convenient way to adjust the output to the correct size.

    Raises:
        ValueError: if number of dimensions is not supported
    """

    kernel       = tconv_parameters['kernel'].to(od_relevance.device)
    rf_shape     = tconv_parameters['receptive_field_shape']
    stride       = tconv_parameters['stride']
    left_padding = tconv_parameters['left_padding']

    num_dims = len(kernel.shape) - 2

    # repeat boundary values
    od_padding = [rf_shape[i//2] // stride[i//2] + 1 for i in range(2 * num_dims)]
    padded_od_relevance = F.pad(od_relevance, od_padding[::-1], mode='replicate')
    od_padding = od_padding[::2]

    # apply mapping and left trimming
    if num_dims == 1:
        id_relevance = F.conv_transpose1d(padded_od_relevance, kernel, stride=stride)
        id_relevance = id_relevance[..., left_padding[0] + stride[0] * od_padding[0] :]
    elif num_dims == 2:
        id_relevance = F.conv_transpose2d(padded_od_relevance, kernel, stride=stride)
        id_relevance = id_relevance[..., left_padding[0] + stride[0] * od_padding[0] :, left_padding[1] + stride[1] * od_padding[1]:]
    elif num_dims == 3:
        id_relevance = F.conv_transpose2d(padded_od_relevance, kernel, stride=stride)
        id_relevance = id_relevance[..., left_padding[0] + stride[0] * od_padding[0] :, left_padding[1] + stride[1] * od_padding[1]:, left_padding[2] + stride[2] * od_padding[2] :]
    else:
        raise ValueError(f'[relegance_map_to_input_domain] error: num_dims = {num_dims} not supported')

    return id_relevance


def relegance_resize_relevance_to_input_size(reference_input, relevance):
    """ adjusts size of relevance tensor to reference input size

    Args:
        reference_input (torch.tensor): discriminator input tensor for reference
        relevance (torch.tensor): input-domain relevance corresponding to input tensor reference_input

    Returns:
        torch.tensor: resized relevance

    Raises:
        ValueError: if number of dimensions is not supported
    """
    resized_relevance = torch.zeros_like(reference_input)

    num_dims = len(reference_input.shape) - 2
    with torch.no_grad():
        if num_dims == 1:
            resized_relevance[:] = relevance[..., : min(reference_input.size(-1), relevance.size(-1))]
        elif num_dims == 2:
            resized_relevance[:] = relevance[..., : min(reference_input.size(-2), relevance.size(-2)), : min(reference_input.size(-1), relevance.size(-1))]
        elif num_dims == 3:
            resized_relevance[:] = relevance[..., : min(reference_input.size(-3), relevance.size(-3)), : min(reference_input.size(-2), relevance.size(-2)), : min(reference_input.size(-1), relevance.size(-1))]
        else:
            raise ValueError(f'[relegance_map_to_input_domain] error: num_dims = {num_dims} not supported')

    return resized_relevance
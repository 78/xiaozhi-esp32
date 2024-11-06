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

""" module for inspecting models during inference """

import os

import yaml
import matplotlib.pyplot as plt
import matplotlib.animation as animation

import torch
import numpy as np

# stores entries {key : {'fid' : fid, 'fs' : fs, 'dim' : dim, 'dtype' : dtype}}
_state = dict()
_folder = 'endoscopy'

def get_gru_gates(gru, input, state):
    hidden_size = gru.hidden_size

    direct = torch.matmul(gru.weight_ih_l0, input.squeeze())
    recurrent = torch.matmul(gru.weight_hh_l0, state.squeeze())

    # reset gate
    start, stop = 0 * hidden_size, 1 * hidden_size
    reset_gate = torch.sigmoid(direct[start : stop] + gru.bias_ih_l0[start : stop] + recurrent[start : stop] + gru.bias_hh_l0[start : stop])

    # update gate
    start, stop = 1 * hidden_size, 2 * hidden_size
    update_gate = torch.sigmoid(direct[start : stop] + gru.bias_ih_l0[start : stop] + recurrent[start : stop] + gru.bias_hh_l0[start : stop])

    # new gate
    start, stop = 2 * hidden_size, 3 * hidden_size
    new_gate = torch.tanh(direct[start : stop] + gru.bias_ih_l0[start : stop] + reset_gate * (recurrent[start : stop] +  gru.bias_hh_l0[start : stop]))

    return {'reset_gate' : reset_gate, 'update_gate' : update_gate, 'new_gate' : new_gate}


def init(folder='endoscopy'):
    """ sets up output folder for endoscopy data """

    global _folder
    _folder = folder

    if not os.path.exists(folder):
        os.makedirs(folder)
    else:
        print(f"warning: endoscopy folder {folder} exists. Content may be lost or inconsistent results may occur.")

def write_data(key, data, fs):
    """ appends data to previous data written under key """

    global _state

    # convert to numpy if torch.Tensor is given
    if isinstance(data, torch.Tensor):
        data = data.detach().numpy()

    if not key in _state:
        _state[key] = {
            'fid'   : open(os.path.join(_folder, key + '.bin'), 'wb'),
            'fs'    : fs,
            'dim'   : tuple(data.shape),
            'dtype' : str(data.dtype)
        }

        with open(os.path.join(_folder, key + '.yml'), 'w') as f:
            f.write(yaml.dump({'fs' : fs, 'dim' : tuple(data.shape), 'dtype' : str(data.dtype).split('.')[-1]}))
    else:
        if _state[key]['fs'] != fs:
            raise ValueError(f"fs changed for key {key}: {_state[key]['fs']} vs. {fs}")
        if _state[key]['dtype'] != str(data.dtype):
            raise ValueError(f"dtype changed for key {key}: {_state[key]['dtype']} vs. {str(data.dtype)}")
        if _state[key]['dim'] != tuple(data.shape):
            raise ValueError(f"dim changed for key {key}: {_state[key]['dim']} vs. {tuple(data.shape)}")

    _state[key]['fid'].write(data.tobytes())

def close(folder='endoscopy'):
    """ clean up """
    for key in _state.keys():
        _state[key]['fid'].close()


def read_data(folder='endoscopy'):
    """ retrieves written data as numpy arrays """


    keys = [name[:-4] for name in os.listdir(folder) if name.endswith('.yml')]

    return_dict = dict()

    for key in keys:
        with open(os.path.join(folder, key + '.yml'), 'r') as f:
            value = yaml.load(f.read(), yaml.FullLoader)

        with open(os.path.join(folder, key + '.bin'), 'rb') as f:
            data = np.frombuffer(f.read(), dtype=value['dtype'])

        value['data'] = data.reshape((-1,) + value['dim'])

        return_dict[key] = value

    return return_dict

def get_best_reshape(shape, target_ratio=1):
    """ calculated the best 2d reshape of shape given the target ratio (rows/cols)"""

    if len(shape) > 1:
        pixel_count = 1
        for s in shape:
            pixel_count *= s
    else:
        pixel_count = shape[0]

    if pixel_count == 1:
        return (1,)

    num_columns = int((pixel_count / target_ratio)**.5)

    while (pixel_count % num_columns):
        num_columns -= 1

    num_rows = pixel_count // num_columns

    return (num_rows, num_columns)

def get_type_and_shape(shape):

    # can happen if data is one dimensional
    if len(shape) == 0:
        shape = (1,)

    # calculate pixel count
    if len(shape) > 1:
        pixel_count = 1
        for s in shape:
            pixel_count *= s
    else:
        pixel_count = shape[0]

    if pixel_count == 1:
        return 'plot', (1, )

    # stay with shape if already 2-dimensional
    if len(shape) == 2:
        if (shape[0] != pixel_count) or (shape[1] != pixel_count):
            return 'image', shape

    return 'image', get_best_reshape(shape)

def make_animation(data, filename, start_index=80, stop_index=-80, interval=20, half_signal_window_length=80):

    # determine plot setup
    num_keys = len(data.keys())

    num_rows = int((num_keys * 3/4) ** .5)

    num_cols = (num_keys + num_rows - 1) // num_rows

    fig, axs = plt.subplots(num_rows, num_cols)
    fig.set_size_inches(num_cols * 5, num_rows * 5)

    display = dict()

    fs_max = max([val['fs'] for val in data.values()])

    num_samples = max([val['data'].shape[0] for val in data.values()])

    keys = sorted(data.keys())

    # inspect data
    for i, key in enumerate(keys):
        axs[i // num_cols, i % num_cols].title.set_text(key)

        display[key] = dict()

        display[key]['type'], display[key]['shape'] = get_type_and_shape(data[key]['dim'])
        display[key]['down_factor'] = data[key]['fs'] / fs_max

    start_index = max(start_index, half_signal_window_length)
    while stop_index < 0:
        stop_index += num_samples

    stop_index = min(stop_index, num_samples - half_signal_window_length)

    # actual plotting
    frames = []
    for index in range(start_index, stop_index):
        ims = []
        for i, key in enumerate(keys):
            feature_index = int(round(index * display[key]['down_factor']))

            if display[key]['type'] == 'plot':
                ims.append(axs[i // num_cols, i % num_cols].plot(data[key]['data'][index - half_signal_window_length : index + half_signal_window_length], marker='P', markevery=[half_signal_window_length], animated=True, color='blue')[0])

            elif display[key]['type'] == 'image':
                ims.append(axs[i // num_cols, i % num_cols].imshow(data[key]['data'][index].reshape(display[key]['shape']), animated=True))

        frames.append(ims)

    ani = animation.ArtistAnimation(fig, frames, interval=interval, blit=True, repeat_delay=1000)

    if not filename.endswith('.mp4'):
        filename += '.mp4'

    ani.save(filename)
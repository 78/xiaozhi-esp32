"""
/* Copyright (c) 2022 Amazon
   Written by Jan Buethe and Jean-Marc Valin */
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

import os
import subprocess
import argparse

os.environ['CUDA_VISIBLE_DEVICES'] = ""

parser = argparse.ArgumentParser(description='Encode redundancy for Opus neural FEC. Designed for use with voip application and 20ms frames')

parser.add_argument('input', metavar='<input signal>', help='audio input (.wav or .raw or .pcm as int16)')
parser.add_argument('checkpoint', metavar='<weights>', help='model checkpoint')
parser.add_argument('q0', metavar='<quant level 0>', type=int, help='quantization level for most recent frame')
parser.add_argument('q1', metavar='<quant level 1>', type=int, help='quantization level for oldest frame')
parser.add_argument('output', type=str, help='output file (will be extended with .fec)')

parser.add_argument('--dump-data', type=str, default='./dump_data', help='path to dump data executable (default ./dump_data)')
parser.add_argument('--num-redundancy-frames', default=52, type=int, help='number of redundancy frames per packet (default 52)')
parser.add_argument('--extra-delay', default=0, type=int, help="last features in packet are calculated with the decoder aligned samples, use this option to add extra delay (in samples at 16kHz)")
parser.add_argument('--lossfile', type=str, help='file containing loss trace (0 for frame received, 1 for lost)')
parser.add_argument('--debug-output', action='store_true', help='if set, differently assembled features are written to disk')

args = parser.parse_args()

import numpy as np
from scipy.io import wavfile
import torch

from rdovae import RDOVAE
from packets import write_fec_packets

torch.set_num_threads(4)

checkpoint = torch.load(args.checkpoint, map_location="cpu")
model = RDOVAE(*checkpoint['model_args'], **checkpoint['model_kwargs'])
model.load_state_dict(checkpoint['state_dict'], strict=False)
model.to("cpu")

lpc_order = 16

## prepare input signal
# SILK frame size is 20ms and LPCNet subframes are 10ms
subframe_size = 160
frame_size = 2 * subframe_size

# 91 samples delay to align with SILK decoded frames
silk_delay = 91

# prepend zeros to have enough history to produce the first package
zero_history = (args.num_redundancy_frames - 1) * frame_size

# dump data has a (feature) delay of 10ms
dump_data_delay = 160

total_delay = silk_delay + zero_history + args.extra_delay - dump_data_delay

# load signal
if args.input.endswith('.raw') or args.input.endswith('.pcm'):
    signal = np.fromfile(args.input, dtype='int16')

elif args.input.endswith('.wav'):
    fs, signal = wavfile.read(args.input)
else:
    raise ValueError(f'unknown input signal format: {args.input}')

# fill up last frame with zeros
padded_signal_length = len(signal) + total_delay
tail = padded_signal_length % frame_size
right_padding = (frame_size - tail) % frame_size

signal = np.concatenate((np.zeros(total_delay, dtype=np.int16), signal, np.zeros(right_padding, dtype=np.int16)))

padded_signal_file  = os.path.splitext(args.input)[0] + '_padded.raw'
signal.tofile(padded_signal_file)

# write signal and call dump_data to create features

feature_file = os.path.splitext(args.input)[0] + '_features.f32'
command = f"{args.dump_data} -test {padded_signal_file} {feature_file}"
r = subprocess.run(command, shell=True)
if r.returncode != 0:
    raise RuntimeError(f"command '{command}' failed with exit code {r.returncode}")

# load features
nb_features = model.feature_dim + lpc_order
nb_used_features = model.feature_dim

# load features
features = np.fromfile(feature_file, dtype='float32')
num_subframes = len(features) // nb_features
num_subframes = 2 * (num_subframes // 2)
num_frames = num_subframes // 2

features = np.reshape(features, (1, -1, nb_features))
features = features[:, :, :nb_used_features]
features = features[:, :num_subframes, :]

# quant_ids in reverse decoding order
quant_ids = torch.round((args.q1 + (args.q0 - args.q1) * torch.arange(args.num_redundancy_frames // 2) / (args.num_redundancy_frames // 2 - 1))).long()

print(f"using quantization levels {quant_ids}...")

# convert input to torch tensors
features = torch.from_numpy(features)


# run encoder
print("running fec encoder...")
with torch.no_grad():

    # encoding
    z, states, state_size = model.encode(features)


    # decoder on packet chunks
    input_length = args.num_redundancy_frames // 2
    offset = args.num_redundancy_frames - 1

    packets = []
    packet_sizes = []

    for i in range(offset, num_frames):
        print(f"processing frame {i - offset}...")
        # quantize / unquantize latent vectors
        zi = torch.clone(z[:, i - 2 * input_length + 2: i + 1 : 2, :])
        zi, rates = model.quantize(zi, quant_ids)
        zi = model.unquantize(zi, quant_ids)

        features = model.decode(zi, states[:, i : i + 1, :])
        packets.append(features.squeeze(0).numpy())
        packet_size = 8 * int((torch.sum(rates) + 7 + state_size) / 8)
        packet_sizes.append(packet_size)


# write packets
packet_file = args.output + '.fec' if not args.output.endswith('.fec') else args.output
write_fec_packets(packet_file, packets, packet_sizes)


print(f"average redundancy rate: {int(round(sum(packet_sizes) / len(packet_sizes) * 50 / 1000))} kbps")

# assemble features according to loss file
if args.lossfile != None:
    num_packets = len(packets)
    loss = np.loadtxt(args.lossfile, dtype='int16')
    fec_out = np.zeros((num_packets * 2, packets[0].shape[-1]), dtype='float32')
    foffset = -2
    ptr = 0
    count = 2
    for i in range(num_packets):
        if (loss[i] == 0) or (i == num_packets - 1):

            fec_out[ptr:ptr+count,:] = packets[i][foffset:, :]

            ptr    += count
            foffset = -2
            count   = 2
        else:
            count   += 2
            foffset -= 2

    fec_out_full = np.zeros((fec_out.shape[0], 36), dtype=np.float32)
    fec_out_full[:, : fec_out.shape[-1]] = fec_out

    fec_out_full.tofile(packet_file[:-4] + f'_fec.f32')


if args.debug_output:
    import itertools

    batches = [4]
    offsets = [0, 2 * args.num_redundancy_frames - 4]

    # sanity checks
    # 1. concatenate features at offset 0
    for batch, offset in itertools.product(batches, offsets):

        stop = packets[0].shape[1] - offset
        test_features = np.concatenate([packet[stop - batch: stop, :] for packet in packets[::batch//2]], axis=0)

        test_features_full = np.zeros((test_features.shape[0], nb_features), dtype=np.float32)
        test_features_full[:, :nb_used_features] = test_features[:, :]

        print(f"writing debug output {packet_file[:-4] + f'_torch_batch{batch}_offset{offset}.f32'}")
        test_features_full.tofile(packet_file[:-4] + f'_torch_batch{batch}_offset{offset}.f32')

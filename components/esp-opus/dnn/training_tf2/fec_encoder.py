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


import numpy as np
from scipy.io import wavfile
import tensorflow as tf

from rdovae import new_rdovae_model, pvq_quantize, apply_dead_zone, sq_rate_metric
from fec_packets import write_fec_packets, read_fec_packets


debug = False

if debug:
    args = type('dummy', (object,),
    {
        'input' : 'item1.wav',
        'weights' : 'testout/rdovae_alignment_fix_1024_120.h5',
        'enc_lambda' : 0.0007,
        'output' : "test_0007.fec",
        'cond_size' : 1024,
        'num_redundancy_frames' : 64,
        'extra_delay' : 0,
        'dump_data' : './dump_data'
    })()
    os.environ['CUDA_VISIBLE_DEVICES']=""
else:
    parser = argparse.ArgumentParser(description='Encode redundancy for Opus neural FEC. Designed for use with voip application and 20ms frames')

    parser.add_argument('input', metavar='<input signal>', help='audio input (.wav or .raw or .pcm as int16)')
    parser.add_argument('weights', metavar='<weights>', help='trained model file (.h5)')
#    parser.add_argument('enc_lambda', metavar='<lambda>', type=float, help='lambda for controlling encoder rate')
    parser.add_argument('output', type=str, help='output file (will be extended with .fec)')

    parser.add_argument('--dump-data', type=str, default='./dump_data', help='path to dump data executable (default ./dump_data)')
    parser.add_argument('--cond-size', metavar='<units>', default=1024, type=int, help='number of units in conditioning network (default 1024)')
    parser.add_argument('--quant-levels', type=int, help="number of quantization steps (default: 40)", default=40)
    parser.add_argument('--num-redundancy-frames', default=64, type=int, help='number of redundancy frames (20ms) per packet (default 64)')
    parser.add_argument('--extra-delay', default=0, type=int, help="last features in packet are calculated with the decoder aligned samples, use this option to add extra delay (in samples at 16kHz)")
    parser.add_argument('--lossfile', type=str, help='file containing loss trace (0 for frame received, 1 for lost)')

    parser.add_argument('--debug-output', action='store_true', help='if set, differently assembled features are written to disk')

    args = parser.parse_args()

model, encoder, decoder, qembedding = new_rdovae_model(nb_used_features=20, nb_bits=80, batch_size=1, nb_quant=args.quant_levels, cond_size=args.cond_size)
model.load_weights(args.weights)

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
if args.input.endswith('.raw') or args.input.endswith('.pcm') or args.input.endswith('.sw'):
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
nb_features = model.nb_used_features + lpc_order
nb_used_features = model.nb_used_features

# load features
features = np.fromfile(feature_file, dtype='float32')
num_subframes = len(features) // nb_features
num_subframes = 2 * (num_subframes // 2)
num_frames = num_subframes // 2

features = np.reshape(features, (1, -1, nb_features))
features = features[:, :, :nb_used_features]
features = features[:, :num_subframes, :]

#variable quantizer depending on the delay
q0 = 3
q1 = 15
quant_id = np.round(q1 + (q0-q1)*np.arange(args.num_redundancy_frames//2)/args.num_redundancy_frames).astype('int16')
#print(quant_id)

quant_embed = qembedding(quant_id)

# run encoder
print("running fec encoder...")
symbols, gru_state_dec = encoder.predict(features)

# apply quantization
nsymbols = 80
quant_scale = tf.math.softplus(quant_embed[:, :nsymbols]).numpy()
dead_zone = tf.math.softplus(quant_embed[:, nsymbols : 2 * nsymbols]).numpy()
#symbols = apply_dead_zone([symbols, dead_zone]).numpy()
#qsymbols = np.round(symbols)
quant_gru_state_dec = pvq_quantize(gru_state_dec, 82)

# rate estimate
hard_distr_embed = tf.math.sigmoid(quant_embed[:, 4 * nsymbols : ]).numpy()
#rate_input = np.concatenate((qsymbols, hard_distr_embed, enc_lambda), axis=-1)
#rates = sq_rate_metric(None, rate_input, reduce=False).numpy()

# run decoder
input_length = args.num_redundancy_frames // 2
offset = args.num_redundancy_frames - 1

packets = []
packet_sizes = []

sym_batch = np.zeros((num_frames-offset, args.num_redundancy_frames//2, nsymbols), dtype='float32')
quant_state = quant_gru_state_dec[0, offset:num_frames, :]
#pack symbols for batch processing
for i in range(offset, num_frames):
    sym_batch[i-offset, :, :] = symbols[0, i - 2 * input_length + 2 : i + 1 : 2, :]

#quantize symbols
sym_batch = sym_batch * quant_scale
sym_batch = apply_dead_zone([sym_batch, dead_zone]).numpy()
sym_batch = np.round(sym_batch)

hard_distr_embed = np.broadcast_to(hard_distr_embed, (sym_batch.shape[0], sym_batch.shape[1], 2*sym_batch.shape[2]))
fake_lambda = np.ones((sym_batch.shape[0], sym_batch.shape[1], 1), dtype='float32')
rate_input = np.concatenate((sym_batch, hard_distr_embed, fake_lambda), axis=-1)
rates = sq_rate_metric(None, rate_input, reduce=False).numpy()
#print(rates.shape)
print("average rate = ", np.mean(rates[args.num_redundancy_frames:,:]))

#sym_batch.tofile('qsyms.f32')

sym_batch = sym_batch / quant_scale
#print(sym_batch.shape, quant_state.shape)
#features = decoder.predict([sym_batch, quant_state])
features = decoder([sym_batch, quant_state])

#for i in range(offset, num_frames):
#    print(f"processing frame {i - offset}...")
#    features = decoder.predict([qsymbols[:, i - 2 * input_length + 2 : i + 1 : 2, :], quant_embed_dec[:, i - 2 * input_length + 2 : i + 1 : 2, :], quant_gru_state_dec[:, i, :]])
#    packets.append(features)
#    packet_size = 8 * int((np.sum(rates[:, i - 2 * input_length + 2 : i + 1 : 2]) + 7) / 8) + 64
#    packet_sizes.append(packet_size)


# write packets
packet_file = args.output + '.fec' if not args.output.endswith('.fec') else args.output
#write_fec_packets(packet_file, packets, packet_sizes)


#print(f"average redundancy rate: {int(round(sum(packet_sizes) / len(packet_sizes) * 50 / 1000))} kbps")

if args.lossfile != None:
    loss = np.loadtxt(args.lossfile, dtype='int16')
    fec_out = np.zeros((features.shape[0]*2, features.shape[-1]), dtype='float32')
    foffset = -2
    ptr = 0;
    count = 2;
    for i in range(features.shape[0]):
        if (loss[i] == 0) or (i == features.shape[0]-1):
            fec_out[ptr:ptr+count,:] = features[i, foffset:, :]
            #print("filled ", count)
            foffset = -2
            ptr = ptr+count
            count = 2
        else:
            count = count + 2
            foffset = foffset - 2

    fec_out_full = np.zeros((fec_out.shape[0], nb_features), dtype=np.float32)
    fec_out_full[:, :nb_used_features] = fec_out

    fec_out_full.tofile(packet_file[:-4] + f'_fec.f32')


#create packets array like in the original version for debugging purposes
for i in range(offset, num_frames):
    packets.append(features[i-offset:i-offset+1, :, :])

if args.debug_output:
    import itertools

    #batches = [2, 4]
    batches = [4]
    #offsets = [0, 4, 20]
    offsets = [0, (args.num_redundancy_frames - 2)*2]
    # sanity checks
    # 1. concatenate features at offset 0
    for batch, offset in itertools.product(batches, offsets):

        stop = packets[0].shape[1] - offset
        print(batch, offset, stop)
        test_features = np.concatenate([packet[:,stop - batch: stop, :] for packet in packets[::batch//2]], axis=1)

        test_features_full = np.zeros((test_features.shape[1], nb_features), dtype=np.float32)
        test_features_full[:, :nb_used_features] = test_features[0, :, :]

        print(f"writing debug output {packet_file[:-4] + f'_tf_batch{batch}_offset{offset}.f32'}")
        test_features_full.tofile(packet_file[:-4] + f'_tf_batch{batch}_offset{offset}.f32')

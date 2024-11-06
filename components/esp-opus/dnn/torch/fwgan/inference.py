import os
import time
import torch
import numpy as np
from scipy import signal as si
from scipy.io import wavfile
import argparse

from models import model_dict

parser = argparse.ArgumentParser()
parser.add_argument('model', choices=['fwgan400', 'fwgan500'], help='model name')
parser.add_argument('weightfile', type=str, help='weight file')
parser.add_argument('input', type=str, help='input: feature file or folder with feature files')
parser.add_argument('output', type=str, help='output: wav file name or folder name, depending on input')


########################### Signal Processing Layers ###########################

def preemphasis(x, coef= -0.85):

    return si.lfilter(np.array([1.0, coef]), np.array([1.0]), x).astype('float32')

def deemphasis(x, coef= -0.85):

    return si.lfilter(np.array([1.0]), np.array([1.0, coef]), x).astype('float32')

gamma = 0.92
weighting_vector = np.array([gamma**i for i in range(16,0,-1)])


def lpc_synthesis_one_frame(frame, filt, buffer, weighting_vector=np.ones(16)):

    out = np.zeros_like(frame)

    filt = np.flip(filt)

    inp = frame[:]


    for i in range(0, inp.shape[0]):

        s = inp[i] - np.dot(buffer*weighting_vector, filt)

        buffer[0] = s

        buffer = np.roll(buffer, -1)

        out[i] = s

    return out

def inverse_perceptual_weighting (pw_signal, filters, weighting_vector):

    #inverse perceptual weighting= H_preemph / W(z/gamma)

    pw_signal = preemphasis(pw_signal)

    signal = np.zeros_like(pw_signal)
    buffer = np.zeros(16)
    num_frames = pw_signal.shape[0] //160
    assert num_frames == filters.shape[0]

    for frame_idx in range(0, num_frames):

        in_frame = pw_signal[frame_idx*160: (frame_idx+1)*160][:]
        out_sig_frame = lpc_synthesis_one_frame(in_frame, filters[frame_idx, :], buffer, weighting_vector)
        signal[frame_idx*160: (frame_idx+1)*160] = out_sig_frame[:]
        buffer[:] = out_sig_frame[-16:]

    return signal


def process_item(generator, feature_filename, output_filename, verbose=False):

    feat = np.memmap(feature_filename, dtype='float32', mode='r')

    num_feat_frames = len(feat) // 36
    feat = np.reshape(feat, (num_feat_frames, 36))

    bfcc = np.copy(feat[:, :18])
    corr = np.copy(feat[:, 19:20]) + 0.5
    bfcc_with_corr =  torch.from_numpy(np.hstack((bfcc, corr))).type(torch.FloatTensor).unsqueeze(0)#.to(device)

    period = torch.from_numpy((0.1 + 50 * np.copy(feat[:, 18:19]) + 100)\
                            .astype('int32')).type(torch.long).view(1,-1)#.to(device)

    lpc_filters = np.copy(feat[:, -16:])

    start_time = time.time()
    x1 = generator(period, bfcc_with_corr, torch.zeros(1,320)) #this means the vocoder runs in complete synthesis mode with zero history audio frames
    end_time = time.time()
    total_time = end_time - start_time
    x1 = x1.squeeze(1).squeeze(0).detach().cpu().numpy()
    gen_seconds = len(x1)/16000
    out = deemphasis(inverse_perceptual_weighting(x1, lpc_filters, weighting_vector))
    if verbose:
        print(f"Took {total_time:.3f}s to generate {len(x1)}  samples ({gen_seconds}s) -> {gen_seconds/total_time:.2f}x real time")

    out = np.clip(np.round(2**15 * out), -2**15, 2**15 -1).astype(np.int16)
    wavfile.write(output_filename, 16000, out)


########################### The inference loop over folder containing lpcnet feature files #################################
if __name__ == "__main__":

    args = parser.parse_args()

    generator = model_dict[args.model]()


    #Load the FWGAN500Hz Checkpoint
    saved_gen= torch.load(args.weightfile, map_location='cpu')
    generator.load_state_dict(saved_gen)

    #this is just to remove the weight_norm from the model layers as it's no longer needed
    def _remove_weight_norm(m):
        try:
            torch.nn.utils.remove_weight_norm(m)
        except ValueError:  # this module didn't have weight norm
            return
    generator.apply(_remove_weight_norm)

    #enable inference mode
    generator = generator.eval()

    print('Successfully loaded the generator model ... start generation:')

    if os.path.isdir(args.input):

        os.makedirs(args.output, exist_ok=True)

        for fn in os.listdir(args.input):
            print(f"processing input {fn}...")
            feature_filename = os.path.join(args.input, fn)
            output_filename = os.path.join(args.output, os.path.splitext(fn)[0] + f"_{args.model}.wav")
            process_item(generator, feature_filename, output_filename)
    else:
        process_item(generator, args.input, args.output)

    print("Finished!")
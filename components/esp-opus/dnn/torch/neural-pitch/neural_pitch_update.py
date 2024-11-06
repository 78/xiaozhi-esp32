import argparse
parser = argparse.ArgumentParser()

parser.add_argument('features', type=str, help='Features generated from dump_data')
parser.add_argument('data', type=str, help='Data generated from dump_data (offset by 5ms)')
parser.add_argument('output', type=str, help='output .f32 feature file with replaced neural pitch')
parser.add_argument('checkpoint', type=str, help='model checkpoint file')
parser.add_argument('path_lpcnet_extractor', type=str, help='path to LPCNet extractor object file (generated on compilation)')
parser.add_argument('--device', type=str, help='compute device',default = None,required = False)
parser.add_argument('--replace_xcorr', type = bool, default = False, help='Replace LPCNet xcorr with updated one')

args = parser.parse_args()

import os

from utils import stft, random_filter
import subprocess
import numpy as np
import json
import torch
import tqdm

from models import PitchDNNIF, PitchDNNXcorr, PitchDNN

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
if device is not None:
    device = torch.device(args.device)

# Loading the appropriate model
checkpoint = torch.load(args.checkpoint, map_location='cpu')
dict_params = checkpoint['config']

if dict_params['data_format'] == 'if':
    pitch_nn = PitchDNNIF(dict_params['freq_keep']*3, dict_params['gru_dim'], dict_params['output_dim'])
elif dict_params['data_format'] == 'xcorr':
    pitch_nn = PitchDNNXcorr(dict_params['xcorr_dim'], dict_params['gru_dim'], dict_params['output_dim'])
else:
    pitch_nn = PitchDNN(dict_params['freq_keep']*3, dict_params['xcorr_dim'], dict_params['gru_dim'], dict_params['output_dim'])

pitch_nn.load_state_dict(checkpoint['state_dict'])
pitch_nn = pitch_nn.to(device)

N = dict_params['window_size']
H = dict_params['hop_factor']
freq_keep = dict_params['freq_keep']

os.environ["OMP_NUM_THREADS"] = "16"


def run_lpc(signal, lpcs, frame_length=160):
    num_frames, lpc_order = lpcs.shape

    prediction = np.concatenate(
        [- np.convolve(signal[i * frame_length : (i + 1) * frame_length + lpc_order - 1], lpcs[i], mode='valid') for i in range(num_frames)]
    )
    error = signal[lpc_order :] - prediction

    return prediction, error


if __name__ == "__main__":
    args = parser.parse_args()

    features = np.memmap(args.features, dtype=np.float32,mode = 'r').reshape((-1, 36))
    data     = np.memmap(args.data, dtype=np.int16,mode = 'r').reshape((-1, 2))

    num_frames = features.shape[0]
    feature_dim = features.shape[1]

    assert feature_dim == 36

    output  = np.memmap(args.output, dtype=np.float32, shape=(num_frames, feature_dim), mode='w+')
    output[:, :36] = features

    # lpc coefficients and signal
    lpcs = features[:, 20:36]
    sig = data[:, 1]

    # parameters

    # constants
    pitch_min = 32
    pitch_max = 256
    lpc_order = 16
    fs = 16000
    frame_length = 160
    overlap_frames = 100
    chunk_size = 10000
    history_length = frame_length * overlap_frames
    history = np.zeros(history_length, dtype=np.int16)
    pitch_position=18
    xcorr_position=19
    conf_position=36

    num_frames = len(sig) // 160 - 1

    frame_start = 0
    frame_stop = min(frame_start + chunk_size, num_frames)
    signal_start = 0
    signal_stop = frame_stop * frame_length

    niters = (num_frames - 1)//chunk_size
    for i in tqdm.trange(niters):
        if (frame_start > num_frames - 1):
            break
        chunk = np.concatenate((history, sig[signal_start:signal_stop]))
        chunk_la = np.concatenate((history, sig[signal_start:signal_stop + 80]))

        # Feature computation
        spec = stft(x = np.concatenate([np.zeros(80),chunk_la/(2**15 - 1)]), w = 'boxcar', N = N, H = H).T
        phase_diff = spec*np.conj(np.roll(spec,1,axis = -1))
        phase_diff = phase_diff/(np.abs(phase_diff) + 1.0e-8)
        idx_save = np.concatenate([np.arange(freq_keep),(N//2 + 1) + np.arange(freq_keep),2*(N//2 + 1) + np.arange(freq_keep)])
        feature = np.concatenate([np.log(np.abs(spec) + 1.0e-8),np.real(phase_diff),np.imag(phase_diff)],axis = 0).T
        feature_if = feature[:,idx_save]

        data_temp = np.memmap('./temp_featcompute_' + dict_params['data_format'] + '_.raw', dtype=np.int16, shape=(chunk.shape[0]), mode='w+')
        data_temp[:chunk.shape[0]] = chunk_la[80:].astype(np.int16)

        subprocess.run([args.path_lpcnet_extractor, './temp_featcompute_' + dict_params['data_format'] + '_.raw', './temp_featcompute_xcorr_' + dict_params['data_format'] + '_.raw'])
        feature_xcorr = np.flip(np.fromfile('./temp_featcompute_xcorr_' + dict_params['data_format'] + '_.raw', dtype='float32').reshape((-1,256),order = 'C'),axis = 1)
        ones_zero_lag = np.expand_dims(np.ones(feature_xcorr.shape[0]),-1)
        feature_xcorr = np.concatenate([ones_zero_lag,feature_xcorr],axis = -1)

        os.remove('./temp_featcompute_' + dict_params['data_format'] + '_.raw')
        os.remove('./temp_featcompute_xcorr_' + dict_params['data_format'] + '_.raw')

        if dict_params['data_format'] == 'if':
            feature = feature_if
        elif dict_params['data_format'] == 'xcorr':
            feature = feature_xcorr
        else:
            indmin = min(feature_if.shape[0],feature_xcorr.shape[0])
            feature = np.concatenate([feature_xcorr[:indmin,:],feature_if[:indmin,:]],-1)

        # Compute pitch with my model
        model_cents = pitch_nn(torch.from_numpy(np.copy(np.expand_dims(feature,0))).float().to(device))
        model_cents = 20*model_cents.argmax(dim=1).cpu().detach().squeeze().numpy()
        frequency = 62.5*2**(model_cents/1200)

        frequency  = frequency[overlap_frames : overlap_frames + frame_stop - frame_start]

        # convert frequencies to periods
        periods    = np.round(fs / frequency)

        periods = np.clip(periods, pitch_min, pitch_max)

        output[frame_start:frame_stop, pitch_position] = (periods - 100) / 50

        frame_offset = (pitch_max + frame_length - 1) // frame_length
        offset = frame_offset * frame_length
        padding = lpc_order


        if frame_start < frame_offset:
            lpc_coeffs = np.concatenate((np.zeros((frame_offset - frame_start, lpc_order), dtype=np.float32), lpcs[:frame_stop]))
        else:
            lpc_coeffs = lpcs[frame_start - frame_offset : frame_stop]

        pred, error = run_lpc(chunk[history_length - offset - padding :], lpc_coeffs, frame_length=frame_length)

        xcorr = np.zeros(frame_stop - frame_start)
        for i, p in enumerate(periods.astype(np.int16)):
            if p > 0:
                f1 = error[offset + i * frame_length : offset + (i + 1) * frame_length]
                f2 = error[offset + i * frame_length - p : offset + (i + 1) * frame_length - p]
                xcorr[i] = np.dot(f1, f2) / np.sqrt(np.dot(f1, f1) * np.dot(f2, f2) + 1e-6)

        output[frame_start:frame_stop, xcorr_position] = xcorr - 0.5

        # update buffers and indices
        history = chunk[-history_length :]

        frame_start += chunk_size
        frame_stop += chunk_size
        frame_stop = min(frame_stop, num_frames)

        signal_start = frame_start * frame_length
        signal_stop  = frame_stop  * frame_length

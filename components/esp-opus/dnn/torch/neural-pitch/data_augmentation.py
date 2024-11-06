"""
Perform Data Augmentation (Gain, Additive Noise, Random Filtering) on Input TTS Data
1. Read in chunks and compute clean pitch first
2. Then add in augmentation (Noise/Level/Response)
    - Adds filtered noise from the "Demand" dataset, https://zenodo.org/record/1227121#.XRKKxYhKiUk
    - When using the Demand Dataset, consider each channel as a possible noise input, and keep the first 4 minutes of noise for training
3. Use this "augmented" audio for feature computation, and compute pitch using CREPE on the clean input

Notes: To ensure consistency with the discovered CREPE offset, we do the following
- We pad the input audio to the zero-centered CREPE estimator with 80 zeros
- We pad the input audio to our feature computation with 160 zeros to center them
"""

import argparse
parser = argparse.ArgumentParser()

parser.add_argument('data', type=str, help='input raw audio data')
parser.add_argument('output', type=str, help='output directory')
parser.add_argument('path_lpcnet_extractor', type=str, help='path to LPCNet extractor object file (generated on compilation)')
parser.add_argument('noise_dataset', type=str, help='Location of the Demand Datset')
parser.add_argument('--flag_xcorr', type=bool, help='Flag to additionally dump xcorr features',choices=[True,False],default = False,required = False)
parser.add_argument('--fraction_input_use', type=float, help='Fraction of input data to consider',default = 0.3,required = False)
parser.add_argument('--gpu_index', type=int, help='GPU index to use if multiple GPUs',default = 0,required = False)
parser.add_argument('--choice_augment', type=str, help='Choice of noise augmentation, either use additive synthetic noise or add noise from the demand dataset',choices = ['demand','synthetic'],default = "demand",required = False)
parser.add_argument('--fraction_clean', type=float, help='Fraction of data to keep clean (that is not augment with anything)',default = 0.2,required = False)
parser.add_argument('--chunk_size', type=int, help='Number of samples to augment with for each iteration',default = 80000,required = False)
parser.add_argument('--N', type=int, help='STFT window size',default = 320,required = False)
parser.add_argument('--H', type=int, help='STFT Hop size',default = 160,required = False)
parser.add_argument('--freq_keep', type=int, help='Number of Frequencies to keep',default = 30,required = False)

args = parser.parse_args()

import os
os.environ["CUDA_VISIBLE_DEVICES"] = str(args.gpu_index)

from utils import stft, random_filter

import numpy as np
import tqdm
import crepe
import random
import glob
import subprocess

data_full = np.memmap(args.data, dtype=np.int16,mode = 'r')
data = data_full[:(int)(args.fraction_input_use*data_full.shape[0])]

# list_features = []
list_cents = []
list_confidences = []

N = args.N
H = args.H
freq_keep = args.freq_keep
# Minimum/Maximum periods, decided by LPCNet
min_period = 32
max_period = 256
f_ref = 16000/max_period
chunk_size = args.chunk_size
num_frames_chunk = chunk_size//H
list_indices_keep = np.concatenate([np.arange(freq_keep), (N//2 + 1) + np.arange(freq_keep), 2*(N//2 + 1) + np.arange(freq_keep)])

output_IF  = np.memmap(args.output + '_iffeat.f32', dtype=np.float32, shape=(((data.shape[0]//chunk_size - 1)//1)*num_frames_chunk,list_indices_keep.shape[0]), mode='w+')
if args.flag_xcorr:
    output_xcorr  = np.memmap(args.output + '_xcorr.f32', dtype=np.float32, shape=(((data.shape[0]//chunk_size - 1)//1)*num_frames_chunk,257), mode='w+')

fraction_clean = args.fraction_clean

noise_dataset = args.noise_dataset

for i in tqdm.trange((data.shape[0]//chunk_size - 1)//1):
    chunk = data[i*chunk_size:(i + 1)*chunk_size]/(2**15 - 1)

    # Clean Pitch/Confidence Estimate
    # Padding input to CREPE by 80 samples to ensure it aligns
    _, pitch, confidence, _ = crepe.predict(np.concatenate([np.zeros(80),chunk]), 16000, center=True, viterbi=True,verbose=0)
    cent = 1200*np.log2(np.divide(pitch, f_ref, out=np.zeros_like(pitch), where=pitch!=0) + 1.0e-8)

    # Filter out of range pitches/confidences
    confidence[pitch < 16000/max_period] = 0
    confidence[pitch > 16000/min_period] = 0

    # Keep fraction of data clean, augment only 1 minus the fraction
    if (np.random.rand() > fraction_clean):
        # Response, generate controlled/random 2nd order IIR filter and filter chunk
        chunk = random_filter(chunk)

        # Level/Gain response {scale by random gain between 1.0e-3 and 10}
        # Generate random gain in dB and then convert to scale
        g_dB = np.random.uniform(low =  -60, high = 20, size = 1)
        # g_dB = 0
        g = 10**(g_dB/20)

        # Noise Addition {Add random SNR 2nd order randomly colored noise}
        # Generate noise SNR value and add corresponding noise
        snr_dB = np.random.uniform(low =  -20, high = 30, size = 1)

        if args.choice_augment == 'synthetic':
            n = np.random.randn(chunk_size)
        else:
            list_noisefiles = noise_dataset + '*.wav'
            noise_file = random.choice(glob.glob(list_noisefiles))
            n = np.memmap(noise_file, dtype=np.int16,mode = 'r')/(2**15 - 1)
            rand_range = np.random.randint(low = 0, high = (n.shape[0] - 16000*60 - chunk.shape[0])) # 16000 is subtracted because we will use the last 1 minutes of noise for testing
            n = n[rand_range:rand_range + chunk.shape[0]]

        # Randomly filter the sampled noise as well
        n = random_filter(n)
        # generate random prime number between 0,500 and make those samples of noise 0 (to prevent GRU from picking up temporal patterns)
        Nprime = random.choice([2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503, 509, 521, 523, 541])
        n[chunk_size - Nprime:] = np.zeros(Nprime)
        snr_multiplier = np.sqrt((np.sum(np.abs(chunk)**2)/np.sum(np.abs(n)**2))*10**(-snr_dB/10))

        chunk = g*(chunk + snr_multiplier*n)

    # Zero pad input audio by 160 to center the frames
    spec = stft(x = np.concatenate([np.zeros(160),chunk]), w = 'boxcar', N = N, H = H).T
    phase_diff = spec*np.conj(np.roll(spec,1,axis = -1))
    phase_diff = phase_diff/(np.abs(phase_diff) + 1.0e-8)
    feature = np.concatenate([np.log(np.abs(spec) + 1.0e-8),np.real(phase_diff),np.imag(phase_diff)],axis = 0).T
    feature = feature[:,list_indices_keep]

    if args.flag_xcorr:
        # Dump noisy audio into temp file
        data_temp = np.memmap('./temp_augment.raw', dtype=np.int16, shape=(chunk.shape[0]), mode='w+')
        # data_temp[:chunk.shape[0]] = (chunk/(np.max(np.abs(chunk)))*(2**15 - 1)).astype(np.int16)
        data_temp[:chunk.shape[0]] = ((chunk)*(2**15 - 1)).astype(np.int16)

        subprocess.run([args.path_lpcnet_extractor, './temp_augment.raw', './temp_augment_xcorr.f32'])
        feature_xcorr = np.flip(np.fromfile('./temp_augment_xcorr.f32', dtype='float32').reshape((-1,256),order = 'C'),axis = 1)
        ones_zero_lag = np.expand_dims(np.ones(feature_xcorr.shape[0]),-1)
        feature_xcorr = np.concatenate([ones_zero_lag,feature_xcorr],axis = -1)

        os.remove('./temp_augment.raw')
        os.remove('./temp_augment_xcorr.f32')
    num_frames = min(cent.shape[0],feature.shape[0],feature_xcorr.shape[0],num_frames_chunk)
    feature = feature[:num_frames,:]
    cent = cent[:num_frames]
    confidence = confidence[:num_frames]
    feature_xcorr = feature_xcorr[:num_frames]
    output_IF[i*num_frames_chunk:(i + 1)*num_frames_chunk,:] = feature
    output_xcorr[i*num_frames_chunk:(i + 1)*num_frames_chunk,:] = feature_xcorr
    list_cents.append(cent)
    list_confidences.append(confidence)

list_cents = np.hstack(list_cents)
list_confidences = np.hstack(list_confidences)

np.save(args.output + '_pitches',np.vstack([list_cents,list_confidences]))

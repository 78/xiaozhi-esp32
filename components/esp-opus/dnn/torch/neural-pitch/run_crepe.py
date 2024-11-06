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
parser.add_argument('--gpu-index', type=int, help='GPU index to use if multiple GPUs',default = 0,required = False)
parser.add_argument('--chunk-size-frames', type=int, help='Number of frames to process at a time',default = 100000,required = False)

args = parser.parse_args()

import os
os.environ["CUDA_VISIBLE_DEVICES"] = str(args.gpu_index)

import numpy as np
import tqdm
import crepe

data = np.memmap(args.data, dtype=np.int16,mode = 'r')

# list_features = []
list_cents = []
list_confidences = []

min_period = 32
max_period = 256
f_ref = 16000/max_period
chunk_size_frames = args.chunk_size_frames
chunk_size = chunk_size_frames*160

nb_chunks = (data.shape[0]+79)//chunk_size+1

output_data = np.zeros((0,2),dtype='float32')

for i in tqdm.trange(nb_chunks):
    if i==0:
        chunk = np.concatenate([np.zeros(80),data[:chunk_size-80]])
    elif i==nb_chunks-1:
        chunk = data[i*chunk_size-80:]
    else:
        chunk = data[i*chunk_size-80:(i+1)*chunk_size-80]
    chunk = chunk/np.array(32767.,dtype='float32')

    # Clean Pitch/Confidence Estimate
    # Padding input to CREPE by 80 samples to ensure it aligns
    _, pitch, confidence, _ = crepe.predict(chunk, 16000, center=True, viterbi=True,verbose=0)
    pitch = pitch[:chunk_size_frames]
    confidence = confidence[:chunk_size_frames]


    # Filter out of range pitches/confidences
    confidence[pitch < 16000/max_period] = 0
    confidence[pitch > 16000/min_period] = 0
    pitch = np.reshape(pitch, (-1, 1))
    confidence = np.reshape(confidence, (-1, 1))
    out = np.concatenate([pitch, confidence], axis=-1, dtype='float32')
    output_data = np.concatenate([output_data, out], axis=0)


output_data.tofile(args.output)

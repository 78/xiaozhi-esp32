# convert audio files to protocol v3 stream
import librosa
import opuslib
import struct
import sys
import tqdm
import numpy as np

def encode_audio_to_opus(input_file, output_file):
    # Load audio file using librosa
    audio, sample_rate = librosa.load(input_file, sr=None, mono=False, dtype=np.int16)
    
    # Get left channel if stereo
    if audio.ndim == 2:
        audio = audio[0]
    
    # Initialize Opus encoder
    encoder = opuslib.Encoder(sample_rate, 1, opuslib.APPLICATION_VOIP)

    # Encode audio data to Opus packets
    # Save encoded data to file
    with open(output_file, 'wb') as f:
        sample_rate = 16000 # 16000Hz
        duration = 60 # 60ms every frame
        frame_size = int(sample_rate * duration / 1000)
        for i in tqdm.tqdm(range(0, len(audio) - frame_size, frame_size)):
            frame = audio[i:i + frame_size]
            opus_data = encoder.encode(frame.tobytes(), frame_size=frame_size)
            # protocol format, [1u type, 1u reserved, 2u len, data]
            packet = struct.pack('>BBH', 0, 0, len(opus_data)) + opus_data
            f.write(packet)

# Example usage
if len(sys.argv) != 3:
    print('Usage: python convert.py <input_file> <output_file>')
    sys.exit(1)

input_file = sys.argv[1]
output_file = sys.argv[2]
encode_audio_to_opus(input_file, output_file)

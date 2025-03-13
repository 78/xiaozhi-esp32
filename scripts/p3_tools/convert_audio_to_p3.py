# convert audio files to protocol v3 stream
import librosa
import opuslib
import struct
import sys
import tqdm
import numpy as np
import argparse
import pyloudnorm as pyln

def encode_audio_to_opus(input_file, output_file, target_lufs=None):
    # Load audio file using librosa
    audio, sample_rate = librosa.load(input_file, sr=None, mono=False, dtype=np.float32)
    
    # Convert to mono if stereo
    if audio.ndim == 2:
        audio = librosa.to_mono(audio)
    
    if target_lufs is not None:
        print("Note: Automatic loudness adjustment is enabled, which may cause", file=sys.stderr) 
        print("      audio distortion. If the input audio has already been ", file=sys.stderr)
        print("      loudness-adjusted or if the input audio is TTS audio, ", file=sys.stderr)
        print("      please use the `-d` parameter to disable loudness adjustment.", file=sys.stderr)
        meter = pyln.Meter(sample_rate)
        current_loudness = meter.integrated_loudness(audio)
        audio = pyln.normalize.loudness(audio, current_loudness, target_lufs)
        print(f"Adjusted loudness: {current_loudness:.1f} LUFS -> {target_lufs} LUFS")

    # Convert sample rate to 16000Hz if necessary
    target_sample_rate = 16000
    if sample_rate != target_sample_rate:
        audio = librosa.resample(audio, orig_sr=sample_rate, target_sr=target_sample_rate)
        sample_rate = target_sample_rate
    
    # Convert audio data back to int16 after processing
    audio = (audio * 32767).astype(np.int16)
    
    # Initialize Opus encoder
    encoder = opuslib.Encoder(sample_rate, 1, opuslib.APPLICATION_AUDIO)

    # Encode and save
    with open(output_file, 'wb') as f:
        duration = 60  # 60ms per frame
        frame_size = int(sample_rate * duration / 1000)
        for i in tqdm.tqdm(range(0, len(audio) - frame_size, frame_size)):
            frame = audio[i:i + frame_size]
            opus_data = encoder.encode(frame.tobytes(), frame_size=frame_size)
            packet = struct.pack('>BBH', 0, 0, len(opus_data)) + opus_data
            f.write(packet)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert audio to Opus with loudness normalization')
    parser.add_argument('input_file', help='Input audio file')
    parser.add_argument('output_file', help='Output .opus file')
    parser.add_argument('-l', '--lufs', type=float, default=-16.0,
                       help='Target loudness in LUFS (default: -16)')
    parser.add_argument('-d', '--disable-loudnorm', action='store_true',
                       help='Disable loudness normalization')
    args = parser.parse_args()

    target_lufs = None if args.disable_loudnorm else args.lufs
    encode_audio_to_opus(args.input_file, args.output_file, target_lufs)
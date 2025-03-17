import struct
import sys
import opuslib
import numpy as np
from tqdm import tqdm
import soundfile as sf


def decode_p3_to_audio(input_file, output_file):
    sample_rate = 16000
    channels = 1
    decoder = opuslib.Decoder(sample_rate, channels)

    pcm_frames = []
    frame_size = int(sample_rate * 60 / 1000)

    with open(input_file, "rb") as f:
        f.seek(0, 2)
        total_size = f.tell()
        f.seek(0)

        with tqdm(total=total_size, unit="B", unit_scale=True) as pbar:
            while True:
                header = f.read(4)
                if not header or len(header) < 4:
                    break

                pkt_type, reserved, opus_len = struct.unpack(">BBH", header)
                opus_data = f.read(opus_len)
                if len(opus_data) != opus_len:
                    break

                pcm = decoder.decode(opus_data, frame_size)
                pcm_frames.append(np.frombuffer(pcm, dtype=np.int16))

                pbar.update(4 + opus_len)

    if not pcm_frames:
        raise ValueError("No valid audio data found")

    pcm_data = np.concatenate(pcm_frames)

    sf.write(output_file, pcm_data, sample_rate, subtype="PCM_16")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_p3_to_audio.py <input.p3> <output.wav>")
        sys.exit(1)

    decode_p3_to_audio(sys.argv[1], sys.argv[2])

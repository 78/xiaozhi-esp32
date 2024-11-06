import os
import argparse

import numpy as np
from scipy import signal
from scipy.io import wavfile
import resampy




parser = argparse.ArgumentParser()

parser.add_argument("filelist", type=str, help="file with filenames for concatenation in WAVE format")
parser.add_argument("target_fs", type=int, help="target sampling rate of concatenated file")
parser.add_argument("output", type=str, help="binary output file (integer16)")
parser.add_argument("--basedir", type=str, help="basedir for filenames in filelist, defaults to ./", default="./")
parser.add_argument("--normalize", action="store_true", help="apply normalization")
parser.add_argument("--db_max", type=float, help="max DB for random normalization", default=0)
parser.add_argument("--db_min", type=float, help="min DB for random normalization", default=0)
parser.add_argument("--verbose", action="store_true")

def read_filelist(basedir, filelist):
    with open(filelist, "r") as f:
        files = f.readlines()

    fullfiles = [os.path.join(basedir, f.rstrip('\n')) for f in files if len(f.rstrip('\n')) > 0]

    return fullfiles

def read_wave(file, target_fs):
    fs, x = wavfile.read(file)

    if fs < target_fs:
        return None
        print(f"[read_wave] warning: file {file} will be up-sampled from {fs} to {target_fs} Hz")

    if fs != target_fs:
        x = resampy.resample(x, fs, target_fs)

    return x.astype(np.float32)

def random_normalize(x, db_min, db_max, max_val=2**15 - 1):
    db = np.random.uniform(db_min, db_max, 1)
    m = np.abs(x).max()
    c = 10**(db/20) * max_val / m

    return c * x


def concatenate(filelist : str, output : str, target_fs: int, normalize=True, db_min=0, db_max=0, verbose=False):

    overlap_size = int(40 * target_fs / 8000)
    overlap_mem = np.zeros(overlap_size, dtype=np.float32)
    overlap_win1 = (0.5 + 0.5 * np.cos(np.arange(0, overlap_size) * np.pi / overlap_size)).astype(np.float32)
    overlap_win2 = np.flipud(overlap_win1)

    with open(output, 'wb') as f:
        for file in filelist:
            x = read_wave(file, target_fs)
            if x is None: continue

            if len(x) < 10 * overlap_size:
                if verbose: print(f"skipping {file}...")
                continue
            elif verbose:
                print(f"processing {file}...")

            if normalize:
                x = random_normalize(x, db_min, db_max)

            x1 = x[:-overlap_size]
            x1[:overlap_size] = overlap_win1 * overlap_mem + overlap_win2 * x1[:overlap_size]

            f.write(x1.astype(np.int16).tobytes())

            overlap_mem = x1[-overlap_size]


if __name__ == "__main__":
    args = parser.parse_args()

    filelist = read_filelist(args.basedir, args.filelist)

    concatenate(filelist, args.output, args.target_fs, normalize=args.normalize, db_min=args.db_min, db_max=args.db_max, verbose=args.verbose)

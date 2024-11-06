import os
import argparse
import yaml
import subprocess

import numpy as np

from moc2 import compare as moc

DEBUG=False

parser = argparse.ArgumentParser()

parser.add_argument('inputdir', type=str, help='Input folder with test items')
parser.add_argument('outputdir', type=str, help='Output folder')
parser.add_argument('bitrate', type=int, help='bitrate to test')
parser.add_argument('--reference_opus_demo', type=str, default='./opus_demo', help='reference opus_demo binary for generating bitstreams and reference output')
parser.add_argument('--encoder_options', type=str, default="", help='encoder options (e.g. -complexity 5)')
parser.add_argument('--test_opus_demo', type=str, default='./opus_demo', help='opus_demo binary under test')
parser.add_argument('--test_opus_demo_options', type=str, default='-dec_complexity 7', help='options for test opus_demo (e.g. "-dec_complexity 7")')
parser.add_argument('--verbose', type=int, default=0, help='verbosity level: 0 for quiet (default), 1 for reporting individual test results, 2 for reporting per-item scores in failed tests')

def run_opus_encoder(opus_demo_path, input_pcm_path, bitstream_path, application, fs, num_channels, bitrate, options=[], verbose=False):

    call_args = [
        opus_demo_path,
        "-e",
        application,
        str(fs),
        str(num_channels),
        str(bitrate),
        "-bandwidth",
        "WB"
    ]

    call_args += options

    call_args += [
        input_pcm_path,
        bitstream_path
    ]

    try:
        if verbose:
            print(f"running {call_args}...")
            subprocess.run(call_args)
        else:
            subprocess.run(call_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except:
        return 1

    return 0


def run_opus_decoder(opus_demo_path, bitstream_path, output_pcm_path, fs, num_channels, options=[], verbose=False):

    call_args = [
        opus_demo_path,
        "-d",
        str(fs),
        str(num_channels)
    ]

    call_args += options

    call_args += [
        bitstream_path,
        output_pcm_path
    ]

    try:
        if verbose:
            print(f"running {call_args}...")
            subprocess.run(call_args)
        else:
            subprocess.run(call_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except:
        return 1

    return 0

def compute_moc_score(reference_pcm, test_pcm, delay=91):
    x_ref = np.fromfile(reference_pcm, dtype=np.int16).astype(np.float32) / (2 ** 15)
    x_cut = np.fromfile(test_pcm, dtype=np.int16).astype(np.float32) / (2 ** 15)

    moc_score = moc(x_ref, x_cut[delay:])

    return moc_score

def sox(*call_args):
    try:
        call_args = ["sox"] + list(call_args)
        subprocess.run(call_args)
        return 0
    except:
        return 1

def process_clip_factory(ref_opus_demo, test_opus_demo, enc_options, test_options):
    def process_clip(clip_path, processdir, bitrate):
        # derive paths
        clipname = os.path.splitext(os.path.split(clip_path)[1])[0]
        pcm_path = os.path.join(processdir, clipname + ".raw")
        bitstream_path = os.path.join(processdir, clipname + ".bin")
        ref_path = os.path.join(processdir, clipname + "_ref.raw")
        test_path = os.path.join(processdir, clipname + "_test.raw")

        # run sox
        sox(clip_path, pcm_path)

        # run encoder
        run_opus_encoder(ref_opus_demo, pcm_path, bitstream_path, "voip", 16000, 1, bitrate, enc_options)

        # run decoder
        run_opus_decoder(ref_opus_demo, bitstream_path, ref_path, 16000, 1)
        run_opus_decoder(test_opus_demo, bitstream_path, test_path, 16000, 1, options=test_options)

        d_ref  = compute_moc_score(pcm_path, ref_path)
        d_test = compute_moc_score(pcm_path, test_path)

        return d_ref, d_test


    return process_clip

def main(inputdir, outputdir, bitrate, reference_opus_demo, test_opus_demo, enc_option_string, test_option_string, verbose):

    # load clips list
    with open(os.path.join(inputdir, 'clips.yml'), "r") as f:
        clips = yaml.safe_load(f)

    # parse test options
    enc_options = enc_option_string.split()
    test_options = test_option_string.split()

    process_clip = process_clip_factory(reference_opus_demo, test_opus_demo, enc_options, test_options)

    os.makedirs(outputdir, exist_ok=True)
    processdir = os.path.join(outputdir, 'process')
    os.makedirs(processdir, exist_ok=True)

    num_passed = 0
    results = dict()
    min_rel_diff = 1000
    min_mean = 1000
    worst_clip = None
    worst_lang = None
    for lang, lang_clips in clips.items():
        if verbose > 0: print(f"processing language {lang}...")
        results[lang] = np.zeros((len(lang_clips), 2))
        for i, clip in enumerate(lang_clips):
            clip_path = os.path.join(inputdir, clip)
            d_ref, d_test = process_clip(clip_path, processdir, bitrate)
            results[lang][i, 0] = d_ref
            results[lang][i, 1] = d_test

        alpha = 0.5
        rel_diff = ((results[lang][:, 0] ** alpha - results[lang][:, 1] ** alpha) /(results[lang][:, 0] ** alpha))

        min_idx = np.argmin(rel_diff).item()
        if rel_diff[min_idx] < min_rel_diff:
            min_rel_diff = rel_diff[min_idx]
            worst_clip = lang_clips[min_idx]

        if np.mean(rel_diff) < min_mean:
            min_mean = np.mean(rel_diff).item()
            worst_lang = lang

        if np.min(rel_diff) < -0.1 or np.mean(rel_diff) < -0.025:
            if verbose > 0: print(f"FAIL ({np.mean(results[lang], axis=0)} {np.mean(rel_diff)} {np.min(rel_diff)})")
            if verbose > 1:
                for i, c in enumerate(lang_clips):
                    print(f"    {c:50s} {results[lang][i]} {rel_diff[i]}")
        else:
            if verbose > 0: print(f"PASS ({np.mean(results[lang], axis=0)} {np.mean(rel_diff)} {np.min(rel_diff)})")
            num_passed += 1

    print(f"{num_passed}/{len(clips)} tests passed!")

    print(f"worst case occured at clip {worst_clip} with relative difference of {min_rel_diff}")
    print(f"worst mean relative difference was {min_mean} for test {worst_lang}")

    np.save(os.path.join(outputdir, f'results_' + "_".join(test_options) + f"_{bitrate}.npy"), results, allow_pickle=True)



if __name__ == "__main__":
    args = parser.parse_args()

    main(args.inputdir,
         args.outputdir,
         args.bitrate,
         args.reference_opus_demo,
         args.test_opus_demo,
         args.encoder_options,
         args.test_opus_demo_options,
         args.verbose)

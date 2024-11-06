import argparse
import os
import yaml
import subprocess

import numpy as np



parser = argparse.ArgumentParser()
parser.add_argument('commonvoice_base_dir')
parser.add_argument('output_dir')
parser.add_argument('--clips-per-language', required=False, type=int, default=10)
parser.add_argument('--seed', required=False, type=int, default=2024)


def select_clips(dir, num_clips=10):

    if num_clips % 2:
        print(f"warning: number of clips will be reduced to {num_clips - 1}")
    female = dict()
    male = dict()

    clips = np.genfromtxt(os.path.join(dir, 'validated.tsv'), delimiter='\t', dtype=str, invalid_raise=False)
    clips_by_client = dict()

    if len(clips.shape) < 2 or len(clips) < num_clips:
        # not enough data to proceed
        return None

    for client in set(clips[1:,0]):
        client_clips = clips[clips[:, 0] == client]
        f, m = False, False
        if 'female_feminine' in client_clips[:, 8]:
            female[client] = client_clips[client_clips[:, 8] == 'female_feminine']
            f = True
        if 'male_masculine' in client_clips[:, 8]:
            male[client] = client_clips[client_clips[:, 8] == 'male_masculine']
            m = True

        if f and m:
            print(f"both male and female clips under client {client}")


    if min(len(female), len(male)) < num_clips // 2:
        return None

    # select num_clips // 2 random female clients
    female_client_selection = np.array(list(female.keys()), dtype=str)[np.random.choice(len(female), num_clips//2, replace=False)]
    female_clip_selection = []
    for c in female_client_selection:
        s_idx = np.random.randint(0, len(female[c]))
        female_clip_selection.append(os.path.join(dir, 'clips', female[c][s_idx, 1].item()))

    # select num_clips // 2 random female clients
    male_client_selection = np.array(list(male.keys()), dtype=str)[np.random.choice(len(male), num_clips//2, replace=False)]
    male_clip_selection = []
    for c in male_client_selection:
        s_idx = np.random.randint(0, len(male[c]))
        male_clip_selection.append(os.path.join(dir, 'clips', male[c][s_idx, 1].item()))

    return female_clip_selection + male_clip_selection

def ffmpeg_available():
    try:
        x = subprocess.run(['ffmpeg', '-h'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return x.returncode == 0
    except:
        return False


def convert_clips(selection, outdir):
    if not ffmpeg_available():
        raise RuntimeError("ffmpeg not available")

    clipdir = os.path.join(outdir, 'clips')
    os.makedirs(clipdir, exist_ok=True)

    clipdict = dict()

    for lang, clips in selection.items():
        clipdict[lang] = []
        for clip in clips:
            clipname = os.path.splitext(os.path.split(clip)[-1])[0]
            target_name = os.path.join('clips', clipname + '.wav')
            call_args = ['ffmpeg', '-i', clip, '-ar', '16000', os.path.join(outdir, target_name)]
            print(call_args)
            r = subprocess.run(call_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if r.returncode != 0:
                raise RuntimeError(f'could not execute {call_args}')
            clipdict[lang].append(target_name)

    return clipdict


if __name__ == "__main__":
    if not ffmpeg_available():
        raise RuntimeError("ffmpeg not available")

    args = parser.parse_args()

    base_dir = args.commonvoice_base_dir
    output_dir = args.output_dir
    seed = args.seed

    np.random.seed(seed)

    langs = os.listdir(base_dir)
    selection = dict()

    for lang in langs:
        print(f"processing {lang}...")
        clips = select_clips(os.path.join(base_dir, lang))
        if clips is not None:
            selection[lang] = clips


    os.makedirs(output_dir, exist_ok=True)

    clips = convert_clips(selection, output_dir)

    with open(os.path.join(output_dir, 'clips.yml'), 'w') as f:
        yaml.dump(clips, f)

import os

import torch
import numpy as np

def load_lpcnet_features(feature_file, version=2):
    if version == 2:
        layout = {
            'cepstrum': [0,18],
            'periods': [18, 19],
            'pitch_corr': [19, 20],
            'lpc': [20, 36]
            }
        frame_length = 36

    elif version == 1:
        layout = {
            'cepstrum': [0,18],
            'periods': [36, 37],
            'pitch_corr': [37, 38],
            'lpc': [39, 55],
            }
        frame_length = 55
    else:
        raise ValueError(f'unknown feature version: {version}')


    raw_features = torch.from_numpy(np.fromfile(feature_file, dtype='float32'))
    raw_features = raw_features.reshape((-1, frame_length))

    features = torch.cat(
        [
            raw_features[:, layout['cepstrum'][0]   : layout['cepstrum'][1]],
            raw_features[:, layout['pitch_corr'][0] : layout['pitch_corr'][1]]
        ],
        dim=1
    )

    lpcs = raw_features[:, layout['lpc'][0]   : layout['lpc'][1]]
    periods = (0.1 + 50 * raw_features[:, layout['periods'][0] : layout['periods'][1]] + 100).long()

    return {'features' : features, 'periods' : periods, 'lpcs' : lpcs}



def create_new_data(signal_path, reference_data_path, new_data_path, offset=320, preemph_factor=0.85):
    ref_data = np.memmap(reference_data_path, dtype=np.int16)
    signal = np.memmap(signal_path, dtype=np.int16)

    signal_preemph_path = os.path.splitext(signal_path)[0] + '_preemph.raw'
    signal_preemph = np.memmap(signal_preemph_path, dtype=np.int16, mode='write', shape=signal.shape)


    assert len(signal) % 160 == 0
    num_frames = len(signal) // 160
    mem = np.zeros(1)
    for fr in range(len(signal)//160):
        signal_preemph[fr * 160 : (fr + 1) * 160] = np.convolve(np.concatenate((mem, signal[fr * 160 : (fr + 1) * 160])), [1, -preemph_factor], mode='valid')
        mem = signal[(fr + 1) * 160 - 1 : (fr + 1) * 160]

    new_data = np.memmap(new_data_path, dtype=np.int16, mode='write', shape=ref_data.shape)

    new_data[:] = 0
    N = len(signal) - offset
    new_data[1 : 2*N + 1: 2] = signal_preemph[offset:]
    new_data[2 : 2*N + 2: 2] = signal_preemph[offset:]


def parse_warpq_scores(output_file):
    """ extracts warpq scores from output file """

    with open(output_file, "r") as f:
        lines = f.readlines()

    scores = [float(line.split("WARP-Q score:")[-1]) for line in lines if line.startswith("WARP-Q score:")]

    return scores


def parse_stats_file(file):

    with open(file, "r") as f:
        lines = f.readlines()

    mean     = float(lines[0].split(":")[-1])
    bt_mean  = float(lines[1].split(":")[-1])
    top_mean = float(lines[2].split(":")[-1])

    return mean, bt_mean, top_mean

def collect_test_stats(test_folder):
    """ collects statistics for all discovered metrics from test folder """

    metrics = {'pesq', 'warpq', 'pitch_error', 'voicing_error'}

    results = dict()

    content = os.listdir(test_folder)

    stats_files = [file for file in content if file.startswith('stats_')]

    for file in stats_files:
        metric = file[len("stats_") : -len(".txt")]

        if metric not in metrics:
            print(f"warning: unknown metric {metric}")

        mean, bt_mean, top_mean = parse_stats_file(os.path.join(test_folder, file))

        results[metric] = [mean, bt_mean, top_mean]

    return results

"""
/* Copyright (c) 2023 Amazon
   Written by Jan Buethe */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
"""

import os
import argparse

import numpy as np
import matplotlib.pyplot as plt
from prettytable import PrettyTable
from matplotlib.patches import Patch

parser = argparse.ArgumentParser()
parser.add_argument('folder', type=str, help='path to folder with pre-calculated metrics')
parser.add_argument('--metric', choices=['pesq', 'moc', 'warpq', 'nomad', 'laceloss', 'all'], default='all', help='default: all')
parser.add_argument('--output', type=str, default=None, help='alternative output folder, default: folder')

def load_data(folder):
    data = dict()

    if os.path.isfile(os.path.join(folder, 'results_moc.npy')):
        data['moc'] = np.load(os.path.join(folder, 'results_moc.npy'), allow_pickle=True).item()

    if os.path.isfile(os.path.join(folder, 'results_moc2.npy')):
        data['moc2'] = np.load(os.path.join(folder, 'results_moc2.npy'), allow_pickle=True).item()

    if os.path.isfile(os.path.join(folder, 'results_pesq.npy')):
        data['pesq'] = np.load(os.path.join(folder, 'results_pesq.npy'), allow_pickle=True).item()

    if os.path.isfile(os.path.join(folder, 'results_warpq.npy')):
        data['warpq'] = np.load(os.path.join(folder, 'results_warpq.npy'), allow_pickle=True).item()

    if os.path.isfile(os.path.join(folder, 'results_nomad.npy')):
        data['nomad'] = np.load(os.path.join(folder, 'results_nomad.npy'), allow_pickle=True).item()

    if os.path.isfile(os.path.join(folder, 'results_laceloss.npy')):
        data['laceloss'] = np.load(os.path.join(folder, 'results_laceloss.npy'), allow_pickle=True).item()

    return data

def plot_data(filename, data, title=None):
    compare_dict = dict()
    for br in data.keys():
        compare_dict[f'Opus {br/1000:.1f} kb/s'] = data[br][:, 0]
        compare_dict[f'LACE {br/1000:.1f} kb/s'] = data[br][:, 1]
        compare_dict[f'NoLACE {br/1000:.1f} kb/s'] = data[br][:, 2]

    plt.rcParams.update({
        "text.usetex": True,
        "font.family": "Helvetica",
        "font.size": 32
    })

    black = '#000000'
    red = '#ff5745'
    blue = '#007dbc'
    colors = [black, red, blue]
    legend_elements = [Patch(facecolor=colors[0], label='Opus SILK'),
                    Patch(facecolor=colors[1], label='LACE'),
                    Patch(facecolor=colors[2], label='NoLACE')]

    fig, ax = plt.subplots()
    fig.set_size_inches(40, 20)
    bplot = ax.boxplot(compare_dict.values(), showfliers=False, notch=True, patch_artist=True)

    for i, patch in enumerate(bplot['boxes']):
        patch.set_facecolor(colors[i%3])

    ax.set_xticklabels(compare_dict.keys(), rotation=290)

    if title is not None:
        ax.set_title(title)

    ax.legend(handles=legend_elements)

    fig.savefig(filename, bbox_inches='tight')

if __name__ == "__main__":
    args = parser.parse_args()
    data = load_data(args.folder)


    metrics = list(data.keys()) if args.metric == 'all' else [args.metric]
    folder = args.folder if args.output is None else args.output
    os.makedirs(folder, exist_ok=True)

    for metric in metrics:
        print(f"Plotting data for {metric} metric...")
        plot_data(os.path.join(folder, f"boxplot_{metric}.png"), data[metric], title=metric.upper())

    print("Done.")
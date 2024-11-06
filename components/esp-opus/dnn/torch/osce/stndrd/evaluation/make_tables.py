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

def make_table(filename, data, title=None):

    # mean values
    tbl = PrettyTable()
    tbl.field_names = ['bitrate (bps)', 'Opus', 'LACE', 'NoLACE']
    for br in data.keys():
        opus = data[br][:, 0]
        lace = data[br][:, 1]
        nolace = data[br][:, 2]
        tbl.add_row([br, f"{float(opus.mean()):.3f} ({float(opus.std()):.2f})", f"{float(lace.mean()):.3f} ({float(lace.std()):.2f})", f"{float(nolace.mean()):.3f} ({float(nolace.std()):.2f})"])

    with open(filename + ".txt", "w") as f:
        f.write(str(tbl))

    with open(filename + ".html", "w") as f:
        f.write(tbl.get_html_string())

    with open(filename + ".csv", "w") as f:
        f.write(tbl.get_csv_string())

    print(tbl)


def make_diff_table(filename, data, title=None):

    # mean values
    tbl = PrettyTable()
    tbl.field_names = ['bitrate (bps)', 'LACE - Opus', 'NoLACE - Opus']
    for br in data.keys():
        opus = data[br][:, 0]
        lace = data[br][:, 1] - opus
        nolace = data[br][:, 2] - opus
        tbl.add_row([br, f"{float(lace.mean()):.3f} ({float(lace.std()):.2f})", f"{float(nolace.mean()):.3f} ({float(nolace.std()):.2f})"])

    with open(filename + ".txt", "w") as f:
        f.write(str(tbl))

    with open(filename + ".html", "w") as f:
        f.write(tbl.get_html_string())

    with open(filename + ".csv", "w") as f:
        f.write(tbl.get_csv_string())

    print(tbl)

if __name__ == "__main__":
    args = parser.parse_args()
    data = load_data(args.folder)

    metrics = list(data.keys()) if args.metric == 'all' else [args.metric]
    folder = args.folder if args.output is None else args.output
    os.makedirs(folder, exist_ok=True)

    for metric in metrics:
        print(f"Plotting data for {metric} metric...")
        make_table(os.path.join(folder, f"table_{metric}"), data[metric])
        make_diff_table(os.path.join(folder, f"table_diff_{metric}"), data[metric])

    print("Done.")
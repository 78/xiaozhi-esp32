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
import tempfile
import shutil

import pandas as pd
from scipy.spatial.distance import cdist
from scipy.io import wavfile
import numpy as np

from nomad_audio.nomad import Nomad


parser = argparse.ArgumentParser()
parser.add_argument('folder', type=str, help='folder with processed items')
parser.add_argument('--full-reference', action='store_true', help='use NOMAD as full-reference metric')
parser.add_argument('--device', type=str, default=None, help='device for Nomad')


def get_bitrates(folder):
    with open(os.path.join(folder, 'bitrates.txt')) as f:
        x = f.read()

    bitrates = [int(y) for y in x.rstrip('\n').split()]

    return bitrates

def get_itemlist(folder):
    with open(os.path.join(folder, 'items.txt')) as f:
        lines = f.readlines()

    items = [x.split()[0] for x in lines]

    return items


def nomad_wrapper(ref_folder, deg_folder, full_reference=False, ref_embeddings=None, device=None):
    model = Nomad(device=device)
    if not full_reference:
        results = model.predict(nmr=ref_folder, deg=deg_folder)[0].to_dict()['NOMAD']
        return results, None
    else:
        if ref_embeddings is None:
            print(f"Computing reference embeddings from {ref_folder}")
            ref_data = pd.DataFrame(sorted(os.listdir(ref_folder)))
            ref_data.columns = ['filename']
            ref_data['filename'] = [os.path.join(ref_folder, x) for x in ref_data['filename']]
            ref_embeddings = model.get_embeddings_csv(model.model, ref_data).set_index('filename')

        print(f"Computing degraded embeddings from {deg_folder}")
        deg_data = pd.DataFrame(sorted(os.listdir(deg_folder)))
        deg_data.columns = ['filename']
        deg_data['filename'] = [os.path.join(deg_folder, x) for x in deg_data['filename']]
        deg_embeddings = model.get_embeddings_csv(model.model, deg_data).set_index('filename')

        dist = np.diag(cdist(ref_embeddings, deg_embeddings)) # wasteful
        test_files = [x.split('/')[-1].split('.')[0] for x in deg_embeddings.index]

        results = dict(zip(test_files, dist))

        return results, ref_embeddings




def nomad_process_all(folder, full_reference=False, device=None):
    bitrates = get_bitrates(folder)
    items = get_itemlist(folder)
    with tempfile.TemporaryDirectory() as dir:
        cleandir  = os.path.join(dir, 'clean')
        opusdir   = os.path.join(dir, 'opus')
        lacedir   = os.path.join(dir, 'lace')
        nolacedir = os.path.join(dir, 'nolace')

        # prepare files
        for d in [cleandir, opusdir, lacedir, nolacedir]: os.makedirs(d)
        for br in bitrates:
            for item in items:
                for cond in ['clean', 'opus', 'lace', 'nolace']:
                    shutil.copyfile(os.path.join(folder, cond, f"{item}_{br}_{cond}.wav"), os.path.join(dir, cond, f"{item}_{br}.wav"))

        nomad_opus, ref_embeddings   = nomad_wrapper(cleandir, opusdir, full_reference=full_reference, ref_embeddings=None)
        nomad_lace, ref_embeddings   = nomad_wrapper(cleandir, lacedir, full_reference=full_reference, ref_embeddings=ref_embeddings)
        nomad_nolace, ref_embeddings = nomad_wrapper(cleandir, nolacedir, full_reference=full_reference, ref_embeddings=ref_embeddings)

    results = dict()
    for br in bitrates:
        results[br] = np.zeros((len(items), 3))
        for i, item in enumerate(items):
            key = f"{item}_{br}"
            results[br][i, 0] = nomad_opus[key]
            results[br][i, 1] = nomad_lace[key]
            results[br][i, 2] = nomad_nolace[key]

    return results



if __name__ == "__main__":
    args = parser.parse_args()

    items = get_itemlist(args.folder)
    bitrates = get_bitrates(args.folder)

    results = nomad_process_all(args.folder, full_reference=args.full_reference, device=args.device)

    np.save(os.path.join(args.folder, f'results_nomad.npy'), results)

    print("Done.")

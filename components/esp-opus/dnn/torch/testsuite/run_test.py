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
import multiprocess as multiprocessing
import random
import subprocess
import argparse
import shutil

import yaml

from utils.files import get_wave_file_list
from utils.pesq import compute_PESQ
from utils.pitch import compute_pitch_error


parser = argparse.ArgumentParser()
parser.add_argument('setup', type=str, help='setup yaml specifying end to end processing with model under test')
parser.add_argument('input_folder', type=str, help='input folder path')
parser.add_argument('output_folder', type=str, help='output folder path')
parser.add_argument('--num-testitems', type=int, help="number of testitems to be processed (default 100)", default=100)
parser.add_argument('--seed', type=int, help='seed for random item selection', default=None)
parser.add_argument('--fs', type=int, help="sampling rate at which input is presented as wave file (defaults to 16000)", default=16000)
parser.add_argument('--num-workers', type=int, help="number of subprocesses to be used (default=4)", default=4)
parser.add_argument('--plc-suffix', type=str, default="_is_lost.txt", help="suffix of plc error pattern file: only relevant if command chain uses PLCFILE (default=_is_lost.txt)")
parser.add_argument('--metrics', type=str, default='pesq', help='comma separated string of metrics, supported: {{"pesq", "pitch_error", "voicing_error"}}, default="pesq"')
parser.add_argument('--verbose', action='store_true', help='enables printouts of all commands run in the pipeline')

def check_for_sox_in_path():
    r = subprocess.run("sox -h", shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return r.returncode == 0


def run_save_sh(command, verbose=False):

    if verbose:
        print(f"[run_save_sh] running command {command}...")

    r = subprocess.run(command, shell=True)
    if r.returncode != 0:
        raise RuntimeError(f"command '{command}' failed with exit code {r.returncode}")


def run_processing_chain(input_path, output_path, model_commands, fs, metrics={'pesq'}, plc_suffix="_is_lost.txt", verbose=False):

    # prepare model input
    model_input = output_path + ".resamp.wav"
    run_save_sh(f"sox {input_path} -r {fs} {model_input}", verbose=verbose)

    plcfile = os.path.splitext(input_path)[0] + plc_suffix
    if os.path.isfile(plcfile):
        run_save_sh(f"cp {plcfile} {os.path.dirname(output_path)}")

    # generate model output
    for command in model_commands:
        run_save_sh(command.format(INPUT=model_input, OUTPUT=output_path, PLCFILE=plcfile), verbose=verbose)

    scores = dict()
    cache = dict()
    for metric in metrics:
        if metric == 'pesq':
            # run pesq
            score = compute_PESQ(input_path, output_path, fs=fs)
        elif metric == 'pitch_error':
            if metric in cache:
                score = cache[metric]
            else:
                rval = compute_pitch_error(input_path, output_path, fs=fs)
                score = rval[metric]
                cache['voicing_error'] = rval['voicing_error']
        elif metric == 'voicing_error':
            if metric in cache:
                score = cache[metric]
            else:
                rval = compute_pitch_error(input_path, output_path, fs=fs)
                score = rval[metric]
                cache['pitch_error'] = rval['pitch_error']
        else:
            ValueError(f'error: unknown metric {metric}')

        scores[metric] = score

    return (output_path, scores)


def get_output_path(root_folder, input, output_folder):

    input_relpath = os.path.relpath(input, root_folder)

    os.makedirs(os.path.join(output_folder, 'processing', os.path.dirname(input_relpath)), exist_ok=True)

    output_path = os.path.join(output_folder, 'processing', input_relpath + '.output.wav')

    return output_path


def add_audio_table(f, html_folder, results, title, metric):

    item_folder = os.path.join(html_folder, 'items')
    os.makedirs(item_folder, exist_ok=True)

    # table with results
    f.write(f"""
            <div>
            <h2> {title} </h2>
            <table>
            <tr>
                <th> Rank   </th>
                <th> Name   </th>
                <th> {metric.upper()} </th>
                <th> Audio (out)  </th>
                <th> Audio (orig)  </th>
            </tr>
            """)

    for i, r in enumerate(results):
        item, score = r
        item_name = os.path.basename(item)
        new_item_path = os.path.join(item_folder, item_name)
        shutil.copyfile(item, new_item_path)
        shutil.copyfile(item + '.resamp.wav', os.path.join(item_folder, item_name + '.orig.wav'))

        f.write(f"""
                <tr>
                    <td> {i + 1} </td>
                    <td> {item_name.split('.')[0]} </td>
                    <td> {score:.3f} </td>
                    <td>
                        <audio controls>
                            <source src="items/{item_name}">
                        </audio>
                    </td>
                    <td>
                        <audio controls>
                            <source src="items/{item_name + '.orig.wav'}">
                        </audio>
                    </td>
                </tr>
                """)

    # footer
    f.write("""
            </table>
            </div>
            """)


def create_html(output_folder, results, title, metric):

    html_folder = output_folder
    items_folder = os.path.join(html_folder, 'items')
    os.makedirs(html_folder, exist_ok=True)
    os.makedirs(items_folder, exist_ok=True)

    with open(os.path.join(html_folder, 'index.html'), 'w') as f:
        # header and title
        f.write(f"""
                <!DOCTYPE html>
                <html lang="en">
                <head>
                    <meta charset="utf-8">
                    <title>{title}</title>
                    <style>
                        article {{
                            align-items: flex-start;
                            display: flex;
                            flex-wrap: wrap;
                            gap: 4em;
                        }}
                        html {{
                            box-sizing: border-box;
                            font-family: "Amazon Ember", "Source Sans", "Verdana", "Calibri", sans-serif;
                            padding: 2em;
                        }}
                        td {{
                            padding: 3px 7px;
                            text-align: center;
                        }}
                        td:first-child {{
                            text-align: end;
                        }}
                        th {{
                            background: #ff9900;
                            color: #000;
                            font-size: 1.2em;
                            padding: 7px 7px;
                        }}
                    </style>
                </head>
                </body>
                <h1>{title}</h1>
                <article>
                """)

        # top 20
        add_audio_table(f, html_folder, results[:-21: -1], "Top 20", metric)

        # 20 around median
        N = len(results) // 2
        add_audio_table(f, html_folder, results[N + 10 : N - 10: -1], "Median 20", metric)

        # flop 20
        add_audio_table(f, html_folder, results[:20], "Flop 20", metric)

        # footer
        f.write("""
                </article>
                </body>
                </html>
                """)

metric_sorting_signs = {
    'pesq'          : 1,
    'pitch_error'   : -1,
    'voicing_error' : -1
}

def is_valid_result(data, metrics):
    if not isinstance(data, dict):
        return False

    for metric in metrics:
        if not metric in data:
            return False

    return True


def evaluate_results(output_folder, results, metric):

    results = sorted(results, key=lambda x : metric_sorting_signs[metric] * x[1])
    with open(os.path.join(args.output_folder, f'scores_{metric}.txt'), 'w') as f:
        for result in results:
            f.write(f"{os.path.relpath(result[0], args.output_folder)} {result[1]}\n")


    # some statistics
    mean = sum([r[1] for r in results]) / len(results)
    top_mean = sum([r[1] for r in results[-20:]]) / 20
    bottom_mean = sum([r[1] for r in results[:20]]) / 20

    with open(os.path.join(args.output_folder, f'stats_{metric}.txt'), 'w') as f:
        f.write(f"mean score: {mean}\n")
        f.write(f"bottom mean score: {bottom_mean}\n")
        f.write(f"top mean score: {top_mean}\n")

    print(f"\nmean score: {mean}")
    print(f"bottom mean score: {bottom_mean}")
    print(f"top mean score: {top_mean}\n")

    # create output html
    create_html(os.path.join(output_folder, 'html', metric), results, setup['test'], metric)

if __name__ == "__main__":
    args = parser.parse_args()

    # check for sox
    if not check_for_sox_in_path():
        raise RuntimeError("script requires sox")


    # prepare output folder
    if os.path.exists(args.output_folder):
        print("warning: output folder exists")

        reply = input('continue? (y/n): ')
        while reply not in {'y', 'n'}:
            reply = input('continue? (y/n): ')

        if reply == 'n':
            os._exit()
        else:
            # start with a clean sleight
            shutil.rmtree(args.output_folder)

    os.makedirs(args.output_folder, exist_ok=True)

    # extract metrics
    metrics = args.metrics.split(",")
    for metric in metrics:
        if not metric in metric_sorting_signs:
            print(f"unknown metric {metric}")
            args.usage()

    # read setup
    print(f"loading {args.setup}...")
    with open(args.setup, "r") as f:
        setup = yaml.load(f.read(), yaml.FullLoader)

    model_commands = setup['processing']

    print("\nfound the following model commands:")
    for command in model_commands:
        print(command.format(INPUT='input.wav', OUTPUT='output.wav', PLCFILE='input_is_lost.txt'))

    # store setup to output folder
    setup['input']  = os.path.abspath(args.input_folder)
    setup['output'] = os.path.abspath(args.output_folder)
    setup['seed']   = args.seed
    with open(os.path.join(args.output_folder, 'setup.yml'), 'w') as f:
        yaml.dump(setup, f)

    # get input
    print(f"\nCollecting audio files from {args.input_folder}...")
    file_list = get_wave_file_list(args.input_folder, check_for_features=False)
    print(f"...{len(file_list)} files found\n")

    # sample from file list
    file_list = sorted(file_list)
    random.seed(args.seed)
    random.shuffle(file_list)
    num_testitems = min(args.num_testitems, len(file_list))
    file_list = file_list[:num_testitems]


    print(f"\nlaunching test on {num_testitems} items...")
    # helper function for parallel processing
    def func(input_path):
        output_path = get_output_path(args.input_folder, input_path, args.output_folder)

        try:
            rval = run_processing_chain(input_path, output_path, model_commands, args.fs, metrics=metrics, plc_suffix=args.plc_suffix, verbose=args.verbose)
        except:
            rval = (input_path, -1)

        return rval

    with multiprocessing.Pool(args.num_workers) as p:
        results = p.map(func, file_list)

    results_dict = dict()
    for name, values in results:
        if is_valid_result(values, metrics):
            results_dict[name] = values

    print(results_dict)

    # evaluating results
    num_failures = num_testitems - len(results_dict)
    print(f"\nprocessing of {num_failures} items failed\n")

    for metric in metrics:
        print(metric)
        evaluate_results(
            args.output_folder,
            [(name, value[metric]) for name, value in results_dict.items()],
            metric
        )
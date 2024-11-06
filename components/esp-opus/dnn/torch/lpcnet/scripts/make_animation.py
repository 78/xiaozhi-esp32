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


""" script for creating animations from debug data

"""


import argparse


import sys
sys.path.append('./')

from utils.endoscopy import make_animation, read_data

parser = argparse.ArgumentParser()

parser.add_argument('folder', type=str, help='endoscopy folder with debug output')
parser.add_argument('output', type=str, help='output file (will be auto-extended with .mp4)')

parser.add_argument('--start-index', type=int, help='index of first sample to be considered', default=0)
parser.add_argument('--stop-index', type=int, help='index of last sample to be considered', default=-1)
parser.add_argument('--interval', type=int, help='interval between frames in ms', default=20)
parser.add_argument('--half-window-length', type=int, help='half size of window for displaying signals', default=80)


if __name__ == "__main__":
    args = parser.parse_args()

    filename = args.output if args.output.endswith('.mp4') else args.output + '.mp4'
    data = read_data(args.folder)

    make_animation(
        data,
        filename,
        start_index=args.start_index,
        stop_index = args.stop_index,
        half_signal_window_length=args.half_window_length
    )

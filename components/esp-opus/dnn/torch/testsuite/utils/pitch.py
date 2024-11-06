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

import numpy as np
from scipy.io import wavfile
import amfm_decompy.pYAAPT as pYAAPT
import amfm_decompy.basic_tools as basic

def get_voicing_info(x, sr=16000):

    signal = basic.SignalObj(x, sr)
    pitch = pYAAPT.yaapt(signal, **{'frame_length' : 20.0, 'tda_frame_length' : 20.0})

    pitch_values = pitch.samp_values
    voiced_flags = pitch.vuv.astype('float')

    return pitch_values, voiced_flags

def compute_pitch_error(ref_path, test_path, fs=16000):
    fs_orig, x_orig = wavfile.read(ref_path)
    fs_test, x_test = wavfile.read(test_path)

    min_length = min(len(x_orig), len(x_test))
    x_orig = x_orig[:min_length]
    x_test = x_test[:min_length]

    assert fs_orig == fs_test == fs

    pitch_contour_orig, voicing_orig = get_voicing_info(x_orig.astype(np.float32))
    pitch_contour_test, voicing_test = get_voicing_info(x_test.astype(np.float32))

    return {
        'pitch_error' : np.mean(np.abs(pitch_contour_orig - pitch_contour_test)).item(),
        'voicing_error' : np.sum(np.abs(voicing_orig - voicing_test)).item() / len(voicing_orig)
        }
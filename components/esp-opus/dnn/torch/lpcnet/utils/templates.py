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

from models import multi_rate_lpcnet
import copy

setup_dict = dict()

dataset_template_v2 = {
    'version'               : 2,
    'feature_file'          : 'features.f32',
    'signal_file'           : 'data.s16',
    'frame_length'          : 160,
    'feature_frame_length'  : 36,
    'signal_frame_length'   : 2,
    'feature_dtype'         : 'float32',
    'signal_dtype'          : 'int16',
    'feature_frame_layout'  : {'cepstrum': [0,18], 'periods': [18, 19], 'pitch_corr': [19, 20], 'lpc': [20, 36]},
    'signal_frame_layout'   : {'last_signal' : 0, 'signal': 1} # signal, last_signal, error, prediction
}

dataset_template_v1 = {
    'version'               : 1,
    'feature_file'          : 'features.f32',
    'signal_file'           : 'data.u8',
    'frame_length'          : 160,
    'feature_frame_length'  : 55,
    'signal_frame_length'   : 4,
    'feature_dtype'         : 'float32',
    'signal_dtype'          : 'uint8',
    'feature_frame_layout'  : {'cepstrum': [0,18], 'periods': [36, 37], 'pitch_corr': [37, 38], 'lpc': [39, 55]},
    'signal_frame_layout'   : {'last_signal' : 0, 'prediction' : 1, 'last_error': 2, 'error': 3} # signal, last_signal, error, prediction
}

# lpcnet

lpcnet_config = {
    'frame_size'                : 160,
    'gru_a_units'               : 384,
    'gru_b_units'               : 64,
    'feature_conditioning_dim'  : 128,
    'feature_conv_kernel_size'  : 3,
    'period_levels'             : 257,
    'period_embedding_dim'      : 64,
    'signal_embedding_dim'      : 128,
    'signal_levels'             : 256,
    'feature_dimension'         : 19,
    'output_levels'             : 256,
    'lpc_gamma'                 : 0.9,
    'features'                  : ['cepstrum', 'periods', 'pitch_corr'],
    'signals'                   : ['last_signal', 'prediction', 'last_error'],
    'input_layout'              : { 'signals'  : {'last_signal' : 0, 'prediction' : 1, 'last_error' : 2},
                                    'features' : {'cepstrum' : [0, 18], 'pitch_corr' : [18, 19]} },
    'target'                    : 'error',
    'feature_history'           : 2,
    'feature_lookahead'         : 2,
    'sparsification'            :   {
       'gru_a' :  {
            'start'     : 10000,
            'stop'      : 30000,
            'interval'  : 100,
            'exponent'  : 3,
            'params'   :   {
                'W_hr' : (0.05, [4, 8], True),
                'W_hz' : (0.05, [4, 8], True),
                'W_hn' : (0.2,  [4, 8], True)
                },
        },
       'gru_b' :  {
            'start'     : 10000,
            'stop'      : 30000,
            'interval'  : 100,
            'exponent'  : 3,
            'params'   :   {
                'W_ir' : (0.5, [4, 8], False),
                'W_iz' : (0.5, [4, 8], False),
                'W_in' : (0.5,  [4, 8], False)
                },
        }
    },
    'add_reference_phase'       : False,
    'reference_phase_dim'       : 0
}



# multi rate
subconditioning = {
        'subconditioning_a' : {
            'number_of_subsamples'  : 2,
            'method'                : 'modulative',
            'signals'               : ['last_signal', 'prediction', 'last_error'],
            'pcm_embedding_size'    : 64,
            'kwargs'                : dict()

        },
        'subconditioning_b' : {
            'number_of_subsamples'  : 2,
            'method'                : 'modulative',
            'signals'               : ['last_signal', 'prediction', 'last_error'],
            'pcm_embedding_size'    : 64,
            'kwargs'                : dict()
        }
}

multi_rate_lpcnet_config = lpcnet_config.copy()
multi_rate_lpcnet_config['subconditioning'] = subconditioning

training_default = {
    'batch_size'        : 256,
    'epochs'            : 20,
    'lr'                : 1e-3,
    'lr_decay_factor'   : 2.5e-5,
    'adam_betas'        : [0.9, 0.99],
    'frames_per_sample' : 15
}

lpcnet_setup = {
    'dataset'       : '/local/datasets/lpcnet_training',
    'lpcnet'        : {'config' : lpcnet_config, 'model': 'lpcnet'},
    'training'      : training_default
}

multi_rate_lpcnet_setup = copy.deepcopy(lpcnet_setup)
multi_rate_lpcnet_setup['lpcnet']['config'] = multi_rate_lpcnet_config
multi_rate_lpcnet_setup['lpcnet']['model'] = 'multi_rate'

setup_dict = {
    'lpcnet'     : lpcnet_setup,
    'multi_rate' : multi_rate_lpcnet_setup
}

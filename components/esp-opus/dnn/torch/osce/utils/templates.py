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


setup_dict = dict()

lace_setup = {
    'dataset': '/local/datasets/silk_enhancement_v2_full_6to64kbps/training',
    'validation_dataset': '/local/datasets/silk_enhancement_v2_full_6to64kbps/validation',
    'model': {
        'name': 'lace',
        'args': [],
        'kwargs': {
            'comb_gain_limit_db': 10,
            'cond_dim': 128,
            'conv_gain_limits_db': [-12, 12],
            'global_gain_limits_db': [-6, 6],
            'hidden_feature_dim': 96,
            'kernel_size': 15,
            'num_features': 93,
            'numbits_embedding_dim': 8,
            'numbits_range': [50, 650],
            'partial_lookahead': True,
            'pitch_embedding_dim': 64,
            'pitch_max': 300,
            'preemph': 0.85,
            'skip': 91,
            'softquant': True,
            'sparsify': False,
            'sparsification_density': 0.4,
            'sparsification_schedule': [10000, 40000, 200]
        }
    },
    'data': {
        'frames_per_sample': 100,
        'no_pitch_value': 7,
        'preemph': 0.85,
        'skip': 91,
        'pitch_hangover': 8,
        'acorr_radius': 2,
        'num_bands_clean_spec': 64,
        'num_bands_noisy_spec': 18,
        'noisy_spec_scale': 'opus',
        'pitch_hangover': 0,
    },
    'training': {
        'batch_size': 256,
        'lr': 5.e-4,
        'lr_decay_factor': 2.5e-5,
        'epochs': 50,
        'loss': {
            'w_l1': 0,
            'w_lm': 0,
            'w_logmel': 0,
            'w_sc': 0,
            'w_wsc': 0,
            'w_xcorr': 0,
            'w_sxcorr': 1,
            'w_l2': 10,
            'w_slm': 2
        }
    }
}


nolace_setup = {
    'dataset': '/local/datasets/silk_enhancement_v2_full_6to64kbps/training',
    'validation_dataset': '/local/datasets/silk_enhancement_v2_full_6to64kbps/validation',
    'model': {
        'name': 'nolace',
        'args': [],
        'kwargs': {
            'avg_pool_k': 4,
            'comb_gain_limit_db': 10,
            'cond_dim': 256,
            'conv_gain_limits_db': [-12, 12],
            'global_gain_limits_db': [-6, 6],
            'hidden_feature_dim': 96,
            'kernel_size': 15,
            'num_features': 93,
            'numbits_embedding_dim': 8,
            'numbits_range': [50, 650],
            'partial_lookahead': True,
            'pitch_embedding_dim': 64,
            'pitch_max': 300,
            'preemph': 0.85,
            'skip': 91,
            'softquant': True,
            'sparsify': False,
            'sparsification_density': 0.4,
            'sparsification_schedule': [10000, 40000, 200]
        }
    },
    'data': {
        'frames_per_sample': 100,
        'no_pitch_value': 7,
        'preemph': 0.85,
        'skip': 91,
        'pitch_hangover': 8,
        'acorr_radius': 2,
        'num_bands_clean_spec': 64,
        'num_bands_noisy_spec': 18,
        'noisy_spec_scale': 'opus',
        'pitch_hangover': 0,
    },
    'training': {
        'batch_size': 256,
        'lr': 5.e-4,
        'lr_decay_factor': 2.5e-5,
        'epochs': 50,
        'loss': {
            'w_l1': 0,
            'w_lm': 0,
            'w_logmel': 0,
            'w_sc': 0,
            'w_wsc': 0,
            'w_xcorr': 0,
            'w_sxcorr': 1,
            'w_l2': 10,
            'w_slm': 2
        }
    }
}

nolace_setup_adv = {
    'dataset': '/local/datasets/silk_enhancement_v2_full_6to64kbps/training',
    'model': {
        'name': 'nolace',
        'args': [],
        'kwargs': {
            'avg_pool_k': 4,
            'comb_gain_limit_db': 10,
            'cond_dim': 256,
            'conv_gain_limits_db': [-12, 12],
            'global_gain_limits_db': [-6, 6],
            'hidden_feature_dim': 96,
            'kernel_size': 15,
            'num_features': 93,
            'numbits_embedding_dim': 8,
            'numbits_range': [50, 650],
            'partial_lookahead': True,
            'pitch_embedding_dim': 64,
            'pitch_max': 300,
            'preemph': 0.85,
            'skip': 91,
            'softquant': True,
            'sparsify': False,
            'sparsification_density': 0.4,
            'sparsification_schedule': [0, 0, 200]
        }
    },
    'data': {
        'frames_per_sample': 100,
        'no_pitch_value': 7,
        'preemph': 0.85,
        'skip': 91,
        'pitch_hangover': 8,
        'acorr_radius': 2,
        'num_bands_clean_spec': 64,
        'num_bands_noisy_spec': 18,
        'noisy_spec_scale': 'opus',
        'pitch_hangover': 0,
    },
    'discriminator': {
        'args': [],
        'kwargs': {
            'architecture': 'free',
            'design': 'f_down',
            'fft_sizes_16k': [
                64,
                128,
                256,
                512,
                1024,
                2048,
            ],
            'freq_roi': [0, 7400],
            'fs': 16000,
            'max_channels': 256,
            'noise_gain': 0.0,
        },
        'name': 'fdmresdisc',
    },
    'training': {
        'adv_target': 'target_orig',
        'batch_size': 64,
        'epochs': 50,
        'gen_lr_reduction': 1,
        'lambda_feat': 1.0,
        'lambda_reg': 0.6,
        'loss': {
            'w_l1': 0,
            'w_l2': 10,
            'w_lm': 0,
            'w_logmel': 0,
            'w_sc': 0,
            'w_slm': 20,
            'w_sxcorr': 1,
            'w_wsc': 0,
            'w_xcorr': 0,
        },
        'lr': 0.0001,
        'lr_decay_factor': 2.5e-09,
    }
}


lavoce_setup = {
    'data': {
        'frames_per_sample': 100,
        'target': 'signal'
    },
    'dataset': '/local/datasets/lpcnet_large/training',
    'model': {
        'args': [],
        'kwargs': {
            'comb_gain_limit_db': 10,
            'cond_dim': 256,
            'conv_gain_limits_db': [-12, 12],
            'global_gain_limits_db': [-6, 6],
            'kernel_size': 15,
            'num_features': 19,
            'pitch_embedding_dim': 64,
            'pitch_max': 300,
            'preemph': 0.85,
            'pulses': True
            },
        'name': 'lavoce'
    },
    'training': {
        'batch_size': 256,
        'epochs': 50,
        'loss': {
            'w_l1': 0,
            'w_l2': 0,
            'w_lm': 0,
            'w_logmel': 0,
            'w_sc': 0,
            'w_slm': 2,
            'w_sxcorr': 1,
            'w_wsc': 0,
            'w_xcorr': 0
        },
        'lr': 0.0005,
        'lr_decay_factor': 2.5e-05
    },
    'validation_dataset': '/local/datasets/lpcnet_large/validation'
}

lavoce_setup_adv = {
    'data': {
        'frames_per_sample': 100,
        'target': 'signal'
    },
    'dataset': '/local/datasets/lpcnet_large/training',
    'discriminator': {
        'args': [],
        'kwargs': {
            'architecture': 'free',
            'design': 'f_down',
            'fft_sizes_16k': [
                64,
                128,
                256,
                512,
                1024,
                2048,
            ],
            'freq_roi': [0, 7400],
            'fs': 16000,
            'max_channels': 256,
            'noise_gain': 0.0,
        },
        'name': 'fdmresdisc',
    },
    'model': {
        'args': [],
        'kwargs': {
            'comb_gain_limit_db': 10,
            'cond_dim': 256,
            'conv_gain_limits_db': [-12, 12],
            'global_gain_limits_db': [-6, 6],
            'kernel_size': 15,
            'num_features': 19,
            'pitch_embedding_dim': 64,
            'pitch_max': 300,
            'preemph': 0.85,
            'pulses': True
            },
        'name': 'lavoce'
    },
    'training': {
        'batch_size': 64,
        'epochs': 50,
        'gen_lr_reduction': 1,
        'lambda_feat': 1.0,
        'lambda_reg': 0.6,
        'loss': {
            'w_l1': 0,
            'w_l2': 0,
            'w_lm': 0,
            'w_logmel': 0,
            'w_sc': 0,
            'w_slm': 2,
            'w_sxcorr': 1,
            'w_wsc': 0,
            'w_xcorr': 0
        },
        'lr': 0.0001,
        'lr_decay_factor': 2.5e-09
    },
}


setup_dict = {
    'lace': lace_setup,
    'nolace': nolace_setup,
    'nolace_adv': nolace_setup_adv,
    'lavoce': lavoce_setup,
    'lavoce_adv': lavoce_setup_adv
}

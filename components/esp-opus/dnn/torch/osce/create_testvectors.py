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

import torch
import numpy as np

from models import model_dict
from utils import endoscopy

parser = argparse.ArgumentParser()

parser.add_argument('checkpoint_path', type=str, help='path to folder containing checkpoints "lace_checkpoint.pth" and nolace_checkpoint.pth"')
parser.add_argument('output_folder', type=str, help='output folder for testvectors')
parser.add_argument('--debug', action='store_true', help='add debug output to output folder')


def create_adaconv_testvector(prefix, adaconv, num_frames, debug=False):
    feature_dim = adaconv.feature_dim
    in_channels = adaconv.in_channels
    out_channels = adaconv.out_channels
    frame_size = adaconv.frame_size

    features = torch.randn((1, num_frames, feature_dim))
    x_in = torch.randn((1, in_channels, num_frames * frame_size))

    x_out = adaconv(x_in, features, debug=debug)

    features = features[0].detach().numpy()
    x_in = x_in[0].reshape(in_channels, num_frames, frame_size).permute(1, 0, 2).detach().numpy()
    x_out = x_out[0].reshape(out_channels, num_frames, frame_size).permute(1, 0, 2).detach().numpy()

    features.tofile(prefix + '_features.f32')
    x_in.tofile(prefix + '_x_in.f32')
    x_out.tofile(prefix + '_x_out.f32')

def create_adacomb_testvector(prefix, adacomb, num_frames, debug=False):
    feature_dim = adacomb.feature_dim
    in_channels = 1
    frame_size = adacomb.frame_size

    features = torch.randn((1, num_frames, feature_dim))
    x_in = torch.randn((1, in_channels, num_frames * frame_size))
    p_in = torch.randint(adacomb.kernel_size, 250, (1, num_frames))

    x_out = adacomb(x_in, features, p_in, debug=debug)

    features = features[0].detach().numpy()
    x_in = x_in[0].permute(1, 0).detach().numpy()
    p_in = p_in[0].detach().numpy().astype(np.int32)
    x_out = x_out[0].permute(1, 0).detach().numpy()

    features.tofile(prefix + '_features.f32')
    x_in.tofile(prefix + '_x_in.f32')
    p_in.tofile(prefix + '_p_in.s32')
    x_out.tofile(prefix + '_x_out.f32')

def create_adashape_testvector(prefix, adashape, num_frames):
    feature_dim = adashape.feature_dim
    frame_size = adashape.frame_size

    features = torch.randn((1, num_frames, feature_dim))
    x_in = torch.randn((1, 1, num_frames * frame_size))

    x_out = adashape(x_in, features)

    features = features[0].detach().numpy()
    x_in = x_in.flatten().detach().numpy()
    x_out = x_out.flatten().detach().numpy()

    features.tofile(prefix + '_features.f32')
    x_in.tofile(prefix + '_x_in.f32')
    x_out.tofile(prefix + '_x_out.f32')

def create_feature_net_testvector(prefix, model, num_frames):
    num_features = model.num_features
    num_subframes = 4 * num_frames

    input_features = torch.randn((1, num_subframes, num_features))
    periods = torch.randint(32, 300, (1, num_subframes))
    numbits = model.numbits_range[0] + torch.rand((1, num_frames, 2)) * (model.numbits_range[1] - model.numbits_range[0])


    pembed = model.pitch_embedding(periods)
    nembed = torch.repeat_interleave(model.numbits_embedding(numbits).flatten(2), 4, dim=1)
    full_features = torch.cat((input_features, pembed, nembed), dim=-1)

    cf = model.feature_net(full_features)

    input_features.float().numpy().tofile(prefix + "_in_features.f32")
    periods.numpy().astype(np.int32).tofile(prefix + "_periods.s32")
    numbits.float().numpy().tofile(prefix + "_numbits.f32")
    full_features.detach().numpy().tofile(prefix + "_full_features.f32")
    cf.detach().numpy().tofile(prefix + "_out_features.f32")



if __name__ == "__main__":
    args = parser.parse_args()

    os.makedirs(args.output_folder, exist_ok=True)

    lace_checkpoint = torch.load(os.path.join(args.checkpoint_path, "lace_checkpoint.pth"), map_location='cpu')
    nolace_checkpoint = torch.load(os.path.join(args.checkpoint_path, "nolace_checkpoint.pth"), map_location='cpu')

    lace = model_dict['lace'](**lace_checkpoint['setup']['model']['kwargs'])
    nolace = model_dict['nolace'](**nolace_checkpoint['setup']['model']['kwargs'])

    lace.load_state_dict(lace_checkpoint['state_dict'])
    nolace.load_state_dict(nolace_checkpoint['state_dict'])

    if args.debug:
        endoscopy.init(args.output_folder)

    # lace af1, 1 input channel, 1 output channel
    create_adaconv_testvector(os.path.join(args.output_folder, "lace_af1"), lace.af1, 5, debug=args.debug)

    # nolace af1, 1 input channel, 2 output channels
    create_adaconv_testvector(os.path.join(args.output_folder, "nolace_af1"), nolace.af1, 5, debug=args.debug)

    # nolace af4, 2 input channel, 1 output channels
    create_adaconv_testvector(os.path.join(args.output_folder, "nolace_af4"), nolace.af4, 5, debug=args.debug)

    # nolace af2, 2 input channel, 2 output channels
    create_adaconv_testvector(os.path.join(args.output_folder, "nolace_af2"), nolace.af2, 5, debug=args.debug)

    # lace cf1
    create_adacomb_testvector(os.path.join(args.output_folder, "lace_cf1"), lace.cf1, 5, debug=args.debug)

    # nolace tdshape1
    create_adashape_testvector(os.path.join(args.output_folder, "nolace_tdshape1"), nolace.tdshape1, 5)

    # lace feature net
    create_feature_net_testvector(os.path.join(args.output_folder, 'lace'), lace, 5)

    if args.debug:
        endoscopy.close()

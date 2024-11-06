/* Copyright (c) 2022 Amazon
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

#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "dred_rdovae_enc.h"
#include "os_support.h"
#include "dred_rdovae_constants.h"

static void conv1_cond_init(float *mem, int len, int dilation, int *init)
{
    if (!*init) {
        int i;
        for (i=0;i<dilation;i++) OPUS_CLEAR(&mem[i*len], len);
    }
    *init = 1;
}

void dred_rdovae_encode_dframe(
    RDOVAEEncState *enc_state,           /* io: encoder state */
    const RDOVAEEnc *model,
    float *latents,                 /* o: latent vector */
    float *initial_state,           /* o: initial state */
    const float *input,              /* i: double feature frame (concatenated) */
    int arch
    )
{
    float padded_latents[DRED_PADDED_LATENT_DIM];
    float padded_state[DRED_PADDED_STATE_DIM];
    float buffer[ENC_DENSE1_OUT_SIZE + ENC_GRU1_OUT_SIZE + ENC_GRU2_OUT_SIZE + ENC_GRU3_OUT_SIZE + ENC_GRU4_OUT_SIZE + ENC_GRU5_OUT_SIZE
               + ENC_CONV1_OUT_SIZE + ENC_CONV2_OUT_SIZE + ENC_CONV3_OUT_SIZE + ENC_CONV4_OUT_SIZE + ENC_CONV5_OUT_SIZE];
    float state_hidden[GDENSE1_OUT_SIZE];
    int output_index = 0;

    /* run encoder stack and concatenate output in buffer*/
    compute_generic_dense(&model->enc_dense1, &buffer[output_index], input, ACTIVATION_TANH, arch);
    output_index += ENC_DENSE1_OUT_SIZE;

    compute_generic_gru(&model->enc_gru1_input, &model->enc_gru1_recurrent, enc_state->gru1_state, buffer, arch);
    OPUS_COPY(&buffer[output_index], enc_state->gru1_state, ENC_GRU1_OUT_SIZE);
    output_index += ENC_GRU1_OUT_SIZE;
    conv1_cond_init(enc_state->conv1_state, output_index, 1, &enc_state->initialized);
    compute_generic_conv1d(&model->enc_conv1, &buffer[output_index], enc_state->conv1_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += ENC_CONV1_OUT_SIZE;

    compute_generic_gru(&model->enc_gru2_input, &model->enc_gru2_recurrent, enc_state->gru2_state, buffer, arch);
    OPUS_COPY(&buffer[output_index], enc_state->gru2_state, ENC_GRU2_OUT_SIZE);
    output_index += ENC_GRU2_OUT_SIZE;
    conv1_cond_init(enc_state->conv2_state, output_index, 2, &enc_state->initialized);
    compute_generic_conv1d_dilation(&model->enc_conv2, &buffer[output_index], enc_state->conv2_state, buffer, output_index, 2, ACTIVATION_TANH, arch);
    output_index += ENC_CONV2_OUT_SIZE;

    compute_generic_gru(&model->enc_gru3_input, &model->enc_gru3_recurrent, enc_state->gru3_state, buffer, arch);
    OPUS_COPY(&buffer[output_index], enc_state->gru3_state, ENC_GRU3_OUT_SIZE);
    output_index += ENC_GRU3_OUT_SIZE;
    conv1_cond_init(enc_state->conv3_state, output_index, 2, &enc_state->initialized);
    compute_generic_conv1d_dilation(&model->enc_conv3, &buffer[output_index], enc_state->conv3_state, buffer, output_index, 2, ACTIVATION_TANH, arch);
    output_index += ENC_CONV3_OUT_SIZE;

    compute_generic_gru(&model->enc_gru4_input, &model->enc_gru4_recurrent, enc_state->gru4_state, buffer, arch);
    OPUS_COPY(&buffer[output_index], enc_state->gru4_state, ENC_GRU4_OUT_SIZE);
    output_index += ENC_GRU4_OUT_SIZE;
    conv1_cond_init(enc_state->conv4_state, output_index, 2, &enc_state->initialized);
    compute_generic_conv1d_dilation(&model->enc_conv4, &buffer[output_index], enc_state->conv4_state, buffer, output_index, 2, ACTIVATION_TANH, arch);
    output_index += ENC_CONV4_OUT_SIZE;

    compute_generic_gru(&model->enc_gru5_input, &model->enc_gru5_recurrent, enc_state->gru5_state, buffer, arch);
    OPUS_COPY(&buffer[output_index], enc_state->gru5_state, ENC_GRU5_OUT_SIZE);
    output_index += ENC_GRU5_OUT_SIZE;
    conv1_cond_init(enc_state->conv5_state, output_index, 2, &enc_state->initialized);
    compute_generic_conv1d_dilation(&model->enc_conv5, &buffer[output_index], enc_state->conv5_state, buffer, output_index, 2, ACTIVATION_TANH, arch);
    output_index += ENC_CONV5_OUT_SIZE;

    compute_generic_dense(&model->enc_zdense, padded_latents, buffer, ACTIVATION_LINEAR, arch);
    OPUS_COPY(latents, padded_latents, DRED_LATENT_DIM);

    /* next, calculate initial state */
    compute_generic_dense(&model->gdense1, state_hidden, buffer, ACTIVATION_TANH, arch);
    compute_generic_dense(&model->gdense2, padded_state, state_hidden, ACTIVATION_LINEAR, arch);
    OPUS_COPY(initial_state, padded_state, DRED_STATE_DIM);
}

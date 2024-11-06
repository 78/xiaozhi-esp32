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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dred_rdovae_dec.h"
#include "dred_rdovae_constants.h"
#include "os_support.h"

static void conv1_cond_init(float *mem, int len, int dilation, int *init)
{
    if (!*init) {
        int i;
        for (i=0;i<dilation;i++) OPUS_CLEAR(&mem[i*len], len);
    }
    *init = 1;
}

void DRED_rdovae_decode_all(const RDOVAEDec *model, float *features, const float *state, const float *latents, int nb_latents, int arch)
{
    int i;
    RDOVAEDecState dec;
    memset(&dec, 0, sizeof(dec));
    dred_rdovae_dec_init_states(&dec, model, state, arch);
    for (i = 0; i < 2*nb_latents; i += 2)
    {
        dred_rdovae_decode_qframe(
            &dec,
            model,
            &features[2*i*DRED_NUM_FEATURES],
            &latents[(i/2)*DRED_LATENT_DIM],
            arch);
    }
}

void dred_rdovae_dec_init_states(
    RDOVAEDecState *h,            /* io: state buffer handle */
    const RDOVAEDec *model,
    const float *initial_state,  /* i: initial state */
    int arch
    )
{
    float hidden[DEC_HIDDEN_INIT_OUT_SIZE];
    float state_init[DEC_GRU1_STATE_SIZE+DEC_GRU2_STATE_SIZE+DEC_GRU3_STATE_SIZE+DEC_GRU4_STATE_SIZE+DEC_GRU5_STATE_SIZE];
    int counter=0;
    compute_generic_dense(&model->dec_hidden_init, hidden, initial_state, ACTIVATION_TANH, arch);
    compute_generic_dense(&model->dec_gru_init, state_init, hidden, ACTIVATION_TANH, arch);
    OPUS_COPY(h->gru1_state, state_init, DEC_GRU1_STATE_SIZE);
    counter += DEC_GRU1_STATE_SIZE;
    OPUS_COPY(h->gru2_state, &state_init[counter], DEC_GRU2_STATE_SIZE);
    counter += DEC_GRU2_STATE_SIZE;
    OPUS_COPY(h->gru3_state, &state_init[counter], DEC_GRU3_STATE_SIZE);
    counter += DEC_GRU3_STATE_SIZE;
    OPUS_COPY(h->gru4_state, &state_init[counter], DEC_GRU4_STATE_SIZE);
    counter += DEC_GRU4_STATE_SIZE;
    OPUS_COPY(h->gru5_state, &state_init[counter], DEC_GRU5_STATE_SIZE);
    h->initialized = 0;
}


void dred_rdovae_decode_qframe(
    RDOVAEDecState *dec_state,       /* io: state buffer handle */
    const RDOVAEDec *model,
    float *qframe,              /* o: quadruple feature frame (four concatenated frames in reverse order) */
    const float *input,          /* i: latent vector */
    int arch
    )
{
    float buffer[DEC_DENSE1_OUT_SIZE + DEC_GRU1_OUT_SIZE + DEC_GRU2_OUT_SIZE + DEC_GRU3_OUT_SIZE + DEC_GRU4_OUT_SIZE + DEC_GRU5_OUT_SIZE
                 + DEC_CONV1_OUT_SIZE + DEC_CONV2_OUT_SIZE + DEC_CONV3_OUT_SIZE + DEC_CONV4_OUT_SIZE + DEC_CONV5_OUT_SIZE];
    int output_index = 0;

    /* run encoder stack and concatenate output in buffer*/
    compute_generic_dense(&model->dec_dense1, &buffer[output_index], input, ACTIVATION_TANH, arch);
    output_index += DEC_DENSE1_OUT_SIZE;

    compute_generic_gru(&model->dec_gru1_input, &model->dec_gru1_recurrent, dec_state->gru1_state, buffer, arch);
    compute_glu(&model->dec_glu1, &buffer[output_index], dec_state->gru1_state, arch);
    output_index += DEC_GRU1_OUT_SIZE;
    conv1_cond_init(dec_state->conv1_state, output_index, 1, &dec_state->initialized);
    compute_generic_conv1d(&model->dec_conv1, &buffer[output_index], dec_state->conv1_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += DEC_CONV1_OUT_SIZE;

    compute_generic_gru(&model->dec_gru2_input, &model->dec_gru2_recurrent, dec_state->gru2_state, buffer, arch);
    compute_glu(&model->dec_glu2, &buffer[output_index], dec_state->gru2_state, arch);
    output_index += DEC_GRU2_OUT_SIZE;
    conv1_cond_init(dec_state->conv2_state, output_index, 1, &dec_state->initialized);
    compute_generic_conv1d(&model->dec_conv2, &buffer[output_index], dec_state->conv2_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += DEC_CONV2_OUT_SIZE;

    compute_generic_gru(&model->dec_gru3_input, &model->dec_gru3_recurrent, dec_state->gru3_state, buffer, arch);
    compute_glu(&model->dec_glu3, &buffer[output_index], dec_state->gru3_state, arch);
    output_index += DEC_GRU3_OUT_SIZE;
    conv1_cond_init(dec_state->conv3_state, output_index, 1, &dec_state->initialized);
    compute_generic_conv1d(&model->dec_conv3, &buffer[output_index], dec_state->conv3_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += DEC_CONV3_OUT_SIZE;

    compute_generic_gru(&model->dec_gru4_input, &model->dec_gru4_recurrent, dec_state->gru4_state, buffer, arch);
    compute_glu(&model->dec_glu4, &buffer[output_index], dec_state->gru4_state, arch);
    output_index += DEC_GRU4_OUT_SIZE;
    conv1_cond_init(dec_state->conv4_state, output_index, 1, &dec_state->initialized);
    compute_generic_conv1d(&model->dec_conv4, &buffer[output_index], dec_state->conv4_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += DEC_CONV4_OUT_SIZE;

    compute_generic_gru(&model->dec_gru5_input, &model->dec_gru5_recurrent, dec_state->gru5_state, buffer, arch);
    compute_glu(&model->dec_glu5, &buffer[output_index], dec_state->gru5_state, arch);
    output_index += DEC_GRU5_OUT_SIZE;
    conv1_cond_init(dec_state->conv5_state, output_index, 1, &dec_state->initialized);
    compute_generic_conv1d(&model->dec_conv5, &buffer[output_index], dec_state->conv5_state, buffer, output_index, ACTIVATION_TANH, arch);
    output_index += DEC_CONV5_OUT_SIZE;

    compute_generic_dense(&model->dec_output, qframe, buffer, ACTIVATION_LINEAR, arch);
}

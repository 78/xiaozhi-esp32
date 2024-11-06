/* Copyright (c) 2018 Mozilla */
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
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
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

#include <math.h>
#include <stdio.h>
#include "nnet_data.h"
#include "nnet.h"
#include "common.h"
#include "arch.h"
#include "lpcnet.h"
#include "lpcnet_private.h"
#include "os_support.h"

#define PREEMPH 0.85f

#define PDF_FLOOR 0.002

#define FRAME_INPUT_SIZE (NB_FEATURES + EMBED_PITCH_OUT_SIZE)


#if 0
static void print_vector(float *x, int N)
{
    int i;
    for (i=0;i<N;i++) printf("%f ", x[i]);
    printf("\n");
}
#endif

#ifdef END2END
void rc2lpc(float *lpc, const float *rc)
{
  int i, j, k;
  float tmp[LPC_ORDER];
  float ntmp[LPC_ORDER] = {0.0};
  OPUS_COPY(tmp, rc, LPC_ORDER);
  for(i = 0; i < LPC_ORDER ; i++)
    {
        for(j = 0; j <= i-1; j++)
        {
            ntmp[j] = tmp[j] + tmp[i]*tmp[i - j - 1];
        }
        for(k = 0; k <= i-1; k++)
        {
            tmp[k] = ntmp[k];
        }
    }
  for(i = 0; i < LPC_ORDER ; i++)
  {
    lpc[i] = tmp[i];
  }
}

#endif

void run_frame_network(LPCNetState *lpcnet, float *gru_a_condition, float *gru_b_condition, float *lpc, const float *features)
{
    NNetState *net;
    float condition[FEATURE_DENSE2_OUT_SIZE];
    float in[FRAME_INPUT_SIZE];
    float conv1_out[FEATURE_CONV1_OUT_SIZE];
    float conv2_out[FEATURE_CONV2_OUT_SIZE];
    float dense1_out[FEATURE_DENSE1_OUT_SIZE];
    int pitch;
    float rc[LPC_ORDER];
    /* Matches the Python code -- the 0.1 avoids rounding issues. */
    pitch = (int)floor(.1 + 50*features[NB_BANDS]+100);
    pitch = IMIN(255, IMAX(33, pitch));
    net = &lpcnet->nnet;
    OPUS_COPY(in, features, NB_FEATURES);
    compute_embedding(&lpcnet->model.embed_pitch, &in[NB_FEATURES], pitch);
    compute_conv1d(&lpcnet->model.feature_conv1, conv1_out, net->feature_conv1_state, in);
    if (lpcnet->frame_count < FEATURE_CONV1_DELAY) OPUS_CLEAR(conv1_out, FEATURE_CONV1_OUT_SIZE);
    compute_conv1d(&lpcnet->model.feature_conv2, conv2_out, net->feature_conv2_state, conv1_out);
    if (lpcnet->frame_count < FEATURES_DELAY) OPUS_CLEAR(conv2_out, FEATURE_CONV2_OUT_SIZE);
    _lpcnet_compute_dense(&lpcnet->model.feature_dense1, dense1_out, conv2_out);
    _lpcnet_compute_dense(&lpcnet->model.feature_dense2, condition, dense1_out);
    OPUS_COPY(rc, condition, LPC_ORDER);
    _lpcnet_compute_dense(&lpcnet->model.gru_a_dense_feature, gru_a_condition, condition);
    _lpcnet_compute_dense(&lpcnet->model.gru_b_dense_feature, gru_b_condition, condition);
#ifdef END2END
    rc2lpc(lpc, rc);
#elif FEATURES_DELAY>0
    memcpy(lpc, lpcnet->old_lpc[FEATURES_DELAY-1], LPC_ORDER*sizeof(lpc[0]));
    memmove(lpcnet->old_lpc[1], lpcnet->old_lpc[0], (FEATURES_DELAY-1)*LPC_ORDER*sizeof(lpc[0]));
    lpc_from_cepstrum(lpcnet->old_lpc[0], features);
#else
    lpc_from_cepstrum(lpc, features);
#endif
#ifdef LPC_GAMMA
    lpc_weighting(lpc, LPC_GAMMA);
#endif
    if (lpcnet->frame_count < 1000) lpcnet->frame_count++;
}

void run_frame_network_deferred(LPCNetState *lpcnet, const float *features)
{
    int max_buffer_size = lpcnet->model.feature_conv1.kernel_size + lpcnet->model.feature_conv2.kernel_size - 2;
    celt_assert(max_buffer_size <= MAX_FEATURE_BUFFER_SIZE);
    if (lpcnet->feature_buffer_fill == max_buffer_size) {
        OPUS_MOVE(lpcnet->feature_buffer, &lpcnet->feature_buffer[NB_FEATURES],  (max_buffer_size-1)*NB_FEATURES);
    } else {
      lpcnet->feature_buffer_fill++;
    }
    OPUS_COPY(&lpcnet->feature_buffer[(lpcnet->feature_buffer_fill-1)*NB_FEATURES], features, NB_FEATURES);
}

void run_frame_network_flush(LPCNetState *lpcnet)
{
    int i;
    for (i=0;i<lpcnet->feature_buffer_fill;i++) {
        float lpc[LPC_ORDER];
        float gru_a_condition[3*GRU_A_STATE_SIZE];
        float gru_b_condition[3*GRU_B_STATE_SIZE];
        run_frame_network(lpcnet, gru_a_condition, gru_b_condition, lpc, &lpcnet->feature_buffer[i*NB_FEATURES]);
    }
    lpcnet->feature_buffer_fill = 0;
}

int run_sample_network(LPCNetState *lpcnet, const float *gru_a_condition, const float *gru_b_condition, int last_exc, int last_sig, int pred, const float *sampling_logit_table, kiss99_ctx *rng)
{
    NNetState *net;
    float gru_a_input[3*GRU_A_STATE_SIZE];
    float in_b[GRU_A_STATE_SIZE+FEATURE_DENSE2_OUT_SIZE];
    float gru_b_input[3*GRU_B_STATE_SIZE];
    net = &lpcnet->nnet;
#if 1
    compute_gru_a_input(gru_a_input, gru_a_condition, GRU_A_STATE_SIZE, &lpcnet->model.gru_a_embed_sig, last_sig, &lpcnet->model.gru_a_embed_pred, pred, &lpcnet->model.gru_a_embed_exc, last_exc);
#else
    OPUS_COPY(gru_a_input, gru_a_condition, 3*GRU_A_STATE_SIZE);
    accum_embedding(&lpcnet->model.gru_a_embed_sig, gru_a_input, last_sig);
    accum_embedding(&lpcnet->model.gru_a_embed_pred, gru_a_input, pred);
    accum_embedding(&lpcnet->model.gru_a_embed_exc, gru_a_input, last_exc);
#endif
    /*compute_gru3(&gru_a, net->gru_a_state, gru_a_input);*/
    compute_sparse_gru(&lpcnet->model.sparse_gru_a, net->gru_a_state, gru_a_input);
    OPUS_COPY(in_b, net->gru_a_state, GRU_A_STATE_SIZE);
    OPUS_COPY(gru_b_input, gru_b_condition, 3*GRU_B_STATE_SIZE);
    compute_gruB(&lpcnet->model.gru_b, gru_b_input, net->gru_b_state, in_b);
    return sample_mdense(&lpcnet->model.dual_fc, net->gru_b_state, sampling_logit_table, rng);
}

int lpcnet_get_size()
{
    return sizeof(LPCNetState);
}

void lpcnet_reset(LPCNetState *lpcnet)
{
    const char* rng_string="LPCNet";
    OPUS_CLEAR((char*)&lpcnet->LPCNET_RESET_START,
            sizeof(LPCNetState)-
            ((char*)&lpcnet->LPCNET_RESET_START - (char*)lpcnet));
    lpcnet->last_exc = lin2ulaw(0.f);
    kiss99_srand(&lpcnet->rng, (const unsigned char *)rng_string, strlen(rng_string));
}

int lpcnet_init(LPCNetState *lpcnet)
{
    int i;
    int ret;
    for (i=0;i<256;i++) {
        float prob = .025f+.95f*i/255.f;
        lpcnet->sampling_logit_table[i] = -log((1-prob)/prob);
    }
#ifndef USE_WEIGHTS_FILE
    ret = init_lpcnet_model(&lpcnet->model, lpcnet_arrays);
#else
    ret = 0;
#endif
    lpcnet_reset(lpcnet);
    celt_assert(ret == 0);
    return ret;
}

int lpcnet_load_model(LPCNetState *st, const unsigned char *data, int len) {
  WeightArray *list;
  int ret;
  parse_weights(&list, data, len);
  ret = init_lpcnet_model(&st->model, list);
  opus_free(list);
  if (ret == 0) return 0;
  else return -1;
}


LPCNetState *lpcnet_create()
{
    LPCNetState *lpcnet;
    lpcnet = (LPCNetState *)opus_alloc(lpcnet_get_size(), 1);
    OPUS_CLEAR(lpcnet, 1);
    lpcnet_init(lpcnet);
    return lpcnet;
}

void lpcnet_destroy(LPCNetState *lpcnet)
{
    opus_free(lpcnet);
}

void lpcnet_reset_signal(LPCNetState *lpcnet)
{
    lpcnet->deemph_mem = 0;
    lpcnet->last_exc = lin2ulaw(0.f);
    OPUS_CLEAR(lpcnet->last_sig, LPC_ORDER);
    OPUS_CLEAR(lpcnet->nnet.gru_a_state, GRU_A_STATE_SIZE);
    OPUS_CLEAR(lpcnet->nnet.gru_b_state, GRU_B_STATE_SIZE);
}

void lpcnet_synthesize_tail_impl(LPCNetState *lpcnet, opus_int16 *output, int N, int preload)
{
    int i;

    if (lpcnet->frame_count <= FEATURES_DELAY)
    {
        OPUS_CLEAR(output, N);
        return;
    }
    for (i=0;i<N;i++)
    {
        int j;
        float pcm;
        int exc;
        int last_sig_ulaw;
        int pred_ulaw;
        float pred = 0;
        for (j=0;j<LPC_ORDER;j++) pred -= lpcnet->last_sig[j]*lpcnet->lpc[j];
        last_sig_ulaw = lin2ulaw(lpcnet->last_sig[0]);
        pred_ulaw = lin2ulaw(pred);
        exc = run_sample_network(lpcnet, lpcnet->gru_a_condition, lpcnet->gru_b_condition, lpcnet->last_exc, last_sig_ulaw, pred_ulaw, lpcnet->sampling_logit_table, &lpcnet->rng);
        if (i < preload) {
          exc = lin2ulaw(output[i]-PREEMPH*lpcnet->deemph_mem - pred);
          pcm = output[i]-PREEMPH*lpcnet->deemph_mem;
        } else {
          pcm = pred + ulaw2lin(exc);
        }
        OPUS_MOVE(&lpcnet->last_sig[1], &lpcnet->last_sig[0], LPC_ORDER-1);
        lpcnet->last_sig[0] = pcm;
        lpcnet->last_exc = exc;
        pcm += PREEMPH*lpcnet->deemph_mem;
        lpcnet->deemph_mem = pcm;
        if (pcm<-32767) pcm = -32767;
        if (pcm>32767) pcm = 32767;
        if (i >= preload) output[i] = (int)floor(.5 + pcm);
    }
}

void lpcnet_synthesize_impl(LPCNetState *lpcnet, const float *features, opus_int16 *output, int N, int preload)
{
    run_frame_network(lpcnet, lpcnet->gru_a_condition, lpcnet->gru_b_condition, lpcnet->lpc, features);
    lpcnet_synthesize_tail_impl(lpcnet, output, N, preload);
}

void lpcnet_synthesize(LPCNetState *lpcnet, const float *features, opus_int16 *output, int N) {
    lpcnet_synthesize_impl(lpcnet, features, output, N, 0);
}

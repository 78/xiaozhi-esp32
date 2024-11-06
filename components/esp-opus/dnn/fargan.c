/* Copyright (c) 2023 Amazon */
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

#include "fargan.h"
#include "os_support.h"
#include "freq.h"
#include "fargan_data.h"
#include "lpcnet.h"
#include "pitch.h"
#include "nnet.h"
#include "lpcnet_private.h"
#include "cpu_support.h"

#define FARGAN_FEATURES (NB_FEATURES)

static void compute_fargan_cond(FARGANState *st, float *cond, const float *features, int period)
{
  FARGAN *model;
  float dense_in[NB_FEATURES+COND_NET_PEMBED_OUT_SIZE];
  float conv1_in[COND_NET_FCONV1_IN_SIZE];
  float fdense2_in[COND_NET_FCONV1_OUT_SIZE];
  model = &st->model;
  celt_assert(FARGAN_FEATURES+COND_NET_PEMBED_OUT_SIZE == model->cond_net_fdense1.nb_inputs);
  celt_assert(COND_NET_FCONV1_IN_SIZE == model->cond_net_fdense1.nb_outputs);
  celt_assert(COND_NET_FCONV1_OUT_SIZE == model->cond_net_fconv1.nb_outputs);
  OPUS_COPY(&dense_in[NB_FEATURES], &model->cond_net_pembed.float_weights[IMAX(0,IMIN(period-32, 223))*COND_NET_PEMBED_OUT_SIZE], COND_NET_PEMBED_OUT_SIZE);
  OPUS_COPY(dense_in, features, NB_FEATURES);

  compute_generic_dense(&model->cond_net_fdense1, conv1_in, dense_in, ACTIVATION_TANH, st->arch);
  compute_generic_conv1d(&model->cond_net_fconv1, fdense2_in, st->cond_conv1_state, conv1_in, COND_NET_FCONV1_IN_SIZE, ACTIVATION_TANH, st->arch);
  compute_generic_dense(&model->cond_net_fdense2, cond, fdense2_in, ACTIVATION_TANH, st->arch);
}

static void fargan_deemphasis(float *pcm, float *deemph_mem) {
  int i;
  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) {
    pcm[i] += FARGAN_DEEMPHASIS * *deemph_mem;
    *deemph_mem = pcm[i];
  }
}

static void run_fargan_subframe(FARGANState *st, float *pcm, const float *cond, int period)
{
  int i, pos;
  float fwc0_in[SIG_NET_INPUT_SIZE];
  float gru1_in[SIG_NET_FWC0_CONV_OUT_SIZE+2*FARGAN_SUBFRAME_SIZE];
  float gru2_in[SIG_NET_GRU1_OUT_SIZE+2*FARGAN_SUBFRAME_SIZE];
  float gru3_in[SIG_NET_GRU2_OUT_SIZE+2*FARGAN_SUBFRAME_SIZE];
  float pred[FARGAN_SUBFRAME_SIZE+4];
  float prev[FARGAN_SUBFRAME_SIZE];
  float pitch_gate[4];
  float gain;
  float gain_1;
  float skip_cat[10000];
  float skip_out[SIG_NET_SKIP_DENSE_OUT_SIZE];
  FARGAN *model;

  celt_assert(st->cont_initialized);
  model = &st->model;

  compute_generic_dense(&model->sig_net_cond_gain_dense, &gain, cond, ACTIVATION_LINEAR, st->arch);
  gain = exp(gain);
  gain_1 = 1.f/(1e-5f + gain);

  pos = PITCH_MAX_PERIOD-period-2;
  for (i=0;i<FARGAN_SUBFRAME_SIZE+4;i++) {
    pred[i] = MIN32(1.f, MAX32(-1.f, gain_1*st->pitch_buf[IMAX(0, pos)]));
    pos++;
    if (pos == PITCH_MAX_PERIOD) pos -= period;
  }
  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) prev[i] = MAX32(-1.f, MIN16(1.f, gain_1*st->pitch_buf[PITCH_MAX_PERIOD-FARGAN_SUBFRAME_SIZE+i]));

  OPUS_COPY(&fwc0_in[0], &cond[0], FARGAN_COND_SIZE);
  OPUS_COPY(&fwc0_in[FARGAN_COND_SIZE], pred, FARGAN_SUBFRAME_SIZE+4);
  OPUS_COPY(&fwc0_in[FARGAN_COND_SIZE+FARGAN_SUBFRAME_SIZE+4], prev, FARGAN_SUBFRAME_SIZE);

  compute_generic_conv1d(&model->sig_net_fwc0_conv, gru1_in, st->fwc0_mem, fwc0_in, SIG_NET_INPUT_SIZE, ACTIVATION_TANH, st->arch);
  celt_assert(SIG_NET_FWC0_GLU_GATE_OUT_SIZE == model->sig_net_fwc0_glu_gate.nb_outputs);
  compute_glu(&model->sig_net_fwc0_glu_gate, gru1_in, gru1_in, st->arch);

  compute_generic_dense(&model->sig_net_gain_dense_out, pitch_gate, gru1_in, ACTIVATION_SIGMOID, st->arch);

  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) gru1_in[SIG_NET_FWC0_GLU_GATE_OUT_SIZE+i] = pitch_gate[0]*pred[i+2];
  OPUS_COPY(&gru1_in[SIG_NET_FWC0_GLU_GATE_OUT_SIZE+FARGAN_SUBFRAME_SIZE], prev, FARGAN_SUBFRAME_SIZE);
  compute_generic_gru(&model->sig_net_gru1_input, &model->sig_net_gru1_recurrent, st->gru1_state, gru1_in, st->arch);
  compute_glu(&model->sig_net_gru1_glu_gate, gru2_in, st->gru1_state, st->arch);

  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) gru2_in[SIG_NET_GRU1_OUT_SIZE+i] = pitch_gate[1]*pred[i+2];
  OPUS_COPY(&gru2_in[SIG_NET_GRU1_OUT_SIZE+FARGAN_SUBFRAME_SIZE], prev, FARGAN_SUBFRAME_SIZE);
  compute_generic_gru(&model->sig_net_gru2_input, &model->sig_net_gru2_recurrent, st->gru2_state, gru2_in, st->arch);
  compute_glu(&model->sig_net_gru2_glu_gate, gru3_in, st->gru2_state, st->arch);

  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) gru3_in[SIG_NET_GRU2_OUT_SIZE+i] = pitch_gate[2]*pred[i+2];
  OPUS_COPY(&gru3_in[SIG_NET_GRU2_OUT_SIZE+FARGAN_SUBFRAME_SIZE], prev, FARGAN_SUBFRAME_SIZE);
  compute_generic_gru(&model->sig_net_gru3_input, &model->sig_net_gru3_recurrent, st->gru3_state, gru3_in, st->arch);
  compute_glu(&model->sig_net_gru3_glu_gate, &skip_cat[SIG_NET_GRU1_OUT_SIZE+SIG_NET_GRU2_OUT_SIZE], st->gru3_state, st->arch);

  OPUS_COPY(skip_cat, gru2_in, SIG_NET_GRU1_OUT_SIZE);
  OPUS_COPY(&skip_cat[SIG_NET_GRU1_OUT_SIZE], gru3_in, SIG_NET_GRU2_OUT_SIZE);
  OPUS_COPY(&skip_cat[SIG_NET_GRU1_OUT_SIZE+SIG_NET_GRU2_OUT_SIZE+SIG_NET_GRU3_OUT_SIZE], gru1_in, SIG_NET_FWC0_CONV_OUT_SIZE);
  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) skip_cat[SIG_NET_GRU1_OUT_SIZE+SIG_NET_GRU2_OUT_SIZE+SIG_NET_GRU3_OUT_SIZE+SIG_NET_FWC0_CONV_OUT_SIZE+i] = pitch_gate[3]*pred[i+2];
  OPUS_COPY(&skip_cat[SIG_NET_GRU1_OUT_SIZE+SIG_NET_GRU2_OUT_SIZE+SIG_NET_GRU3_OUT_SIZE+SIG_NET_FWC0_CONV_OUT_SIZE+FARGAN_SUBFRAME_SIZE], prev, FARGAN_SUBFRAME_SIZE);

  compute_generic_dense(&model->sig_net_skip_dense, skip_out, skip_cat, ACTIVATION_TANH, st->arch);
  compute_glu(&model->sig_net_skip_glu_gate, skip_out, skip_out, st->arch);

  compute_generic_dense(&model->sig_net_sig_dense_out, pcm, skip_out, ACTIVATION_TANH, st->arch);
  for (i=0;i<FARGAN_SUBFRAME_SIZE;i++) pcm[i] *= gain;

  OPUS_MOVE(st->pitch_buf, &st->pitch_buf[FARGAN_SUBFRAME_SIZE], PITCH_MAX_PERIOD-FARGAN_SUBFRAME_SIZE);
  OPUS_COPY(&st->pitch_buf[PITCH_MAX_PERIOD-FARGAN_SUBFRAME_SIZE], pcm, FARGAN_SUBFRAME_SIZE);
  fargan_deemphasis(pcm, &st->deemph_mem);
}

void fargan_cont(FARGANState *st, const float *pcm0, const float *features0)
{
  int i;
  float cond[COND_NET_FDENSE2_OUT_SIZE];
  float x0[FARGAN_CONT_SAMPLES];
  float dummy[FARGAN_SUBFRAME_SIZE];
  int period=0;

  /* Pre-load features. */
  for (i=0;i<5;i++) {
    const float *features = &features0[i*NB_FEATURES];
    st->last_period = period;
    period = (int)floor(.5+256./pow(2.f,((1./60.)*((features[NB_BANDS]+1.5)*60))));
    compute_fargan_cond(st, cond, features, period);
  }

  x0[0] = 0;
  for (i=1;i<FARGAN_CONT_SAMPLES;i++) {
    x0[i] = pcm0[i] - FARGAN_DEEMPHASIS*pcm0[i-1];
  }

  OPUS_COPY(&st->pitch_buf[PITCH_MAX_PERIOD-FARGAN_FRAME_SIZE], x0, FARGAN_FRAME_SIZE);
  st->cont_initialized = 1;

  for (i=0;i<FARGAN_NB_SUBFRAMES;i++) {
    run_fargan_subframe(st, dummy, &cond[i*FARGAN_COND_SIZE], st->last_period);
    OPUS_COPY(&st->pitch_buf[PITCH_MAX_PERIOD-FARGAN_SUBFRAME_SIZE], &x0[FARGAN_FRAME_SIZE+i*FARGAN_SUBFRAME_SIZE], FARGAN_SUBFRAME_SIZE);
  }
  st->deemph_mem = pcm0[FARGAN_CONT_SAMPLES-1];
}


void fargan_init(FARGANState *st)
{
  int ret;
  OPUS_CLEAR(st, 1);
  st->arch = opus_select_arch();
#ifndef USE_WEIGHTS_FILE
  ret = init_fargan(&st->model, fargan_arrays);
#else
  ret = 0;
#endif
  celt_assert(ret == 0);
}

int fargan_load_model(FARGANState *st, const void *data, int len) {
  WeightArray *list;
  int ret;
  parse_weights(&list, data, len);
  ret = init_fargan(&st->model, list);
  opus_free(list);
  if (ret == 0) return 0;
  else return -1;
}

static void fargan_synthesize_impl(FARGANState *st, float *pcm, const float *features)
{
  int subframe;
  float cond[COND_NET_FDENSE2_OUT_SIZE];
  int period;
  celt_assert(st->cont_initialized);

  period = (int)floor(.5+256./pow(2.f,((1./60.)*((features[NB_BANDS]+1.5)*60))));
  compute_fargan_cond(st, cond, features, period);
  for (subframe=0;subframe<FARGAN_NB_SUBFRAMES;subframe++) {
    float *sub_cond;
    sub_cond = &cond[subframe*FARGAN_COND_SIZE];
    run_fargan_subframe(st, &pcm[subframe*FARGAN_SUBFRAME_SIZE], sub_cond, st->last_period);
  }
  st->last_period = period;
}

void fargan_synthesize(FARGANState *st, float *pcm, const float *features)
{
  fargan_synthesize_impl(st, pcm, features);
}

void fargan_synthesize_int(FARGANState *st, opus_int16 *pcm, const float *features)
{
  int i;
  float fpcm[FARGAN_FRAME_SIZE];
  fargan_synthesize(st, fpcm, features);
  for (i=0;i<LPCNET_FRAME_SIZE;i++) pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
}

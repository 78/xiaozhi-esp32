/* Copyright (c) 2021 Amazon */
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

#include "lpcnet_private.h"
#include "lpcnet.h"
#include "plc_data.h"
#include "os_support.h"
#include "common.h"
#include "cpu_support.h"

#ifndef M_PI
#define M_PI 3.141592653
#endif

/* Comment this out to have LPCNet update its state on every good packet (slow). */
#define PLC_SKIP_UPDATES

void lpcnet_plc_reset(LPCNetPLCState *st) {
  OPUS_CLEAR((char*)&st->LPCNET_PLC_RESET_START,
          sizeof(LPCNetPLCState)-
          ((char*)&st->LPCNET_PLC_RESET_START - (char*)st));
  lpcnet_encoder_init(&st->enc);
  OPUS_CLEAR(st->pcm, PLC_BUF_SIZE);
  st->blend = 0;
  st->loss_count = 0;
  st->analysis_gap = 1;
  st->analysis_pos = PLC_BUF_SIZE;
  st->predict_pos = PLC_BUF_SIZE;
}

int lpcnet_plc_init(LPCNetPLCState *st) {
  int ret;
  st->arch = opus_select_arch();
  fargan_init(&st->fargan);
  lpcnet_encoder_init(&st->enc);
  st->loaded = 0;
#ifndef USE_WEIGHTS_FILE
  ret = init_plcmodel(&st->model, plcmodel_arrays);
  if (ret == 0) st->loaded = 1;
#else
  ret = 0;
#endif
  celt_assert(ret == 0);
  lpcnet_plc_reset(st);
  return ret;
}

int lpcnet_plc_load_model(LPCNetPLCState *st, const void *data, int len) {
  WeightArray *list;
  int ret;
  parse_weights(&list, data, len);
  ret = init_plcmodel(&st->model, list);
  opus_free(list);
  if (ret == 0) {
    ret = lpcnet_encoder_load_model(&st->enc, data, len);
  }
  if (ret == 0) {
    ret = fargan_load_model(&st->fargan, data, len);
  }
  if (ret == 0) st->loaded = 1;
  return ret;
}

void lpcnet_plc_fec_add(LPCNetPLCState *st, const float *features) {
  if (features == NULL) {
    st->fec_skip++;
    return;
  }
  if (st->fec_fill_pos == PLC_MAX_FEC) {
    OPUS_MOVE(&st->fec[0][0], &st->fec[st->fec_read_pos][0], (st->fec_fill_pos-st->fec_read_pos)*NB_FEATURES);
    st->fec_fill_pos = st->fec_fill_pos-st->fec_read_pos;
    st->fec_read_pos -= st->fec_read_pos;
  }
  OPUS_COPY(&st->fec[st->fec_fill_pos][0], features, NB_FEATURES);
  st->fec_fill_pos++;
}

void lpcnet_plc_fec_clear(LPCNetPLCState *st) {
  st->fec_read_pos = st->fec_fill_pos = st->fec_skip = 0;
}


static void compute_plc_pred(LPCNetPLCState *st, float *out, const float *in) {
  float tmp[PLC_DENSE_IN_OUT_SIZE];
  PLCModel *model = &st->model;
  PLCNetState *net = &st->plc_net;
  celt_assert(st->loaded);
  compute_generic_dense(&model->plc_dense_in, tmp, in, ACTIVATION_TANH, st->arch);
  compute_generic_gru(&model->plc_gru1_input, &model->plc_gru1_recurrent, net->gru1_state, tmp, st->arch);
  compute_generic_gru(&model->plc_gru2_input, &model->plc_gru2_recurrent, net->gru2_state, net->gru1_state, st->arch);
  compute_generic_dense(&model->plc_dense_out, out, net->gru2_state, ACTIVATION_LINEAR, st->arch);
}

static int get_fec_or_pred(LPCNetPLCState *st, float *out) {
  if (st->fec_read_pos != st->fec_fill_pos && st->fec_skip==0) {
    float plc_features[2*NB_BANDS+NB_FEATURES+1] = {0};
    float discard[NB_FEATURES];
    OPUS_COPY(out, &st->fec[st->fec_read_pos][0], NB_FEATURES);
    st->fec_read_pos++;
    /* Update PLC state using FEC, so without Burg features. */
    OPUS_COPY(&plc_features[2*NB_BANDS], out, NB_FEATURES);
    plc_features[2*NB_BANDS+NB_FEATURES] = -1;
    compute_plc_pred(st, discard, plc_features);
    return 1;
  } else {
    float zeros[2*NB_BANDS+NB_FEATURES+1] = {0};
    compute_plc_pred(st, out, zeros);
    if (st->fec_skip > 0) st->fec_skip--;
    return 0;
  }
}

static void queue_features(LPCNetPLCState *st, const float *features) {
  OPUS_MOVE(&st->cont_features[0], &st->cont_features[NB_FEATURES], (CONT_VECTORS-1)*NB_FEATURES);
  OPUS_COPY(&st->cont_features[(CONT_VECTORS-1)*NB_FEATURES], features, NB_FEATURES);
}

/* In this causal version of the code, the DNN model implemented by compute_plc_pred()
   needs to generate two feature vectors to conceal the first lost packet.*/

int lpcnet_plc_update(LPCNetPLCState *st, opus_int16 *pcm) {
  int i;
  if (st->analysis_pos - FRAME_SIZE >= 0) st->analysis_pos -= FRAME_SIZE;
  else st->analysis_gap = 1;
  if (st->predict_pos - FRAME_SIZE >= 0) st->predict_pos -= FRAME_SIZE;
  OPUS_MOVE(st->pcm, &st->pcm[FRAME_SIZE], PLC_BUF_SIZE-FRAME_SIZE);
  for (i=0;i<FRAME_SIZE;i++) st->pcm[PLC_BUF_SIZE-FRAME_SIZE+i] = (1.f/32768.f)*pcm[i];
  st->loss_count = 0;
  st->blend = 0;
  return 0;
}

static const float att_table[10] = {0, 0,  -.2, -.2,  -.4, -.4,  -.8, -.8, -1.6, -1.6};
int lpcnet_plc_conceal(LPCNetPLCState *st, opus_int16 *pcm) {
  int i;
  celt_assert(st->loaded);
  if (st->blend == 0) {
    int count = 0;
    st->plc_net = st->plc_bak[0];
    while (st->analysis_pos + FRAME_SIZE <= PLC_BUF_SIZE) {
      float x[FRAME_SIZE];
      float plc_features[2*NB_BANDS+NB_FEATURES+1];
      celt_assert(st->analysis_pos >= 0);
      for (i=0;i<FRAME_SIZE;i++) x[i] = 32768.f*st->pcm[st->analysis_pos+i];
      burg_cepstral_analysis(plc_features, x);
      lpcnet_compute_single_frame_features_float(&st->enc, x, st->features, st->arch);
      if ((!st->analysis_gap || count>0) && st->analysis_pos >= st->predict_pos) {
        queue_features(st, st->features);
        OPUS_COPY(&plc_features[2*NB_BANDS], st->features, NB_FEATURES);
        plc_features[2*NB_BANDS+NB_FEATURES] = 1;
        st->plc_bak[0] = st->plc_bak[1];
        st->plc_bak[1] = st->plc_net;
        compute_plc_pred(st, st->features, plc_features);
      }
      st->analysis_pos += FRAME_SIZE;
      count++;
    }
    st->plc_bak[0] = st->plc_bak[1];
    st->plc_bak[1] = st->plc_net;
    get_fec_or_pred(st, st->features);
    queue_features(st, st->features);
    st->plc_bak[0] = st->plc_bak[1];
    st->plc_bak[1] = st->plc_net;
    get_fec_or_pred(st, st->features);
    queue_features(st, st->features);
    fargan_cont(&st->fargan, &st->pcm[PLC_BUF_SIZE-FARGAN_CONT_SAMPLES], st->cont_features);
    st->analysis_gap = 0;
  }
  st->plc_bak[0] = st->plc_bak[1];
  st->plc_bak[1] = st->plc_net;
  if (get_fec_or_pred(st, st->features)) st->loss_count = 0;
  else st->loss_count++;
  if (st->loss_count >= 10) st->features[0] = MAX16(-10, st->features[0]+att_table[9] - 2*(st->loss_count-9));
  else st->features[0] = MAX16(-10, st->features[0]+att_table[st->loss_count]);
  fargan_synthesize_int(&st->fargan, pcm, &st->features[0]);
  queue_features(st, st->features);
  if (st->analysis_pos - FRAME_SIZE >= 0) st->analysis_pos -= FRAME_SIZE;
  else st->analysis_gap = 1;
  st->predict_pos = PLC_BUF_SIZE;
  OPUS_MOVE(st->pcm, &st->pcm[FRAME_SIZE], PLC_BUF_SIZE-FRAME_SIZE);
  for (i=0;i<FRAME_SIZE;i++) st->pcm[PLC_BUF_SIZE-FRAME_SIZE+i] = (1.f/32768.f)*pcm[i];
  st->blend = 1;
  return 0;
}

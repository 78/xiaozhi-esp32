/* Copyright (c) 2017-2019 Mozilla */
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kiss_fft.h"
#include "common.h"
#include <math.h>
#include "freq.h"
#include "pitch.h"
#include "arch.h"
#include <assert.h>
#include "lpcnet_private.h"
#include "lpcnet.h"
#include "os_support.h"
#include "_kiss_fft_guts.h"
#include "celt_lpc.h"
#include "mathops.h"


int lpcnet_encoder_get_size(void) {
  return sizeof(LPCNetEncState);
}

int lpcnet_encoder_init(LPCNetEncState *st) {
  memset(st, 0, sizeof(*st));
  pitchdnn_init(&st->pitchdnn);
  return 0;
}

int lpcnet_encoder_load_model(LPCNetEncState *st, const void *data, int len) {
  return pitchdnn_load_model(&st->pitchdnn, data, len);
}

LPCNetEncState *lpcnet_encoder_create(void) {
  LPCNetEncState *st;
  st = opus_alloc(lpcnet_encoder_get_size());
  lpcnet_encoder_init(st);
  return st;
}

void lpcnet_encoder_destroy(LPCNetEncState *st) {
  opus_free(st);
}

static void frame_analysis(LPCNetEncState *st, kiss_fft_cpx *X, float *Ex, const float *in) {
  float x[WINDOW_SIZE];
  OPUS_COPY(x, st->analysis_mem, OVERLAP_SIZE);
  OPUS_COPY(&x[OVERLAP_SIZE], in, FRAME_SIZE);
  OPUS_COPY(st->analysis_mem, &in[FRAME_SIZE-OVERLAP_SIZE], OVERLAP_SIZE);
  apply_window(x);
  forward_transform(X, x);
  lpcn_compute_band_energy(Ex, X);
}

static void biquad(float *y, float mem[2], const float *x, const float *b, const float *a, int N) {
  int i;
  float mem0, mem1;
  mem0 = mem[0];
  mem1 = mem[1];
  for (i=0;i<N;i++) {
    float xi, yi, mem00;
    xi = x[i];
    yi = x[i] + mem0;
    mem00 = mem0;
    /* Original code:
    mem0 = mem1 + (b[0]*xi - a[0]*yi);
    mem1 = (b[1]*xi - a[1]*yi);
    Modified to reduce dependency chains: (the +1e-30f forces the ordering and has no effect on the output)
    */
    mem0 = (b[0]-a[0])*xi + mem1 - a[0]*mem0;
    mem1 = (b[1]-a[1])*xi + 1e-30f - a[1]*mem00;
    y[i] = yi;
  }
  mem[0] = mem0;
  mem[1] = mem1;
}

#define celt_log10(x) (0.3010299957f*celt_log2(x))

void compute_frame_features(LPCNetEncState *st, const float *in, int arch) {
  float aligned_in[FRAME_SIZE];
  int i;
  float Ly[NB_BANDS];
  float follow, logMax;
  kiss_fft_cpx X[FREQ_SIZE];
  float Ex[NB_BANDS];
  float xcorr[PITCH_MAX_PERIOD];
  float ener0;
  float ener;
  float x[FRAME_SIZE+LPC_ORDER];
  float frame_corr;
  float xy, xx, yy;
  int pitch;
  float ener_norm[PITCH_MAX_PERIOD - PITCH_MIN_PERIOD];
  /* [b,a]=ellip(2, 2, 20, 1200/8000); */
  static const float lp_b[2] = {-0.84946f, 1.f};
  static const float lp_a[2] = {-1.54220f, 0.70781f};
  OPUS_COPY(aligned_in, &st->analysis_mem[OVERLAP_SIZE-TRAINING_OFFSET], TRAINING_OFFSET);
  frame_analysis(st, X, Ex, in);
  st->if_features[0] = MAX16(-1.f, MIN16(1.f, (1.f/64)*(10.f*celt_log10(1e-15f + X[0].r*X[0].r)-6.f)));
  for (i=1;i<PITCH_IF_MAX_FREQ;i++) {
    kiss_fft_cpx prod;
    float norm_1;
    C_MULC(prod, X[i], st->prev_if[i]);
    norm_1 = 1.f/sqrt(1e-15f + prod.r*prod.r + prod.i*prod.i);
    C_MULBYSCALAR(prod, norm_1);
    st->if_features[3*i-2] = prod.r;
    st->if_features[3*i-1] = prod.i;
    st->if_features[3*i] = MAX16(-1.f, MIN16(1.f, (1.f/64)*(10.f*celt_log10(1e-15f + X[i].r*X[i].r + X[i].i*X[i].i)-6.f)));
  }
  OPUS_COPY(st->prev_if, X, PITCH_IF_MAX_FREQ);
  /*for (i=0;i<88;i++) printf("%f ", st->if_features[i]);printf("\n");*/
  logMax = -2;
  follow = -2;
  for (i=0;i<NB_BANDS;i++) {
    Ly[i] = celt_log10(1e-2f+Ex[i]);
    Ly[i] = MAX16(logMax-8, MAX16(follow-2.5f, Ly[i]));
    logMax = MAX16(logMax, Ly[i]);
    follow = MAX16(follow-2.5f, Ly[i]);
  }
  dct(st->features, Ly);
  st->features[0] -= 4;
  lpc_from_cepstrum(st->lpc, st->features);
  for (i=0;i<LPC_ORDER;i++) st->features[NB_BANDS+2+i] = st->lpc[i];
  OPUS_MOVE(st->exc_buf, &st->exc_buf[FRAME_SIZE], PITCH_MAX_PERIOD);
  OPUS_MOVE(st->lp_buf, &st->lp_buf[FRAME_SIZE], PITCH_MAX_PERIOD);
  OPUS_COPY(&aligned_in[TRAINING_OFFSET], in, FRAME_SIZE-TRAINING_OFFSET);
  OPUS_COPY(&x[0], st->pitch_mem, LPC_ORDER);
  OPUS_COPY(&x[LPC_ORDER], aligned_in, FRAME_SIZE);
  OPUS_COPY(st->pitch_mem, &aligned_in[FRAME_SIZE-LPC_ORDER], LPC_ORDER);
  celt_fir(&x[LPC_ORDER], st->lpc, &st->lp_buf[PITCH_MAX_PERIOD], FRAME_SIZE, LPC_ORDER, arch);
  for (i=0;i<FRAME_SIZE;i++) {
    st->exc_buf[PITCH_MAX_PERIOD+i] = st->lp_buf[PITCH_MAX_PERIOD+i] + .7f*st->pitch_filt;
    st->pitch_filt = st->lp_buf[PITCH_MAX_PERIOD+i];
    /*printf("%f\n", st->exc_buf[PITCH_MAX_PERIOD+i]);*/
  }
  biquad(&st->lp_buf[PITCH_MAX_PERIOD], st->lp_mem, &st->lp_buf[PITCH_MAX_PERIOD], lp_b, lp_a, FRAME_SIZE);
  {
    double ener1;
    float *buf = st->exc_buf;
    celt_pitch_xcorr(&buf[PITCH_MAX_PERIOD], buf, xcorr, FRAME_SIZE, PITCH_MAX_PERIOD-PITCH_MIN_PERIOD, arch);
    ener0 = celt_inner_prod(&buf[PITCH_MAX_PERIOD], &buf[PITCH_MAX_PERIOD], FRAME_SIZE, arch);
    ener1 = celt_inner_prod(&buf[0], &buf[0], FRAME_SIZE, arch);
    /*printf("%f\n", st->frame_weight[sub]);*/
    for (i=0;i<PITCH_MAX_PERIOD-PITCH_MIN_PERIOD;i++) {
      ener = 1 + ener0 + ener1;
      st->xcorr_features[i] = 2*xcorr[i];
      ener_norm[i] = ener;
      ener1 += buf[i+FRAME_SIZE]*(double)buf[i+FRAME_SIZE] - buf[i]*(double)buf[i];
      /*printf("%f ", st->xcorr_features[i]);*/
    }
    /* Split in a separate loop so the compiler can vectorize it */
    for (i=0;i<PITCH_MAX_PERIOD-PITCH_MIN_PERIOD;i++) {
      st->xcorr_features[i] /= ener_norm[i];
    }
    /*printf("\n");*/
  }
  st->dnn_pitch = compute_pitchdnn(&st->pitchdnn, st->if_features, st->xcorr_features, arch);
  pitch = (int)floor(.5+256./pow(2.f,((1./60.)*((st->dnn_pitch+1.5)*60))));
  xx = celt_inner_prod(&st->lp_buf[PITCH_MAX_PERIOD], &st->lp_buf[PITCH_MAX_PERIOD], FRAME_SIZE, arch);
  yy = celt_inner_prod(&st->lp_buf[PITCH_MAX_PERIOD-pitch], &st->lp_buf[PITCH_MAX_PERIOD-pitch], FRAME_SIZE, arch);
  xy = celt_inner_prod(&st->lp_buf[PITCH_MAX_PERIOD], &st->lp_buf[PITCH_MAX_PERIOD-pitch], FRAME_SIZE, arch);
  /*printf("%f %f\n", frame_corr, xy/sqrt(1e-15+xx*yy));*/
  frame_corr = xy/sqrt(1+xx*yy);
  frame_corr = log(1.f+exp(5.f*frame_corr))/log(1+exp(5.f));
  st->features[NB_BANDS] = st->dnn_pitch;
  st->features[NB_BANDS + 1] = frame_corr-.5f;
}

void preemphasis(float *y, float *mem, const float *x, float coef, int N) {
  int i;
  for (i=0;i<N;i++) {
    float yi;
    yi = x[i] + *mem;
    *mem = -coef*x[i];
    y[i] = yi;
  }
}

static int lpcnet_compute_single_frame_features_impl(LPCNetEncState *st, float *x, float features[NB_TOTAL_FEATURES], int arch) {
  preemphasis(x, &st->mem_preemph, x, PREEMPHASIS, FRAME_SIZE);
  compute_frame_features(st, x, arch);
  OPUS_COPY(features, &st->features[0], NB_TOTAL_FEATURES);
  return 0;
}

int lpcnet_compute_single_frame_features(LPCNetEncState *st, const opus_int16 *pcm, float features[NB_TOTAL_FEATURES], int arch) {
  int i;
  float x[FRAME_SIZE];
  for (i=0;i<FRAME_SIZE;i++) x[i] = pcm[i];
  lpcnet_compute_single_frame_features_impl(st, x, features, arch);
  return 0;
}

int lpcnet_compute_single_frame_features_float(LPCNetEncState *st, const float *pcm, float features[NB_TOTAL_FEATURES], int arch) {
  int i;
  float x[FRAME_SIZE];
  for (i=0;i<FRAME_SIZE;i++) x[i] = pcm[i];
  lpcnet_compute_single_frame_features_impl(st, x, features, arch);
  return 0;
}

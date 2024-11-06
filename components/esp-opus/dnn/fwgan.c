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

#include "fwgan.h"
#include "os_support.h"
#include "freq.h"
#include "fwgan_data.h"
#include "lpcnet.h"
#include "pitch.h"
#include "nnet.h"
#include "lpcnet_private.h"

#define FEAT_IN_SIZE (BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE/4 + FWGAN_FRAME_SIZE/2)

#define FWGAN_FEATURES (NB_FEATURES-1)

static void pitch_embeddings(float *pembed, float *phase, double w0) {
  int i;
  float wreal, wimag;
#if 1
  /* This Taylor expansion should be good enough since w0 is always small. */
  float w2 = w0*w0;
  wreal = 1 - .5*w2*(1.f - 0.083333333f*w2);
  wimag = w0*(1 - 0.166666667f*w2*(1.f - 0.05f*w2));
#else
  wreal = cos(w0);
  wimag = sin(w0);
#endif
  /* Speed-up phase reference by making phase a unit-norm complex value and rotating it
     by exp(-i*w0) each sample.  */
  for (i=0;i<SUBFRAME_SIZE;i++) {
    float tmp;
    tmp = phase[0]*wreal - phase[1]*wimag;
    phase[1] = phase[0]*wimag + phase[1]*wreal;
    phase[0] = tmp;
    pembed[i] = phase[1];
    pembed[SUBFRAME_SIZE+i] = phase[0];
  }
  /* Renormalize once per sub-frame, though we could probably do it even less frequently. */
  {
    float r = 1.f/sqrt(phase[0]*phase[0] + phase[1]*phase[1]);
    phase[0] *= r;
    phase[1] *= r;
  }
}

static void compute_wlpc(float lpc[LPC_ORDER], const float *features) {
  float lpc_weight;
  int i;
  lpc_from_cepstrum(lpc, features);
  lpc_weight = 1.f;
  for (i=0;i<LPC_ORDER;i++) {
    lpc_weight *= FWGAN_GAMMA;
    lpc[i] *= lpc_weight;
  }
}

static void run_fwgan_upsampler(FWGANState *st, float *cond, const float *features)
{
  FWGAN *model;
  model = &st->model;
  celt_assert(FWGAN_FEATURES == model->bfcc_with_corr_upsampler_fc.nb_inputs);
  celt_assert(BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE == model->bfcc_with_corr_upsampler_fc.nb_outputs);
  compute_generic_dense(&model->bfcc_with_corr_upsampler_fc, cond, features, ACTIVATION_TANH);
}

static void fwgan_synthesize_impl(FWGANState *st, float *pcm, const float *lpc, const float *features);
void fwgan_cont(FWGANState *st, const float *pcm0, const float *features0)
{
  int i;
  float norm2, norm_1;
  float wpcm0[CONT_PCM_INPUTS];
  float cont_inputs[CONT_PCM_INPUTS+1];
  float tmp1[MAX_CONT_SIZE];
  float tmp2[MAX_CONT_SIZE];
  float lpc[LPC_ORDER];
  float new_pcm[FWGAN_FRAME_SIZE];
  FWGAN *model;
  st->embed_phase[0] = 1;
  model = &st->model;
  compute_wlpc(lpc, features0);
  /* Deemphasis memory is just the last continuation sample. */
  st->deemph_mem = pcm0[CONT_PCM_INPUTS-1];

  /* Apply analysis filter, considering that the preemphasis and deemphasis filter
     cancel each other in this case since the LPC filter is constant across that boundary.
     */
  for (i=LPC_ORDER;i<CONT_PCM_INPUTS;i++) {
    int j;
    wpcm0[i] = pcm0[i];
    for (j=0;j<LPC_ORDER;j++) wpcm0[i] += lpc[j]*pcm0[i-j-1];
  }
  /* FIXME: Make this less stupid. */
  for (i=0;i<LPC_ORDER;i++) wpcm0[i] = wpcm0[LPC_ORDER];

  /* The memory of the pre-empahsis is the last sample of the weighted signal
     (ignoring preemphasis+deemphasis combination). */
  st->preemph_mem = wpcm0[CONT_PCM_INPUTS-1];
  /* The memory of the synthesis filter is the pre-emphasized continuation. */
  for (i=0;i<LPC_ORDER;i++) st->syn_mem[i] = pcm0[CONT_PCM_INPUTS-1-i] - FWGAN_DEEMPHASIS*pcm0[CONT_PCM_INPUTS-2-i];

  norm2 = celt_inner_prod(wpcm0, wpcm0, CONT_PCM_INPUTS, st->arch);
  norm_1 = 1.f/sqrt(1e-8f + norm2);
  for (i=0;i<CONT_PCM_INPUTS;i++) cont_inputs[i+1] = norm_1*wpcm0[i];
  cont_inputs[0] = log(sqrt(norm2) + 1e-7f);

  /* Continuation network */
  compute_generic_dense(&model->cont_net_0, tmp1, cont_inputs, ACTIVATION_TANH);
  compute_generic_dense(&model->cont_net_2, tmp2, tmp1, ACTIVATION_TANH);
  compute_generic_dense(&model->cont_net_4, tmp1, tmp2, ACTIVATION_TANH);
  compute_generic_dense(&model->cont_net_6, tmp2, tmp1, ACTIVATION_TANH);
  compute_generic_dense(&model->cont_net_8, tmp1, tmp2, ACTIVATION_TANH);
  celt_assert(CONT_NET_10_OUT_SIZE == model->cont_net_10.nb_outputs);
  compute_generic_dense(&model->cont_net_10, st->cont, tmp1, ACTIVATION_TANH);

  /* Computing continuation for each layer. */
  celt_assert(RNN_GRU_STATE_SIZE == model->rnn_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->rnn_cont_fc_0, st->rnn_state, st->cont, ACTIVATION_TANH);

  celt_assert(FWC1_STATE_SIZE == model->fwc1_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc1_cont_fc_0, st->fwc1_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC2_STATE_SIZE == model->fwc2_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc2_cont_fc_0, st->fwc2_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC3_STATE_SIZE == model->fwc3_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc3_cont_fc_0, st->fwc3_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC4_STATE_SIZE == model->fwc4_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc4_cont_fc_0, st->fwc4_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC5_STATE_SIZE == model->fwc5_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc5_cont_fc_0, st->fwc5_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC6_STATE_SIZE == model->fwc6_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc6_cont_fc_0, st->fwc6_state, st->cont, ACTIVATION_TANH);
  celt_assert(FWC7_STATE_SIZE == model->fwc7_cont_fc_0.nb_outputs);
  compute_generic_dense(&model->fwc7_cont_fc_0, st->fwc7_state, st->cont, ACTIVATION_TANH);

  st->cont_initialized = 1;
  /* Process the first frame, discard the first subframe, and keep the rest for the first
     synthesis call. */
  fwgan_synthesize_impl(st, new_pcm, lpc, features0);
  OPUS_COPY(st->pcm_buf, &new_pcm[SUBFRAME_SIZE], FWGAN_FRAME_SIZE-SUBFRAME_SIZE);
}

static void apply_gain(float *pcm, float c0, float *last_gain) {
  int i;
  float gain = pow(10.f, (0.5f*c0/sqrt(18.f)));
  for (i=0;i<SUBFRAME_SIZE;i++) pcm[i] *= *last_gain;
  *last_gain = gain;
}

static void fwgan_lpc_syn(float *pcm, float *mem, const float *lpc, float last_lpc[LPC_ORDER]) {
  int i;
  for (i=0;i<SUBFRAME_SIZE;i++) {
    int j;
    for (j=0;j<LPC_ORDER;j++) pcm[i] -= mem[j]*last_lpc[j];
    OPUS_MOVE(&mem[1], &mem[0], LPC_ORDER-1);
    mem[0] = pcm[i];
  }
  OPUS_COPY(last_lpc, lpc, LPC_ORDER);
}

static void fwgan_preemphasis(float *pcm, float *preemph_mem) {
  int i;
  for (i=0;i<SUBFRAME_SIZE;i++) {
    float tmp = pcm[i];
    pcm[i] -= FWGAN_DEEMPHASIS * *preemph_mem;
    *preemph_mem = tmp;
  }
}

static void fwgan_deemphasis(float *pcm, float *deemph_mem) {
  int i;
  for (i=0;i<SUBFRAME_SIZE;i++) {
    pcm[i] += FWGAN_DEEMPHASIS * *deemph_mem;
    *deemph_mem = pcm[i];
  }
}

static void run_fwgan_subframe(FWGANState *st, float *pcm, const float *cond, double w0, const float *lpc, float c0)
{
  float tmp1[FWC1_FC_0_OUT_SIZE];
  float tmp2[IMAX(RNN_GRU_STATE_SIZE, FWC2_FC_0_OUT_SIZE)];
  float feat_in[FEAT_IN_SIZE];
  float rnn_in[FEAT_IN_CONV1_CONV_OUT_SIZE];
  float pembed[FWGAN_FRAME_SIZE/2];
  FWGAN *model;
  model = &st->model;

  pitch_embeddings(pembed, st->embed_phase, w0);
  /* Interleave bfcc_cond and pembed for each subframe in feat_in. */
  OPUS_COPY(&feat_in[BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE/4], &cond[0], BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE/4);
  OPUS_COPY(&feat_in[0], &pembed[0], FWGAN_FRAME_SIZE/2);

  compute_generic_conv1d(&model->feat_in_conv1_conv, rnn_in, st->cont_conv1_mem, feat_in, FEAT_IN_CONV1_CONV_IN_SIZE, ACTIVATION_LINEAR);
  celt_assert(FEAT_IN_NL1_GATE_OUT_SIZE == model->feat_in_nl1_gate.nb_outputs);
  compute_gated_activation(&model->feat_in_nl1_gate, rnn_in, rnn_in, ACTIVATION_TANH);

  if (st->cont_initialized == 1) {
    /* On the very first subframe we stop here. We only want to run the feat_in layer since the
       others are initialized via the continuation network. */
    OPUS_CLEAR(pcm, SUBFRAME_SIZE);
    st->cont_initialized = 2;
    apply_gain(pcm, c0, &st->last_gain);
    OPUS_COPY(st->last_lpc, lpc, LPC_ORDER);
    return;
  }

  compute_generic_gru(&model->rnn_gru_input, &model->rnn_gru_recurrent, st->rnn_state, rnn_in);
  celt_assert(IMAX(RNN_GRU_STATE_SIZE, FWC2_FC_0_OUT_SIZE) >= model->rnn_nl_gate.nb_outputs);
  compute_gated_activation(&model->rnn_nl_gate, tmp2, st->rnn_state, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc1_fc_0, tmp1, st->fwc1_state, tmp2, RNN_GRU_STATE_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc1_fc_1_gate, tmp1, tmp1, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc2_fc_0, tmp2, st->fwc2_state, tmp1, FWC1_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc2_fc_1_gate, tmp2, tmp2, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc3_fc_0, tmp1, st->fwc3_state, tmp2, FWC2_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc3_fc_1_gate, tmp1, tmp1, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc4_fc_0, tmp2, st->fwc4_state, tmp1, FWC3_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc4_fc_1_gate, tmp2, tmp2, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc5_fc_0, tmp1, st->fwc5_state, tmp2, FWC4_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc5_fc_1_gate, tmp1, tmp1, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc6_fc_0, tmp2, st->fwc6_state, tmp1, FWC5_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc6_fc_1_gate, tmp2, tmp2, ACTIVATION_TANH);

  compute_generic_conv1d(&model->fwc7_fc_0, tmp1, st->fwc7_state, tmp2, FWC6_FC_0_OUT_SIZE, ACTIVATION_LINEAR);
  compute_gated_activation(&model->fwc7_fc_1_gate, pcm, tmp1, ACTIVATION_TANH);

  apply_gain(pcm, c0, &st->last_gain);
  fwgan_preemphasis(pcm, &st->preemph_mem);
  fwgan_lpc_syn(pcm, st->syn_mem, lpc, st->last_lpc);
  fwgan_deemphasis(pcm, &st->deemph_mem);
}

void fwgan_init(FWGANState *st)
{
  int ret;
  OPUS_CLEAR(st, 1);
  ret = init_fwgan(&st->model, fwgan_arrays);
  celt_assert(ret == 0);
  /* FIXME: perform arch detection. */
}

int fwgan_load_model(FWGANState *st, const unsigned char *data, int len) {
  WeightArray *list;
  int ret;
  parse_weights(&list, data, len);
  ret = init_fwgan(&st->model, list);
  opus_free(list);
  if (ret == 0) return 0;
  else return -1;
}

static void fwgan_synthesize_impl(FWGANState *st, float *pcm, const float *lpc, const float *features)
{
  int subframe;
  float cond[BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE];
  double w0;
  int period;
  float fwgan_features[NB_FEATURES-1];
  celt_assert(st->cont_initialized);
  OPUS_COPY(fwgan_features, features, NB_FEATURES-2);
  fwgan_features[NB_FEATURES-2] = features[NB_FEATURES-1]+.5;

  period = (int)floor(.1 + 50*features[NB_BANDS]+100);
  w0 = 2*M_PI/period;
  run_fwgan_upsampler(st, cond, fwgan_features);
  for (subframe=0;subframe<NB_SUBFRAMES;subframe++) {
    float *sub_cond;
    sub_cond = &cond[subframe*BFCC_WITH_CORR_UPSAMPLER_FC_OUT_SIZE/4];
    run_fwgan_subframe(st, &pcm[subframe*SUBFRAME_SIZE], sub_cond, w0, lpc, features[0]);
  }
}

void fwgan_synthesize(FWGANState *st, float *pcm, const float *features)
{
  float lpc[LPC_ORDER];
  float new_pcm[FWGAN_FRAME_SIZE];
  compute_wlpc(lpc, features);
  fwgan_synthesize_impl(st, new_pcm, lpc, features);
  /* Handle buffering. */
  OPUS_COPY(pcm, st->pcm_buf, FWGAN_FRAME_SIZE-SUBFRAME_SIZE);
  OPUS_COPY(&pcm[FWGAN_FRAME_SIZE-SUBFRAME_SIZE], new_pcm, SUBFRAME_SIZE);
  OPUS_COPY(st->pcm_buf, &new_pcm[SUBFRAME_SIZE], FWGAN_FRAME_SIZE-SUBFRAME_SIZE);
}

void fwgan_synthesize_int(FWGANState *st, opus_int16 *pcm, const float *features)
{
  int i;
  float fpcm[FWGAN_FRAME_SIZE];
  fwgan_synthesize(st, fpcm, features);
  for (i=0;i<LPCNET_FRAME_SIZE;i++) pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
}

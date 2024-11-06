#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include "pitchdnn.h"
#include "os_support.h"
#include "nnet.h"
#include "lpcnet_private.h"


float compute_pitchdnn(
    PitchDNNState *st,
    const float *if_features,
    const float *xcorr_features,
    int arch
    )
{
  float if1_out[DENSE_IF_UPSAMPLER_1_OUT_SIZE];
  float downsampler_in[NB_XCORR_FEATURES + DENSE_IF_UPSAMPLER_2_OUT_SIZE];
  float downsampler_out[DENSE_DOWNSAMPLER_OUT_SIZE];
  float conv1_tmp1[(NB_XCORR_FEATURES + 2)*8] = {0};
  float conv1_tmp2[(NB_XCORR_FEATURES + 2)*8] = {0};
  float output[DENSE_FINAL_UPSAMPLER_OUT_SIZE];
  int i;
  int pos=0;
  float maxval=-1;
  float sum=0;
  float count=0;
  PitchDNN *model = &st->model;
  /* IF */
  compute_generic_dense(&model->dense_if_upsampler_1, if1_out, if_features, ACTIVATION_TANH, arch);
  compute_generic_dense(&model->dense_if_upsampler_2, &downsampler_in[NB_XCORR_FEATURES], if1_out, ACTIVATION_TANH, arch);
  /* xcorr*/
  OPUS_COPY(&conv1_tmp1[1], xcorr_features, NB_XCORR_FEATURES);
  compute_conv2d(&model->conv2d_1, &conv1_tmp2[1], st->xcorr_mem1, conv1_tmp1, NB_XCORR_FEATURES, NB_XCORR_FEATURES+2, ACTIVATION_TANH, arch);
  compute_conv2d(&model->conv2d_2, downsampler_in, st->xcorr_mem2, conv1_tmp2, NB_XCORR_FEATURES, NB_XCORR_FEATURES, ACTIVATION_TANH, arch);

  compute_generic_dense(&model->dense_downsampler, downsampler_out, downsampler_in, ACTIVATION_TANH, arch);
  compute_generic_gru(&model->gru_1_input, &model->gru_1_recurrent, st->gru_state, downsampler_out, arch);
  compute_generic_dense(&model->dense_final_upsampler, output, st->gru_state, ACTIVATION_LINEAR, arch);
  for (i=0;i<180;i++) {
    if (output[i] > maxval) {
      pos = i;
      maxval = output[i];
    }
  }
  for (i=IMAX(0, pos-2); i<=IMIN(179, pos+2); i++) {
    float p = exp(output[i]);
    sum += p*i;
    count += p;
  }
  /*printf("%d %f\n", pos, sum/count);*/
  return (1.f/60.f)*(sum/count) - 1.5;
  /*return 256.f/pow(2.f, (1.f/60.f)*i);*/
}


void pitchdnn_init(PitchDNNState *st)
{
  int ret;
  OPUS_CLEAR(st, 1);
#ifndef USE_WEIGHTS_FILE
  ret = init_pitchdnn(&st->model, pitchdnn_arrays);
#else
  ret = 0;
#endif
  celt_assert(ret == 0);
}

int pitchdnn_load_model(PitchDNNState *st, const void *data, int len) {
  WeightArray *list;
  int ret;
  parse_weights(&list, data, len);
  ret = init_pitchdnn(&st->model, list);
  opus_free(list);
  if (ret == 0) return 0;
  else return -1;
}

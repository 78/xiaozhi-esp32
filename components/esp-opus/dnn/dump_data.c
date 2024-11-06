/* Copyright (c) 2017-2018 Mozilla */
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
#include <unistd.h>
#include "kiss_fft.h"
#include "common.h"
#include <math.h>
#include "freq.h"
#include "pitch.h"
#include "arch.h"
#include <assert.h>
#include "lpcnet.h"
#include "lpcnet_private.h"
#include "os_support.h"
#include "cpu_support.h"


static void biquad(float *y, float mem[2], const float *x, const float *b, const float *a, int N) {
  int i;
  for (i=0;i<N;i++) {
    float xi, yi;
    xi = x[i];
    yi = x[i] + mem[0];
    mem[0] = mem[1] + (b[0]*(double)xi - a[0]*(double)yi);
    mem[1] = (b[1]*(double)xi - a[1]*(double)yi);
    y[i] = yi;
  }
}

static float uni_rand(void) {
  return rand()/(double)RAND_MAX-.5;
}

static void rand_resp(float *a, float *b) {
  a[0] = .75*uni_rand();
  a[1] = .75*uni_rand();
  b[0] = .75*uni_rand();
  b[1] = .75*uni_rand();
}

static opus_int16 float2short(float x)
{
  int i;
  i = (int)floor(.5+x);
  return IMAX(-32767, IMIN(32767, i));
}

int main(int argc, char **argv) {
  int i;
  char *argv0;
  int count=0;
  static const float a_hp[2] = {-1.99599, 0.99600};
  static const float b_hp[2] = {-2, 1};
  float a_sig[2] = {0};
  float b_sig[2] = {0};
  float mem_hp_x[2]={0};
  float mem_resp_x[2]={0};
  float mem_preemph=0;
  float x[FRAME_SIZE];
  int gain_change_count=0;
  FILE *f1;
  FILE *ffeat;
  FILE *fpcm=NULL;
  opus_int16 pcm[FRAME_SIZE]={0};
  opus_int16 tmp[FRAME_SIZE] = {0};
  float speech_gain=1;
  float old_speech_gain = 1;
  int one_pass_completed = 0;
  LPCNetEncState *st;
  int training = -1;
  int burg = 0;
  int pitch = 0;
  FILE *fnoise = NULL;
  float noise_gain = 0;
  long noise_size=0;
  int arch;
  srand(getpid());
  arch = opus_select_arch();
  st = lpcnet_encoder_create();
  argv0=argv[0];
  if (argc == 5 && strcmp(argv[1], "-btrain")==0) {
      burg = 1;
      training = 1;
  }
  else if (argc == 4 && strcmp(argv[1], "-btest")==0) {
      burg = 1;
      training = 0;
  }
  else if (argc == 5 && strcmp(argv[1], "-ptrain")==0) {
      pitch = 1;
      training = 1;
      fnoise = fopen(argv[2], "rb");
      fseek(fnoise, 0, SEEK_END);
      noise_size = ftell(fnoise);
      fseek(fnoise, 0, SEEK_SET);
      argv++;
  }
  else if (argc == 4 && strcmp(argv[1], "-ptest")==0) {
      pitch = 1;
      training = 0;
  }
  else if (argc == 5 && strcmp(argv[1], "-train")==0) training = 1;
  else if (argc == 4 && strcmp(argv[1], "-test")==0) training = 0;
  if (training == -1) {
    fprintf(stderr, "usage: %s -train <speech> <features out> <pcm out>\n", argv0);
    fprintf(stderr, "  or   %s -test <speech> <features out>\n", argv0);
    return 1;
  }
  f1 = fopen(argv[2], "r");
  if (f1 == NULL) {
    fprintf(stderr,"Error opening input .s16 16kHz speech input file: %s\n", argv[2]);
    exit(1);
  }
  ffeat = fopen(argv[3], "wb");
  if (ffeat == NULL) {
    fprintf(stderr,"Error opening output feature file: %s\n", argv[3]);
    exit(1);
  }
  if (training && !pitch) {
    fpcm = fopen(argv[4], "wb");
    if (fpcm == NULL) {
      fprintf(stderr,"Error opening output PCM file: %s\n", argv[4]);
      exit(1);
    }
  }
  while (1) {
    size_t ret;
    ret = fread(tmp, sizeof(opus_int16), FRAME_SIZE, f1);
    if (feof(f1) || ret != FRAME_SIZE) {
      if (!training) break;
      rewind(f1);
      ret = fread(tmp, sizeof(opus_int16), FRAME_SIZE, f1);
      if (ret != FRAME_SIZE) {
        fprintf(stderr, "error reading\n");
        exit(1);
      }
      one_pass_completed = 1;
    }
    for (i=0;i<FRAME_SIZE;i++) x[i] = tmp[i];
    if (count*FRAME_SIZE_5MS>=10000000 && one_pass_completed) break;
    if (training && ++gain_change_count > 2821) {
      speech_gain = pow(10., (-30+(rand()%40))/20.);
      if (rand()&1) speech_gain = -speech_gain;
      if (rand()%20==0) speech_gain *= .01;
      if (!pitch && rand()%100==0) speech_gain = 0;
      gain_change_count = 0;
      rand_resp(a_sig, b_sig);
      if (fnoise != NULL) {
        long pos;
        /* Randomize the fraction because rand() only gives us 31 bits. */
        float frac_pos = rand()/(float)RAND_MAX;
        pos = (long)(frac_pos*noise_size);
        /* 32-bit alignment. */
        pos = pos/4 * 4;
        if (pos > noise_size-500000) pos = noise_size-500000;
        noise_gain = pow(10., (-15+(rand()%40))/20.);
        if (rand()%10==0) noise_gain = 0;
        fseek(fnoise, pos, SEEK_SET);
      }
    }
    if (fnoise != NULL) {
      opus_int16 noise[FRAME_SIZE];
      ret = fread(noise, sizeof(opus_int16), FRAME_SIZE, fnoise);
      for (i=0;i<FRAME_SIZE;i++) x[i] += noise[i]*noise_gain;
    }
    biquad(x, mem_hp_x, x, b_hp, a_hp, FRAME_SIZE);
    biquad(x, mem_resp_x, x, b_sig, a_sig, FRAME_SIZE);
    for (i=0;i<FRAME_SIZE;i++) {
      float g;
      float f = (float)i/FRAME_SIZE;
      g = f*speech_gain + (1-f)*old_speech_gain;
      x[i] *= g;
    }
    if (burg) {
      float ceps[2*NB_BANDS];
      burg_cepstral_analysis(ceps, x);
      fwrite(ceps, sizeof(float), 2*NB_BANDS, ffeat);
    }
    preemphasis(x, &mem_preemph, x, PREEMPHASIS, FRAME_SIZE);
    /* PCM is delayed by 1/2 frame to make the features centered on the frames. */
    for (i=0;i<FRAME_SIZE-TRAINING_OFFSET;i++) pcm[i+TRAINING_OFFSET] = float2short(x[i]);
    compute_frame_features(st, x, arch);

    if (pitch) {
      signed char pitch_features[PITCH_MAX_PERIOD-PITCH_MIN_PERIOD+PITCH_IF_FEATURES];
      for (i=0;i<PITCH_MAX_PERIOD-PITCH_MIN_PERIOD;i++) {
        pitch_features[i] = (int)floor(.5f + 127.f*st->xcorr_features[i]);
      }
      for (i=0;i<PITCH_IF_FEATURES;i++) {
        pitch_features[i+PITCH_MAX_PERIOD-PITCH_MIN_PERIOD] = (int)floor(.5f + 127.f*st->if_features[i]);
      }
      fwrite(pitch_features, PITCH_MAX_PERIOD-PITCH_MIN_PERIOD+PITCH_IF_FEATURES, 1, ffeat);
    } else {
      fwrite(st->features, sizeof(float), NB_TOTAL_FEATURES, ffeat);
    }
    /*if(pitch) fwrite(pcm, FRAME_SIZE, 2, stdout);*/
    if (fpcm) fwrite(pcm, FRAME_SIZE, 2, fpcm);
    /*if (fpcm) fwrite(pcm, sizeof(opus_int16), FRAME_SIZE, fpcm);*/
    for (i=0;i<TRAINING_OFFSET;i++) pcm[i] = float2short(x[i+FRAME_SIZE-TRAINING_OFFSET]);
    old_speech_gain = speech_gain;
    count++;
  }
  fclose(f1);
  fclose(ffeat);
  if (fpcm) fclose(fpcm);
  lpcnet_encoder_destroy(st);
  return 0;
}

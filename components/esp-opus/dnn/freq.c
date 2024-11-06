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
#include "kiss_fft.h"
#include <math.h>
#include "freq.h"
#include "pitch.h"
#include "arch.h"
#include "burg.h"
#include <assert.h>
#include "os_support.h"

#define SQUARE(x) ((x)*(x))

static const opus_int16 eband5ms[] = {
/*0  200 400 600 800  1k 1.2 1.4 1.6  2k 2.4 2.8 3.2  4k 4.8 5.6 6.8  8k*/
  0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 34, 40
};

static const float compensation[] = {
    0.8f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.666667f, 0.5f, 0.5f, 0.5f, 0.333333f, 0.25f, 0.25f, 0.2f, 0.166667f, 0.173913f
};


extern const kiss_fft_state kfft;
extern const float half_window[OVERLAP_SIZE];
extern const float dct_table[NB_BANDS*NB_BANDS];


static void compute_band_energy_inverse(float *bandE, const kiss_fft_cpx *X) {
  int i;
  float sum[NB_BANDS] = {0};
  for (i=0;i<NB_BANDS-1;i++)
  {
    int j;
    int band_size;
    band_size = (eband5ms[i+1]-eband5ms[i])*WINDOW_SIZE_5MS;
    for (j=0;j<band_size;j++) {
      float tmp;
      float frac = (float)j/band_size;
      tmp = SQUARE(X[(eband5ms[i]*WINDOW_SIZE_5MS) + j].r);
      tmp += SQUARE(X[(eband5ms[i]*WINDOW_SIZE_5MS) + j].i);
      tmp = 1.f/(tmp + 1e-9);
      sum[i] += (1-frac)*tmp;
      sum[i+1] += frac*tmp;
    }
  }
  sum[0] *= 2;
  sum[NB_BANDS-1] *= 2;
  for (i=0;i<NB_BANDS;i++)
  {
    bandE[i] = sum[i];
  }
}

static float lpcn_lpc(
      opus_val16 *lpc, /* out: [0...p-1] LPC coefficients      */
      opus_val16 *rc,
const opus_val32 *ac,  /* in:  [0...p] autocorrelation values  */
int          p
)
{
   int i, j;
   opus_val32 r;
   opus_val32 error = ac[0];

   OPUS_CLEAR(lpc, p);
   OPUS_CLEAR(rc, p);
   if (ac[0] != 0)
   {
      for (i = 0; i < p; i++) {
         /* Sum up this iteration's reflection coefficient */
         opus_val32 rr = 0;
         for (j = 0; j < i; j++)
            rr += MULT32_32_Q31(lpc[j],ac[i - j]);
         rr += SHR32(ac[i + 1],3);
         r = -SHL32(rr,3)/error;
         rc[i] = r;
         /*  Update LPC coefficients and total error */
         lpc[i] = SHR32(r,3);
         for (j = 0; j < (i+1)>>1; j++)
         {
            opus_val32 tmp1, tmp2;
            tmp1 = lpc[j];
            tmp2 = lpc[i-1-j];
            lpc[j]     = tmp1 + MULT32_32_Q31(r,tmp2);
            lpc[i-1-j] = tmp2 + MULT32_32_Q31(r,tmp1);
         }

         error = error - MULT32_32_Q31(MULT32_32_Q31(r,r),error);
         /* Bail out once we get 30 dB gain */
         if (error<.001f*ac[0])
            break;
      }
   }
   return error;
}



void lpcn_compute_band_energy(float *bandE, const kiss_fft_cpx *X) {
  int i;
  float sum[NB_BANDS] = {0};
  for (i=0;i<NB_BANDS-1;i++)
  {
    int j;
    int band_size;
    band_size = (eband5ms[i+1]-eband5ms[i])*WINDOW_SIZE_5MS;
    for (j=0;j<band_size;j++) {
      float tmp;
      float frac = (float)j/band_size;
      tmp = SQUARE(X[(eband5ms[i]*WINDOW_SIZE_5MS) + j].r);
      tmp += SQUARE(X[(eband5ms[i]*WINDOW_SIZE_5MS) + j].i);
      sum[i] += (1-frac)*tmp;
      sum[i+1] += frac*tmp;
    }
  }
  sum[0] *= 2;
  sum[NB_BANDS-1] *= 2;
  for (i=0;i<NB_BANDS;i++)
  {
    bandE[i] = sum[i];
  }
}

static void compute_burg_cepstrum(const float *pcm, float *burg_cepstrum, int len, int order) {
  int i;
  float burg_in[FRAME_SIZE];
  float burg_lpc[LPC_ORDER];
  float x[WINDOW_SIZE];
  float Eburg[NB_BANDS];
  float g;
  kiss_fft_cpx LPC[FREQ_SIZE];
  float Ly[NB_BANDS];
  float logMax = -2;
  float follow = -2;
  assert(order <= LPC_ORDER);
  assert(len <= FRAME_SIZE);
  for (i=0;i<len-1;i++) burg_in[i] = pcm[i+1] - PREEMPHASIS*pcm[i];
  g = silk_burg_analysis(burg_lpc, burg_in, 1e-3, len-1, 1, order);
  g /= len - 2*(order-1);
  OPUS_CLEAR(x, WINDOW_SIZE);
  x[0] = 1;
  for (i=0;i<order;i++) x[i+1] = -burg_lpc[i]*pow(.995, i+1);
  forward_transform(LPC, x);
  compute_band_energy_inverse(Eburg, LPC);
  for (i=0;i<NB_BANDS;i++) Eburg[i] *= .45*g*(1.f/((float)WINDOW_SIZE*WINDOW_SIZE*WINDOW_SIZE));
  for (i=0;i<NB_BANDS;i++) {
    Ly[i] = log10(1e-2+Eburg[i]);
    Ly[i] = MAX16(logMax-8, MAX16(follow-2.5, Ly[i]));
    logMax = MAX16(logMax, Ly[i]);
    follow = MAX16(follow-2.5, Ly[i]);
  }
  dct(burg_cepstrum, Ly);
  burg_cepstrum[0] += - 4;
}

void burg_cepstral_analysis(float *ceps, const float *x) {
  int i;
  compute_burg_cepstrum(x,                &ceps[0       ], FRAME_SIZE/2, LPC_ORDER);
  compute_burg_cepstrum(&x[FRAME_SIZE/2], &ceps[NB_BANDS], FRAME_SIZE/2, LPC_ORDER);
  for (i=0;i<NB_BANDS;i++) {
    float c0, c1;
    c0 = ceps[i];
    c1 = ceps[NB_BANDS+i];
    ceps[i         ] = .5*(c0+c1);
    ceps[NB_BANDS+i] = (c0-c1);
  }
}


static void interp_band_gain(float *g, const float *bandE) {
  int i;
  memset(g, 0, FREQ_SIZE);
  for (i=0;i<NB_BANDS-1;i++)
  {
    int j;
    int band_size;
    band_size = (eband5ms[i+1]-eband5ms[i])*WINDOW_SIZE_5MS;
    for (j=0;j<band_size;j++) {
      float frac = (float)j/band_size;
      g[(eband5ms[i]*WINDOW_SIZE_5MS) + j] = (1-frac)*bandE[i] + frac*bandE[i+1];
    }
  }
}


void dct(float *out, const float *in) {
  int i;
  for (i=0;i<NB_BANDS;i++) {
    int j;
    float sum = 0;
    for (j=0;j<NB_BANDS;j++) {
      sum += in[j] * dct_table[j*NB_BANDS + i];
    }
    out[i] = sum*sqrt(2./NB_BANDS);
  }
}

static void idct(float *out, const float *in) {
  int i;
  for (i=0;i<NB_BANDS;i++) {
    int j;
    float sum = 0;
    for (j=0;j<NB_BANDS;j++) {
      sum += in[j] * dct_table[i*NB_BANDS + j];
    }
    out[i] = sum*sqrt(2./NB_BANDS);
  }
}

void forward_transform(kiss_fft_cpx *out, const float *in) {
  int i;
  kiss_fft_cpx x[WINDOW_SIZE];
  kiss_fft_cpx y[WINDOW_SIZE];
  for (i=0;i<WINDOW_SIZE;i++) {
    x[i].r = in[i];
    x[i].i = 0;
  }
  opus_fft(&kfft, x, y, 0);
  for (i=0;i<FREQ_SIZE;i++) {
    out[i] = y[i];
  }
}

static void inverse_transform(float *out, const kiss_fft_cpx *in) {
  int i;
  kiss_fft_cpx x[WINDOW_SIZE];
  kiss_fft_cpx y[WINDOW_SIZE];
  for (i=0;i<FREQ_SIZE;i++) {
    x[i] = in[i];
  }
  for (;i<WINDOW_SIZE;i++) {
    x[i].r = x[WINDOW_SIZE - i].r;
    x[i].i = -x[WINDOW_SIZE - i].i;
  }
  opus_fft(&kfft, x, y, 0);
  /* output in reverse order for IFFT. */
  out[0] = WINDOW_SIZE*y[0].r;
  for (i=1;i<WINDOW_SIZE;i++) {
    out[i] = WINDOW_SIZE*y[WINDOW_SIZE - i].r;
  }
}

static float lpc_from_bands(float *lpc, const float *Ex)
{
   int i;
   float e;
   float ac[LPC_ORDER+1];
   float rc[LPC_ORDER];
   float Xr[FREQ_SIZE];
   kiss_fft_cpx X_auto[FREQ_SIZE];
   float x_auto[WINDOW_SIZE];
   interp_band_gain(Xr, Ex);
   Xr[FREQ_SIZE-1] = 0;
   OPUS_CLEAR(X_auto, FREQ_SIZE);
   for (i=0;i<FREQ_SIZE;i++) X_auto[i].r = Xr[i];
   inverse_transform(x_auto, X_auto);
   for (i=0;i<LPC_ORDER+1;i++) ac[i] = x_auto[i];

   /* -40 dB noise floor. */
   ac[0] += ac[0]*1e-4 + 320/12/38.;
   /* Lag windowing. */
   for (i=1;i<LPC_ORDER+1;i++) ac[i] *= (1 - 6e-5*i*i);
   e = lpcn_lpc(lpc, rc, ac, LPC_ORDER);
   return e;
}

void lpc_weighting(float *lpc, float gamma)
{
  int i;
  float gamma_i = gamma;
  for (i = 0; i < LPC_ORDER; i++)
  {
    lpc[i] *= gamma_i;
    gamma_i *= gamma;
  }
}

float lpc_from_cepstrum(float *lpc, const float *cepstrum)
{
   int i;
   float Ex[NB_BANDS];
   float tmp[NB_BANDS];
   OPUS_COPY(tmp, cepstrum, NB_BANDS);
   tmp[0] += 4;
   idct(Ex, tmp);
   for (i=0;i<NB_BANDS;i++) Ex[i] = pow(10.f, Ex[i])*compensation[i];
   return lpc_from_bands(lpc, Ex);
}

void apply_window(float *x) {
  int i;
  for (i=0;i<OVERLAP_SIZE;i++) {
    x[i] *= half_window[i];
    x[WINDOW_SIZE - 1 - i] *= half_window[i];
  }
}

/* Copyright (c) 2023 Amazon
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

#define OSCE_SPEC_WINDOW_SIZE 320
#define OSCE_SPEC_NUM_FREQS 161


/*DEBUG*/
/*#define WRITE_FEATURES*/
/*#define DEBUG_PRING*/
/*******/

#include "stack_alloc.h"
#include "osce_features.h"
#include "kiss_fft.h"
#include "os_support.h"
#include "osce.h"
#include "freq.h"


#if defined(WRITE_FEATURES) || defined(DEBUG_PRING)
#include <stdio.h>
#include <stdlib.h>
#endif

static const int center_bins_clean[64] = {
      0,      2,      5,      8,     10,     12,     15,     18,
     20,     22,     25,     28,     30,     33,     35,     38,
     40,     42,     45,     48,     50,     52,     55,     58,
     60,     62,     65,     68,     70,     73,     75,     78,
     80,     82,     85,     88,     90,     92,     95,     98,
    100,    102,    105,    108,    110,    112,    115,    118,
    120,    122,    125,    128,    130,    132,    135,    138,
    140,    142,    145,    148,    150,    152,    155,    160
};

static const int center_bins_noisy[18] = {
      0,      4,      8,     12,     16,     20,     24,     28,
     32,     40,     48,     56,     64,     80,     96,    112,
    136,    160
};

static const float band_weights_clean[64] = {
     0.666666666667f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.400000000000f,     0.400000000000f,     0.400000000000f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.400000000000f,     0.400000000000f,     0.400000000000f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.333333333333f,     0.400000000000f,
     0.500000000000f,     0.400000000000f,     0.250000000000f,     0.333333333333f
};

static const float band_weights_noisy[18] = {
     0.400000000000f,     0.250000000000f,     0.250000000000f,     0.250000000000f,
     0.250000000000f,     0.250000000000f,     0.250000000000f,     0.250000000000f,
     0.166666666667f,     0.125000000000f,     0.125000000000f,     0.125000000000f,
     0.083333333333f,     0.062500000000f,     0.062500000000f,     0.050000000000f,
     0.041666666667f,     0.080000000000f
};

static float osce_window[OSCE_SPEC_WINDOW_SIZE] = {
     0.004908718808f,     0.014725683311f,     0.024541228523f,     0.034354408400f,     0.044164277127f,
     0.053969889210f,     0.063770299562f,     0.073564563600f,     0.083351737332f,     0.093130877450f,
     0.102901041421f,     0.112661287575f,     0.122410675199f,     0.132148264628f,     0.141873117332f,
     0.151584296010f,     0.161280864678f,     0.170961888760f,     0.180626435180f,     0.190273572448f,
     0.199902370753f,     0.209511902052f,     0.219101240157f,     0.228669460829f,     0.238215641862f,
     0.247738863176f,     0.257238206902f,     0.266712757475f,     0.276161601717f,     0.285583828929f,
     0.294978530977f,     0.304344802381f,     0.313681740399f,     0.322988445118f,     0.332264019538f,
     0.341507569661f,     0.350718204573f,     0.359895036535f,     0.369037181064f,     0.378143757022f,
     0.387213886697f,     0.396246695891f,     0.405241314005f,     0.414196874117f,     0.423112513073f,
     0.431987371563f,     0.440820594212f,     0.449611329655f,     0.458358730621f,     0.467061954019f,
     0.475720161014f,     0.484332517110f,     0.492898192230f,     0.501416360796f,     0.509886201809f,
     0.518306898929f,     0.526677640552f,     0.534997619887f,     0.543266035038f,     0.551482089078f,
     0.559644990127f,     0.567753951426f,     0.575808191418f,     0.583806933818f,     0.591749407690f,
     0.599634847523f,     0.607462493302f,     0.615231590581f,     0.622941390558f,     0.630591150148f,
     0.638180132051f,     0.645707604824f,     0.653172842954f,     0.660575126926f,     0.667913743292f,
     0.675187984742f,     0.682397150168f,     0.689540544737f,     0.696617479953f,     0.703627273726f,
     0.710569250438f,     0.717442741007f,     0.724247082951f,     0.730981620454f,     0.737645704427f,
     0.744238692572f,     0.750759949443f,     0.757208846506f,     0.763584762206f,     0.769887082016f,
     0.776115198508f,     0.782268511401f,     0.788346427627f,     0.794348361383f,     0.800273734191f,
     0.806121974951f,     0.811892519997f,     0.817584813152f,     0.823198305781f,     0.828732456844f,
     0.834186732948f,     0.839560608398f,     0.844853565250f,     0.850065093356f,     0.855194690420f,
     0.860241862039f,     0.865206121757f,     0.870086991109f,     0.874883999665f,     0.879596685080f,
     0.884224593137f,     0.888767277786f,     0.893224301196f,     0.897595233788f,     0.901879654283f,
     0.906077149740f,     0.910187315596f,     0.914209755704f,     0.918144082372f,     0.921989916403f,
     0.925746887127f,     0.929414632439f,     0.932992798835f,     0.936481041442f,     0.939879024058f,
     0.943186419177f,     0.946402908026f,     0.949528180593f,     0.952561935658f,     0.955503880820f,
     0.958353732530f,     0.961111216112f,     0.963776065795f,     0.966348024735f,     0.968826845041f,
     0.971212287799f,     0.973504123096f,     0.975702130039f,     0.977806096779f,     0.979815820533f,
     0.981731107599f,     0.983551773378f,     0.985277642389f,     0.986908548290f,     0.988444333892f,
     0.989884851171f,     0.991229961288f,     0.992479534599f,     0.993633450666f,     0.994691598273f,
     0.995653875433f,     0.996520189401f,     0.997290456679f,     0.997964603026f,     0.998542563469f,
     0.999024282300f,     0.999409713092f,     0.999698818696f,     0.999891571247f,     0.999987952167f,
     0.999987952167f,     0.999891571247f,     0.999698818696f,     0.999409713092f,     0.999024282300f,
     0.998542563469f,     0.997964603026f,     0.997290456679f,     0.996520189401f,     0.995653875433f,
     0.994691598273f,     0.993633450666f,     0.992479534599f,     0.991229961288f,     0.989884851171f,
     0.988444333892f,     0.986908548290f,     0.985277642389f,     0.983551773378f,     0.981731107599f,
     0.979815820533f,     0.977806096779f,     0.975702130039f,     0.973504123096f,     0.971212287799f,
     0.968826845041f,     0.966348024735f,     0.963776065795f,     0.961111216112f,     0.958353732530f,
     0.955503880820f,     0.952561935658f,     0.949528180593f,     0.946402908026f,     0.943186419177f,
     0.939879024058f,     0.936481041442f,     0.932992798835f,     0.929414632439f,     0.925746887127f,
     0.921989916403f,     0.918144082372f,     0.914209755704f,     0.910187315596f,     0.906077149740f,
     0.901879654283f,     0.897595233788f,     0.893224301196f,     0.888767277786f,     0.884224593137f,
     0.879596685080f,     0.874883999665f,     0.870086991109f,     0.865206121757f,     0.860241862039f,
     0.855194690420f,     0.850065093356f,     0.844853565250f,     0.839560608398f,     0.834186732948f,
     0.828732456844f,     0.823198305781f,     0.817584813152f,     0.811892519997f,     0.806121974951f,
     0.800273734191f,     0.794348361383f,     0.788346427627f,     0.782268511401f,     0.776115198508f,
     0.769887082016f,     0.763584762206f,     0.757208846506f,     0.750759949443f,     0.744238692572f,
     0.737645704427f,     0.730981620454f,     0.724247082951f,     0.717442741007f,     0.710569250438f,
     0.703627273726f,     0.696617479953f,     0.689540544737f,     0.682397150168f,     0.675187984742f,
     0.667913743292f,     0.660575126926f,     0.653172842954f,     0.645707604824f,     0.638180132051f,
     0.630591150148f,     0.622941390558f,     0.615231590581f,     0.607462493302f,     0.599634847523f,
     0.591749407690f,     0.583806933818f,     0.575808191418f,     0.567753951426f,     0.559644990127f,
     0.551482089078f,     0.543266035038f,     0.534997619887f,     0.526677640552f,     0.518306898929f,
     0.509886201809f,     0.501416360796f,     0.492898192230f,     0.484332517110f,     0.475720161014f,
     0.467061954019f,     0.458358730621f,     0.449611329655f,     0.440820594212f,     0.431987371563f,
     0.423112513073f,     0.414196874117f,     0.405241314005f,     0.396246695891f,     0.387213886697f,
     0.378143757022f,     0.369037181064f,     0.359895036535f,     0.350718204573f,     0.341507569661f,
     0.332264019538f,     0.322988445118f,     0.313681740399f,     0.304344802381f,     0.294978530977f,
     0.285583828929f,     0.276161601717f,     0.266712757475f,     0.257238206902f,     0.247738863176f,
     0.238215641862f,     0.228669460829f,     0.219101240157f,     0.209511902052f,     0.199902370753f,
     0.190273572448f,     0.180626435180f,     0.170961888760f,     0.161280864678f,     0.151584296010f,
     0.141873117332f,     0.132148264628f,     0.122410675199f,     0.112661287575f,     0.102901041421f,
     0.093130877450f,     0.083351737332f,     0.073564563600f,     0.063770299562f,     0.053969889210f,
     0.044164277127f,     0.034354408400f,     0.024541228523f,     0.014725683311f,     0.004908718808f
};

static void apply_filterbank(float *x_out, float *x_in, const int *center_bins, const float* band_weights, int num_bands)
{
    int b, i;
    float frac;

    celt_assert(x_in != x_out)

    x_out[0] = 0;
    for (b = 0; b < num_bands - 1; b++)
    {
        x_out[b+1] = 0;
        for (i = center_bins[b]; i < center_bins[b+1]; i++)
        {
            frac = (float) (center_bins[b+1] - i) / (center_bins[b+1] - center_bins[b]);
            x_out[b]   += band_weights[b] * frac * x_in[i];
            x_out[b+1] += band_weights[b+1] * (1 - frac) * x_in[i];

        }
    }
    x_out[num_bands - 1] += band_weights[num_bands - 1] * x_in[center_bins[num_bands - 1]];
#ifdef DEBUG_PRINT
    for (b = 0; b < num_bands; b++)
    {
        printf("band[%d]: %f\n", b, x_out[b]);
    }
#endif
}


static void mag_spec_320_onesided(float *out, float *in)
{
    celt_assert(OSCE_SPEC_WINDOW_SIZE == 320);
    kiss_fft_cpx buffer[OSCE_SPEC_WINDOW_SIZE];
    int k;
    forward_transform(buffer, in);

    for (k = 0; k < OSCE_SPEC_NUM_FREQS; k++)
    {
        out[k] = OSCE_SPEC_WINDOW_SIZE * sqrt(buffer[k].r * buffer[k].r + buffer[k].i * buffer[k].i);
#ifdef DEBUG_PRINT
        printf("magspec[%d]: %f\n", k, out[k]);
#endif
    }
}


static void calculate_log_spectrum_from_lpc(float *spec, opus_int16 *a_q12, int lpc_order)
{
    float buffer[OSCE_SPEC_WINDOW_SIZE] = {0};
    int i;

    /* zero expansion */
    buffer[0] = 1;
    for (i = 0; i < lpc_order; i++)
    {
        buffer[i+1] = - (float)a_q12[i] / (1U << 12);
    }

    /* calculate and invert magnitude spectrum */
    mag_spec_320_onesided(buffer, buffer);

    for (i = 0; i < OSCE_SPEC_NUM_FREQS; i++)
    {
        buffer[i] = 1.f / (buffer[i] + 1e-9f);
    }

    /* apply filterbank */
    apply_filterbank(spec, buffer, center_bins_clean, band_weights_clean, OSCE_CLEAN_SPEC_NUM_BANDS);

    /* log and scaling */
    for (i = 0; i < OSCE_CLEAN_SPEC_NUM_BANDS; i++)
    {
        spec[i] = 0.3f * log(spec[i] + 1e-9f);
    }
}

static void calculate_cepstrum(float *cepstrum, float *signal)
{
    float buffer[OSCE_SPEC_WINDOW_SIZE];
    float *spec = &buffer[OSCE_SPEC_NUM_FREQS + 3];
    int n;

    celt_assert(cepstrum != signal)

    for (n = 0; n < OSCE_SPEC_WINDOW_SIZE; n++)
    {
        buffer[n] = osce_window[n] * signal[n];
    }

    /* calculate magnitude spectrum */
    mag_spec_320_onesided(buffer, buffer);

    /* accumulate bands */
    apply_filterbank(spec, buffer, center_bins_noisy, band_weights_noisy, OSCE_NOISY_SPEC_NUM_BANDS);

    /* log domain conversion */
    for (n = 0; n < OSCE_NOISY_SPEC_NUM_BANDS; n++)
    {
        spec[n] = log(spec[n] + 1e-9f);
#ifdef DEBUG_PRINT
        printf("logspec[%d]: %f\n", n, spec[n]);
#endif
    }

    /* DCT-II (orthonormal) */
    celt_assert(OSCE_NOISY_SPEC_NUM_BANDS == NB_BANDS);
    dct(cepstrum, spec);
}

static void calculate_acorr(float *acorr, float *signal, int lag)
{
    int n, k;
    celt_assert(acorr != signal)

    for (k = -2; k <= 2; k++)
    {
        acorr[k+2] = 0;
        float xx = 0;
        float xy = 0;
        float yy = 0;
        for (n = 0; n < 80; n++)
        {
            /* obviously wasteful -> fix later */
            xx += signal[n] * signal[n];
            yy += signal[n - lag + k] * signal[n - lag + k];
            xy += signal[n] * signal[n - lag + k];
        }
        acorr[k+2] = xy / sqrt(xx * yy + 1e-9f);
    }
}

static int pitch_postprocessing(OSCEFeatureState *psFeatures, int lag, int type)
{
    int new_lag;
    int modulus;

#ifdef OSCE_HANGOVER_BUGFIX
#define TESTBIT 1
#else
#define TESTBIT 0
#endif

    modulus = OSCE_PITCH_HANGOVER;
    if (modulus == 0) modulus ++;

    /* hangover is currently disabled to reflect a bug in the python code. ToDo: re-evaluate hangover */
    if (type != TYPE_VOICED && psFeatures->last_type == TYPE_VOICED && TESTBIT)
    /* enter hangover */
    {
        new_lag = OSCE_NO_PITCH_VALUE;
        if (psFeatures->pitch_hangover_count < OSCE_PITCH_HANGOVER)
        {
            new_lag = psFeatures->last_lag;
            psFeatures->pitch_hangover_count = (psFeatures->pitch_hangover_count + 1) % modulus;
        }
    }
    else if (type != TYPE_VOICED && psFeatures->pitch_hangover_count && TESTBIT)
    /* continue hangover */
    {
        new_lag = psFeatures->last_lag;
        psFeatures->pitch_hangover_count = (psFeatures->pitch_hangover_count + 1) % modulus;
    }
    else if (type != TYPE_VOICED)
    /* unvoiced frame after hangover */
    {
        new_lag = OSCE_NO_PITCH_VALUE;
        psFeatures->pitch_hangover_count = 0;
    }
    else
    /* voiced frame: update last_lag */
    {
        new_lag = lag;
        psFeatures->last_lag = lag;
        psFeatures->pitch_hangover_count = 0;
    }

    /* buffer update */
    psFeatures->last_type = type;

    /* with the current setup this should never happen (but who knows...) */
    celt_assert(new_lag)

    return new_lag;
}

void osce_calculate_features(
    silk_decoder_state          *psDec,                         /* I/O  Decoder state                               */
    silk_decoder_control        *psDecCtrl,                     /* I    Decoder control                             */
    float                       *features,                      /* O    input features                              */
    float                       *numbits,                       /* O    numbits and smoothed numbits                */
    int                         *periods,                       /* O    pitch lags on subframe basis                */
    const opus_int16            xq[],                           /* I    Decoded speech                              */
    opus_int32                  num_bits                        /* I    Size of SILK payload in bits                */
)
{
    int num_subframes, num_samples;
    float buffer[OSCE_FEATURES_MAX_HISTORY + OSCE_MAX_FEATURE_FRAMES * 80];
    float *frame, *pfeatures;
    OSCEFeatureState *psFeatures;
    int i, n, k;
#ifdef WRITE_FEATURES
    static FILE *f_feat = NULL;
    if (f_feat == NULL)
    {
        f_feat = fopen("assembled_features.f32", "wb");
    }
#endif

    /*OPUS_CLEAR(buffer, 1);*/
    memset(buffer, 0, sizeof(buffer));

    num_subframes = psDec->nb_subfr;
    num_samples = num_subframes * 80;
    psFeatures = &psDec->osce.features;

    /* smooth bit count */
    psFeatures->numbits_smooth = 0.9f * psFeatures->numbits_smooth + 0.1f * num_bits;
    numbits[0] = num_bits;
    numbits[1] = psFeatures->numbits_smooth;

    for (n = 0; n < num_samples; n++)
    {
        buffer[OSCE_FEATURES_MAX_HISTORY + n] = (float) xq[n] / (1U<<15);
    }
    OPUS_COPY(buffer, psFeatures->signal_history, OSCE_FEATURES_MAX_HISTORY);

    for (k = 0; k < num_subframes; k++)
    {
        pfeatures = features + k * OSCE_FEATURE_DIM;
        frame = &buffer[OSCE_FEATURES_MAX_HISTORY + k * 80];
        memset(pfeatures, 0, OSCE_FEATURE_DIM); /* precaution */

        /* clean spectrum from lpcs (update every other frame) */
        if (k % 2 == 0)
        {
            calculate_log_spectrum_from_lpc(pfeatures + OSCE_CLEAN_SPEC_START, psDecCtrl->PredCoef_Q12[k >> 1], psDec->LPC_order);
        }
        else
        {
            OPUS_COPY(pfeatures + OSCE_CLEAN_SPEC_START, pfeatures + OSCE_CLEAN_SPEC_START - OSCE_FEATURE_DIM, OSCE_CLEAN_SPEC_LENGTH);
        }

        /* noisy cepstrum from signal (update every other frame) */
        if (k % 2 == 0)
        {
            calculate_cepstrum(pfeatures + OSCE_NOISY_CEPSTRUM_START, frame - 160);
        }
        else
        {
            OPUS_COPY(pfeatures + OSCE_NOISY_CEPSTRUM_START, pfeatures + OSCE_NOISY_CEPSTRUM_START - OSCE_FEATURE_DIM, OSCE_NOISY_CEPSTRUM_LENGTH);
        }

        /* pitch hangover and zero value replacement */
        periods[k] = pitch_postprocessing(psFeatures, psDecCtrl->pitchL[k], psDec->indices.signalType);

        /* auto-correlation around pitch lag */
        calculate_acorr(pfeatures + OSCE_ACORR_START, frame, periods[k]);

        /* ltp */
        celt_assert(OSCE_LTP_LENGTH == LTP_ORDER)
        for (i = 0; i < OSCE_LTP_LENGTH; i++)
        {
            pfeatures[OSCE_LTP_START + i] = (float) psDecCtrl->LTPCoef_Q14[k * LTP_ORDER + i] / (1U << 14);
        }

        /* frame gain */
        pfeatures[OSCE_LOG_GAIN_START] = log((float) psDecCtrl->Gains_Q16[k] / (1UL << 16) + 1e-9f);

#ifdef WRITE_FEATURES
        fwrite(pfeatures, sizeof(*pfeatures), 93, f_feat);
#endif
    }

    /* buffer update */
    OPUS_COPY(psFeatures->signal_history, &buffer[num_samples], OSCE_FEATURES_MAX_HISTORY);
}


void osce_cross_fade_10ms(float *x_enhanced, float *x_in, int length)
{
    int i;
    celt_assert(length >= 160);

    for (i = 0; i < 160; i++)
    {
        x_enhanced[i] = osce_window[i] * x_enhanced[i] + (1.f - osce_window[i]) * x_in[i];
    }


}

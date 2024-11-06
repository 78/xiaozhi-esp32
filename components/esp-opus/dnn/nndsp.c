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


#include "nndsp.h"
#include "arch.h"
#include "nnet.h"
#include "os_support.h"
#include "pitch.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.141592653589793f
#endif

#define KERNEL_INDEX(i_out_channels, i_in_channels, i_kernel) ((((i_out_channels) * in_channels) + (i_in_channels)) * kernel_size + (i_kernel))

void init_adaconv_state(AdaConvState *hAdaConv)
{
    OPUS_CLEAR(hAdaConv, 1);
}

void init_adacomb_state(AdaCombState *hAdaComb)
{
    OPUS_CLEAR(hAdaComb, 1);
}

void init_adashape_state(AdaShapeState *hAdaShape)
{
    OPUS_CLEAR(hAdaShape, 1);
}

void compute_overlap_window(float *window, int overlap_size)
{
    int i_sample;
    for (i_sample=0; i_sample < overlap_size; i_sample++)
    {
        window[i_sample] = 0.5f + 0.5f * cos(M_PI * (i_sample + 0.5f) / overlap_size);
    }
}

#ifdef DEBUG_NNDSP
void print_float_vector(const char* name, const float *vec, int length)
{
    for (int i = 0; i < length; i ++)
    {
        printf("%s[%d]: %f\n", name, i, vec[i]);
    }
}
#endif

static void scale_kernel(
    float *kernel,
    int in_channels,
    int out_channels,
    int kernel_size,
    float *gain
)
/* normalizes (p-norm) kernel over input channel and kernel dimension */
{
    float norm;
    int i_in_channels, i_out_channels, i_kernel;

    for (i_out_channels = 0; i_out_channels < out_channels; i_out_channels++)
    {
        norm = 0;
        for (i_in_channels = 0; i_in_channels < in_channels; i_in_channels ++)
        {
            for (i_kernel = 0; i_kernel < kernel_size; i_kernel++)
            {
                norm += kernel[KERNEL_INDEX(i_out_channels, i_in_channels, i_kernel)] * kernel[KERNEL_INDEX(i_out_channels, i_in_channels, i_kernel)];
            }
        }
#ifdef DEBUG_NNDSP
        printf("kernel norm: %f, %f\n", norm, sqrt(norm));
#endif
        norm = 1.f / (1e-6f + sqrt(norm));
        for (i_in_channels = 0; i_in_channels < in_channels; i_in_channels++)
        {
            for (i_kernel = 0; i_kernel < kernel_size; i_kernel++)
            {

                kernel[KERNEL_INDEX(i_out_channels, i_in_channels, i_kernel)] *= norm * gain[i_out_channels];
            }
        }
    }
}

static void transform_gains(
    float *gains,
    int num_gains,
    float filter_gain_a,
    float filter_gain_b
)
{
    int i;
    for (i = 0; i < num_gains; i++)
    {
        gains[i] = exp(filter_gain_a * gains[i] + filter_gain_b);
    }
}

void adaconv_process_frame(
    AdaConvState* hAdaConv,
    float *x_out,
    const float *x_in,
    const float *features,
    const LinearLayer *kernel_layer,
    const LinearLayer *gain_layer,
    int feature_dim,
    int frame_size,
    int overlap_size,
    int in_channels,
    int out_channels,
    int kernel_size,
    int left_padding,
    float filter_gain_a,
    float filter_gain_b,
    float shape_gain,
    float *window,
    int arch
)
{
    float output_buffer[ADACONV_MAX_FRAME_SIZE * ADACONV_MAX_OUTPUT_CHANNELS];
    float kernel_buffer[ADACONV_MAX_KERNEL_SIZE * ADACONV_MAX_INPUT_CHANNELS * ADACONV_MAX_OUTPUT_CHANNELS];
    float input_buffer[ADACONV_MAX_INPUT_CHANNELS * (ADACONV_MAX_FRAME_SIZE + ADACONV_MAX_KERNEL_SIZE)];
    float kernel0[ADACONV_MAX_KERNEL_SIZE];
    float kernel1[ADACONV_MAX_KERNEL_SIZE];
    float channel_buffer0[ADACONV_MAX_OVERLAP_SIZE];
    float channel_buffer1[ADACONV_MAX_FRAME_SIZE];
    float gain_buffer[ADACONV_MAX_OUTPUT_CHANNELS];
    float *p_input;
    int i_in_channels, i_out_channels, i_sample;

    (void) feature_dim; /* ToDo: figure out whether we might need this information */

    celt_assert(shape_gain == 1);
    celt_assert(left_padding == kernel_size - 1); /* currently only supports causal version. Non-causal version not difficult to implement but will require third loop */
    celt_assert(kernel_size < frame_size);

    OPUS_CLEAR(output_buffer, ADACONV_MAX_FRAME_SIZE * ADACONV_MAX_OUTPUT_CHANNELS);
    OPUS_CLEAR(kernel_buffer, ADACONV_MAX_KERNEL_SIZE * ADACONV_MAX_INPUT_CHANNELS * ADACONV_MAX_OUTPUT_CHANNELS);
    OPUS_CLEAR(input_buffer, ADACONV_MAX_INPUT_CHANNELS * (ADACONV_MAX_FRAME_SIZE + ADACONV_MAX_KERNEL_SIZE));

#ifdef DEBUG_NNDSP
    print_float_vector("x_in", x_in, in_channels * frame_size);
#endif

    /* prepare input */
    for (i_in_channels=0; i_in_channels < in_channels; i_in_channels ++)
    {
        OPUS_COPY(input_buffer + i_in_channels * (kernel_size + frame_size), hAdaConv->history + i_in_channels * kernel_size, kernel_size);
        OPUS_COPY(input_buffer + kernel_size + i_in_channels * (kernel_size + frame_size), x_in + frame_size * i_in_channels, frame_size);
    }
    p_input = input_buffer + kernel_size;


    /* calculate new kernel and new gain */
    compute_generic_dense(kernel_layer, kernel_buffer, features, ACTIVATION_LINEAR, arch);
    compute_generic_dense(gain_layer, gain_buffer, features, ACTIVATION_TANH, arch);
#ifdef DEBUG_NNDSP
    print_float_vector("features", features, feature_dim);
    print_float_vector("adaconv_kernel_raw", kernel_buffer, in_channels * out_channels * kernel_size);
    print_float_vector("adaconv_gain_raw", gain_buffer, out_channels);
#endif
    transform_gains(gain_buffer, out_channels, filter_gain_a, filter_gain_b);
    scale_kernel(kernel_buffer, in_channels, out_channels, kernel_size, gain_buffer);

#ifdef DEBUG_NNDSP
    print_float_vector("adaconv_kernel", kernel_buffer, in_channels * out_channels * kernel_size);
    print_float_vector("adaconv_gain", gain_buffer, out_channels);
#endif

    /* calculate overlapping part using kernel from last frame */

    for (i_out_channels = 0; i_out_channels < out_channels; i_out_channels++)
    {
        for (i_in_channels = 0; i_in_channels < in_channels; i_in_channels++)
        {
            OPUS_CLEAR(kernel0, ADACONV_MAX_KERNEL_SIZE);
            OPUS_CLEAR(kernel1, ADACONV_MAX_KERNEL_SIZE);

            OPUS_COPY(kernel0, hAdaConv->last_kernel + KERNEL_INDEX(i_out_channels, i_in_channels, 0), kernel_size);
            OPUS_COPY(kernel1, kernel_buffer + KERNEL_INDEX(i_out_channels, i_in_channels, 0), kernel_size);
            celt_pitch_xcorr(kernel0, p_input + i_in_channels * (frame_size + kernel_size) - left_padding, channel_buffer0, ADACONV_MAX_KERNEL_SIZE, overlap_size, arch);
            celt_pitch_xcorr(kernel1, p_input + i_in_channels * (frame_size + kernel_size) - left_padding, channel_buffer1, ADACONV_MAX_KERNEL_SIZE, frame_size, arch);
            for (i_sample = 0; i_sample < overlap_size; i_sample++)
            {
                output_buffer[i_sample + i_out_channels * frame_size] +=  window[i_sample] * channel_buffer0[i_sample];
                output_buffer[i_sample + i_out_channels * frame_size] += (1.f - window[i_sample]) * channel_buffer1[i_sample];
            }
            for (i_sample = overlap_size; i_sample < frame_size; i_sample++)
            {
                output_buffer[i_sample + i_out_channels * frame_size] += channel_buffer1[i_sample];
            }
        }
    }

    OPUS_COPY(x_out, output_buffer, out_channels * frame_size);

#ifdef DEBUG_NNDSP
    print_float_vector("x_out", x_out, out_channels * frame_size);
#endif

    /* buffer update */
    for (i_in_channels=0; i_in_channels < in_channels; i_in_channels ++)
    {
        OPUS_COPY(hAdaConv->history + i_in_channels * kernel_size, p_input + i_in_channels * (frame_size + kernel_size) + frame_size - kernel_size, kernel_size);
    }
    OPUS_COPY(hAdaConv->last_kernel, kernel_buffer, kernel_size * in_channels * out_channels);
}

void adacomb_process_frame(
    AdaCombState* hAdaComb,
    float *x_out,
    const float *x_in,
    const float *features,
    const LinearLayer *kernel_layer,
    const LinearLayer *gain_layer,
    const LinearLayer *global_gain_layer,
    int pitch_lag,
    int feature_dim,
    int frame_size,
    int overlap_size,
    int kernel_size,
    int left_padding,
    float filter_gain_a,
    float filter_gain_b,
    float log_gain_limit,
    float *window,
    int arch
)
{
    float output_buffer[ADACOMB_MAX_FRAME_SIZE];
    float output_buffer_last[ADACOMB_MAX_FRAME_SIZE];
    float kernel_buffer[ADACOMB_MAX_KERNEL_SIZE];
    float input_buffer[ADACOMB_MAX_FRAME_SIZE + ADACOMB_MAX_LAG + ADACOMB_MAX_KERNEL_SIZE];
    float gain, global_gain;
    float *p_input;
    int i_sample;
    float kernel[16];
    float last_kernel[16];

    (void) feature_dim; /* ToDo: figure out whether we might need this information */

    OPUS_CLEAR(output_buffer, ADACOMB_MAX_FRAME_SIZE);
    OPUS_CLEAR(kernel_buffer, ADACOMB_MAX_KERNEL_SIZE);
    OPUS_CLEAR(input_buffer, ADACOMB_MAX_FRAME_SIZE + ADACOMB_MAX_LAG + ADACOMB_MAX_KERNEL_SIZE);

    OPUS_COPY(input_buffer, hAdaComb->history, kernel_size + ADACOMB_MAX_LAG);
    OPUS_COPY(input_buffer + kernel_size + ADACOMB_MAX_LAG, x_in, frame_size);
    p_input = input_buffer + kernel_size + ADACOMB_MAX_LAG;

    /* calculate new kernel and new gain */
    compute_generic_dense(kernel_layer, kernel_buffer, features, ACTIVATION_LINEAR, arch);
    compute_generic_dense(gain_layer, &gain, features, ACTIVATION_RELU, arch);
    compute_generic_dense(global_gain_layer, &global_gain, features, ACTIVATION_TANH, arch);
#ifdef DEBUG_NNDSP
    print_float_vector("features", features, feature_dim);
    print_float_vector("adacomb_kernel_raw", kernel_buffer, kernel_size);
    print_float_vector("adacomb_gain_raw", &gain, 1);
    print_float_vector("adacomb_global_gain_raw", &global_gain, 1);
#endif
    gain = exp(log_gain_limit - gain);
    global_gain = exp(filter_gain_a * global_gain + filter_gain_b);
    scale_kernel(kernel_buffer, 1, 1, kernel_size, &gain);

#ifdef DEBUG_NNDSP
    print_float_vector("adacomb_kernel", kernel_buffer, kernel_size);
    print_float_vector("adacomb_gain", &gain, 1);
#endif

    OPUS_CLEAR(kernel, ADACOMB_MAX_KERNEL_SIZE);
    OPUS_CLEAR(last_kernel, ADACOMB_MAX_KERNEL_SIZE);
    OPUS_COPY(kernel, kernel_buffer, kernel_size);
    OPUS_COPY(last_kernel, hAdaComb->last_kernel, kernel_size);

    celt_pitch_xcorr(last_kernel, &p_input[- left_padding - hAdaComb->last_pitch_lag], output_buffer_last, ADACOMB_MAX_KERNEL_SIZE, overlap_size, arch);

    celt_pitch_xcorr(kernel, &p_input[- left_padding - pitch_lag], output_buffer, ADACOMB_MAX_KERNEL_SIZE, frame_size, arch);
    for (i_sample = 0; i_sample < overlap_size; i_sample++)
    {
      output_buffer[i_sample] = hAdaComb->last_global_gain * window[i_sample] * output_buffer_last[i_sample] + global_gain * (1.f - window[i_sample]) * output_buffer[i_sample];
    }

    for (i_sample = 0; i_sample < overlap_size; i_sample++)
    {
      output_buffer[i_sample] += (window[i_sample] * hAdaComb->last_global_gain + (1.f - window[i_sample]) * global_gain) * p_input[i_sample];
    }

    for (i_sample = overlap_size; i_sample < frame_size; i_sample++)
    {
      output_buffer[i_sample] = global_gain * (output_buffer[i_sample] + p_input[i_sample]);
    }
    OPUS_COPY(x_out, output_buffer, frame_size);

#ifdef DEBUG_NNDSP
    print_float_vector("x_out", x_out, frame_size);
#endif

    /* buffer update */
    OPUS_COPY(hAdaComb->last_kernel, kernel_buffer, kernel_size);
    OPUS_COPY(hAdaComb->history, p_input + frame_size - kernel_size - ADACOMB_MAX_LAG, kernel_size + ADACOMB_MAX_LAG);
    hAdaComb->last_pitch_lag = pitch_lag;
    hAdaComb->last_global_gain = global_gain;
}


void adashape_process_frame(
    AdaShapeState *hAdaShape,
    float *x_out,
    const float *x_in,
    const float *features,
    const LinearLayer *alpha1f,
    const LinearLayer *alpha1t,
    const LinearLayer *alpha2,
    int feature_dim,
    int frame_size,
    int avg_pool_k,
    int arch
)
{
    float in_buffer[ADASHAPE_MAX_INPUT_DIM + ADASHAPE_MAX_FRAME_SIZE];
    float out_buffer[ADASHAPE_MAX_FRAME_SIZE];
    float tmp_buffer[ADASHAPE_MAX_FRAME_SIZE];
    int i, k;
    int tenv_size;
    float mean;
    float *tenv;

    celt_assert(frame_size % avg_pool_k == 0);
    celt_assert(feature_dim + frame_size / avg_pool_k + 1 < ADASHAPE_MAX_INPUT_DIM);

    tenv_size = frame_size / avg_pool_k;
    tenv = in_buffer + feature_dim;
    OPUS_CLEAR(tenv, tenv_size + 1);

    OPUS_COPY(in_buffer, features, feature_dim);

    /* calculate temporal envelope */
    mean = 0;
    for (i = 0; i < tenv_size; i++)
    {
        for (k = 0; k < avg_pool_k; k++)
        {
            tenv[i] += fabs(x_in[i * avg_pool_k + k]);
        }
        tenv[i] = log(tenv[i] / avg_pool_k + 1.52587890625e-05f);
        mean += tenv[i];
    }
    mean /= tenv_size;
    for (i = 0; i < tenv_size; i++)
    {
        tenv[i] -= mean;
    }
    tenv[tenv_size] = mean;
#ifdef DEBUG_NNDSP
    print_float_vector("tenv", tenv, tenv_size + 1);
#endif

    /* calculate temporal weights */
#ifdef DEBUG_NNDSP
    print_float_vector("alpha1_in", in_buffer, feature_dim + tenv_size + 1);
#endif
    compute_generic_conv1d(alpha1f, out_buffer, hAdaShape->conv_alpha1f_state, in_buffer, feature_dim, ACTIVATION_LINEAR, arch);
    compute_generic_conv1d(alpha1t, tmp_buffer, hAdaShape->conv_alpha1t_state, tenv, tenv_size + 1, ACTIVATION_LINEAR, arch);
#ifdef DEBUG_NNDSP
    print_float_vector("alpha1_out", out_buffer, frame_size);
#endif
    /* compute leaky ReLU by hand. ToDo: try tanh activation */
    for (i = 0; i < frame_size; i ++)
    {
        float tmp = out_buffer[i] + tmp_buffer[i];
        in_buffer[i] = tmp >= 0 ? tmp : 0.2 * tmp;
    }
#ifdef DEBUG_NNDSP
    print_float_vector("post_alpha1", in_buffer, frame_size);
#endif
    compute_generic_conv1d(alpha2, out_buffer, hAdaShape->conv_alpha2_state, in_buffer, frame_size, ACTIVATION_LINEAR, arch);

    /* shape signal */
    for (i = 0; i < frame_size; i ++)
    {
        x_out[i] = exp(out_buffer[i]) * x_in[i];
    }

}

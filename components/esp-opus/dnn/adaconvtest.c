#include "lace_data.h"
#include "nolace_data.h"
#include "osce.h"
#include "nndsp.h"


#include <stdlib.h>
#include <stdio.h>
#include <math.h>


extern const WeightArray lacelayers_arrays[];
extern const WeightArray nolacelayers_arrays[];

void adaconv_compare(
    const char * prefix,
    int num_frames,
    AdaConvState* hAdaConv,
    LinearLayer *kernel_layer,
    LinearLayer *gain_layer,
    int feature_dim,
    int frame_size,
    int overlap_size,
    int in_channels,
    int out_channels,
    int kernel_size,
    int left_padding,
    float filter_gain_a,
    float filter_gain_b,
    float shape_gain
)
{
    char feature_file[256];
    char x_in_file[256];
    char x_out_file[256];
    char message[512];
    int i_frame, i_sample;
    float mse;
    float features[512];
    float x_in[512];
    float x_out_ref[512];
    float x_out[512];
    float window[40];

    init_adaconv_state(hAdaConv);
    compute_overlap_window(window, 40);

    FILE *f_features, *f_x_in, *f_x_out;

    strcpy(feature_file, prefix);
    strcat(feature_file, "_features.f32");
    f_features = fopen(feature_file, "r");
    if (f_features == NULL)
    {
        sprintf(message, "could not open file %s", feature_file);
        perror(message);
        exit(1);
    }

    strcpy(x_in_file, prefix);
    strcat(x_in_file, "_x_in.f32");
    f_x_in = fopen(x_in_file, "r");
    if (f_x_in == NULL)
    {
        sprintf(message, "could not open file %s", x_in_file);
        perror(message);
        exit(1);
    }

    strcpy(x_out_file, prefix);
    strcat(x_out_file, "_x_out.f32");
    f_x_out = fopen(x_out_file, "r");
    if (f_x_out == NULL)
    {
        sprintf(message, "could not open file %s", x_out_file);
        perror(message);
        exit(1);
    }

    for (i_frame = 0; i_frame < num_frames; i_frame ++)
    {
        if (fread(features, sizeof(float), feature_dim, f_features) != feature_dim)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, feature_file);
            exit(1);
        }

        if (fread(x_in, sizeof(float), frame_size * in_channels, f_x_in) != frame_size * in_channels)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_in_file);
            exit(1);
        }

        if (fread(x_out_ref, sizeof(float), frame_size * out_channels, f_x_out) != frame_size * out_channels)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_out_file);
            exit(1);
        }

        adaconv_process_frame(hAdaConv, x_out, x_in, features, kernel_layer, gain_layer, feature_dim,
            frame_size, overlap_size, in_channels, out_channels, kernel_size, left_padding,
            filter_gain_a, filter_gain_b, shape_gain, window, 0);

        mse = 0;
        for (i_sample = 0; i_sample < frame_size * out_channels; i_sample ++)
        {
            mse += pow(x_out_ref[i_sample] - x_out[i_sample], 2);
        }
        mse = sqrt(mse / (frame_size * out_channels));
        printf("rmse[%d] %f\n", i_frame, mse);

    }
}


void adacomb_compare(
    const char * prefix,
    int num_frames,
    AdaCombState* hAdaComb,
    LinearLayer *kernel_layer,
    LinearLayer *gain_layer,
    LinearLayer *global_gain_layer,
    int feature_dim,
    int frame_size,
    int overlap_size,
    int kernel_size,
    int left_padding,
    float filter_gain_a,
    float filter_gain_b,
    float log_gain_limit
)
{
    char feature_file[256];
    char x_in_file[256];
    char p_in_file[256];
    char x_out_file[256];
    char message[512];
    int i_frame, i_sample;
    float mse;
    float features[512];
    float x_in[512];
    float x_out_ref[512];
    float x_out[512];
    int pitch_lag;
    float window[40];

    init_adacomb_state(hAdaComb);
    compute_overlap_window(window, 40);

    FILE *f_features, *f_x_in, *f_p_in, *f_x_out;

    strcpy(feature_file, prefix);
    strcat(feature_file, "_features.f32");
    f_features = fopen(feature_file, "r");
    if (f_features == NULL)
    {
        sprintf(message, "could not open file %s", feature_file);
        perror(message);
        exit(1);
    }

    strcpy(x_in_file, prefix);
    strcat(x_in_file, "_x_in.f32");
    f_x_in = fopen(x_in_file, "r");
    if (f_x_in == NULL)
    {
        sprintf(message, "could not open file %s", x_in_file);
        perror(message);
        exit(1);
    }

    strcpy(p_in_file, prefix);
    strcat(p_in_file, "_p_in.s32");
    f_p_in = fopen(p_in_file, "r");
    if (f_p_in == NULL)
    {
        sprintf(message, "could not open file %s", p_in_file);
        perror(message);
        exit(1);
    }

    strcpy(x_out_file, prefix);
    strcat(x_out_file, "_x_out.f32");
    f_x_out = fopen(x_out_file, "r");
    if (f_x_out == NULL)
    {
        sprintf(message, "could not open file %s", x_out_file);
        perror(message);
        exit(1);
    }

    for (i_frame = 0; i_frame < num_frames; i_frame ++)
    {
        if (fread(features, sizeof(float), feature_dim, f_features) != feature_dim)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, feature_file);
            exit(1);
        }

        if (fread(x_in, sizeof(float), frame_size, f_x_in) != frame_size)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_in_file);
            exit(1);
        }

        if (fread(&pitch_lag, sizeof(int), 1, f_p_in) != 1)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, p_in_file);
            exit(1);
        }

        if (fread(x_out_ref, sizeof(float), frame_size, f_x_out) != frame_size)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_out_file);
            exit(1);
        }

        adacomb_process_frame(hAdaComb, x_out, x_in, features, kernel_layer, gain_layer, global_gain_layer,
            pitch_lag, feature_dim, frame_size, overlap_size, kernel_size, left_padding, filter_gain_a, filter_gain_b, log_gain_limit, window, 0);


        mse = 0;
        for (i_sample = 0; i_sample < frame_size; i_sample ++)
        {
            mse += pow(x_out_ref[i_sample] - x_out[i_sample], 2);
        }
        mse = sqrt(mse / (frame_size));
        printf("rmse[%d] %f\n", i_frame, mse);

    }
}

void adashape_compare(
    const char * prefix,
    int num_frames,
    AdaShapeState* hAdaShape,
    LinearLayer *alpha1,
    LinearLayer *alpha2,
    int feature_dim,
    int frame_size,
    int avg_pool_k
)
{
    char feature_file[256];
    char x_in_file[256];
    char x_out_file[256];
    char message[512];
    int i_frame, i_sample;
    float mse;
    float features[512];
    float x_in[512];
    float x_out_ref[512];
    float x_out[512];

    init_adashape_state(hAdaShape);

    FILE *f_features, *f_x_in, *f_x_out;

    strcpy(feature_file, prefix);
    strcat(feature_file, "_features.f32");
    f_features = fopen(feature_file, "r");
    if (f_features == NULL)
    {
        sprintf(message, "could not open file %s", feature_file);
        perror(message);
        exit(1);
    }

    strcpy(x_in_file, prefix);
    strcat(x_in_file, "_x_in.f32");
    f_x_in = fopen(x_in_file, "r");
    if (f_x_in == NULL)
    {
        sprintf(message, "could not open file %s", x_in_file);
        perror(message);
        exit(1);
    }

    strcpy(x_out_file, prefix);
    strcat(x_out_file, "_x_out.f32");
    f_x_out = fopen(x_out_file, "r");
    if (f_x_out == NULL)
    {
        sprintf(message, "could not open file %s", x_out_file);
        perror(message);
        exit(1);
    }

    for (i_frame = 0; i_frame < num_frames; i_frame ++)
    {
        if (fread(features, sizeof(float), feature_dim, f_features) != feature_dim)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, feature_file);
            exit(1);
        }

        if (fread(x_in, sizeof(float), frame_size, f_x_in) != frame_size)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_in_file);
            exit(1);
        }

        if (fread(x_out_ref, sizeof(float), frame_size, f_x_out) != frame_size)
        {
            fprintf(stderr, "could not read frame %d from %s\n", i_frame, x_out_file);
            exit(1);
        }

        adashape_process_frame(hAdaShape, x_out, x_in, features, alpha1, alpha2, feature_dim,
            frame_size, avg_pool_k, 0);

        mse = 0;
        for (i_sample = 0; i_sample < frame_size; i_sample ++)
        {
            mse += pow(x_out_ref[i_sample] - x_out[i_sample], 2);
        }
        mse = sqrt(mse / (frame_size));
        printf("rmse[%d] %f\n", i_frame, mse);

    }
}


int main()
{
    LACELayers hLACE;
    NOLACELayers hNoLACE;

    AdaConvState hAdaConv;
    AdaCombState hAdaComb;
    AdaShapeState hAdaShape;

    init_adaconv_state(&hAdaConv);

    init_lacelayers(&hLACE, lacelayers_arrays);
    init_nolacelayers(&hNoLACE, nolacelayers_arrays);

    printf("\ntesting lace.af1 (1 in, 1 out)...\n");
    adaconv_compare(
        "testvectors/lace_af1",
        5,
        &hAdaConv,
        &hLACE.lace_af1_kernel,
        &hLACE.lace_af1_gain,
        LACE_AF1_FEATURE_DIM,
        LACE_AF1_FRAME_SIZE,
        LACE_AF1_OVERLAP_SIZE,
        LACE_AF1_IN_CHANNELS,
        LACE_AF1_OUT_CHANNELS,
        LACE_AF1_KERNEL_SIZE,
        LACE_AF1_LEFT_PADDING,
        LACE_AF1_FILTER_GAIN_A,
        LACE_AF1_FILTER_GAIN_B,
        LACE_AF1_SHAPE_GAIN
    );


    printf("\ntesting nolace.af1 (1 in, 2 out)...\n");
    adaconv_compare(
        "testvectors/nolace_af1",
        5,
        &hAdaConv,
        &hNoLACE.nolace_af1_kernel,
        &hNoLACE.nolace_af1_gain,
        NOLACE_AF1_FEATURE_DIM,
        NOLACE_AF1_FRAME_SIZE,
        NOLACE_AF1_OVERLAP_SIZE,
        NOLACE_AF1_IN_CHANNELS,
        NOLACE_AF1_OUT_CHANNELS,
        NOLACE_AF1_KERNEL_SIZE,
        NOLACE_AF1_LEFT_PADDING,
        NOLACE_AF1_FILTER_GAIN_A,
        NOLACE_AF1_FILTER_GAIN_B,
        NOLACE_AF1_SHAPE_GAIN
    );


    printf("testing nolace.af4 (2 in, 1 out)...\n");
    adaconv_compare(
        "testvectors/nolace_af4",
        5,
        &hAdaConv,
        &hNoLACE.nolace_af4_kernel,
        &hNoLACE.nolace_af4_gain,
        NOLACE_AF4_FEATURE_DIM,
        NOLACE_AF4_FRAME_SIZE,
        NOLACE_AF4_OVERLAP_SIZE,
        NOLACE_AF4_IN_CHANNELS,
        NOLACE_AF4_OUT_CHANNELS,
        NOLACE_AF4_KERNEL_SIZE,
        NOLACE_AF4_LEFT_PADDING,
        NOLACE_AF4_FILTER_GAIN_A,
        NOLACE_AF4_FILTER_GAIN_B,
        NOLACE_AF4_SHAPE_GAIN
    );

    printf("\ntesting nolace.af2 (2 in, 2 out)...\n");
    adaconv_compare(
        "testvectors/nolace_af2",
        5,
        &hAdaConv,
        &hNoLACE.nolace_af2_kernel,
        &hNoLACE.nolace_af2_gain,
        NOLACE_AF2_FEATURE_DIM,
        NOLACE_AF2_FRAME_SIZE,
        NOLACE_AF2_OVERLAP_SIZE,
        NOLACE_AF2_IN_CHANNELS,
        NOLACE_AF2_OUT_CHANNELS,
        NOLACE_AF2_KERNEL_SIZE,
        NOLACE_AF2_LEFT_PADDING,
        NOLACE_AF2_FILTER_GAIN_A,
        NOLACE_AF2_FILTER_GAIN_B,
        NOLACE_AF2_SHAPE_GAIN
    );

    printf("\ntesting lace.cf1...\n");
    adacomb_compare(
        "testvectors/lace_cf1",
        5,
        &hAdaComb,
        &hLACE.lace_cf1_kernel,
        &hLACE.lace_cf1_gain,
        &hLACE.lace_cf1_global_gain,
        LACE_CF1_FEATURE_DIM,
        LACE_CF1_FRAME_SIZE,
        LACE_CF1_OVERLAP_SIZE,
        LACE_CF1_KERNEL_SIZE,
        LACE_CF1_LEFT_PADDING,
        LACE_CF1_FILTER_GAIN_A,
        LACE_CF1_FILTER_GAIN_B,
        LACE_CF1_LOG_GAIN_LIMIT
    );

    printf("\ntesting nolace.tdshape1...\n");
    adashape_compare(
        "testvectors/nolace_tdshape1",
        5,
        &hAdaShape,
        &hNoLACE.nolace_tdshape1_alpha1,
        &hNoLACE.nolace_tdshape1_alpha2,
        NOLACE_TDSHAPE1_FEATURE_DIM,
        NOLACE_TDSHAPE1_FRAME_SIZE,
        NOLACE_TDSHAPE1_AVG_POOL_K
    );

    return 0;
}

/* gcc -DVAR_ARRAYS -DENABLE_OSCE  -I ../include -I ../silk -I . -I ../celt adaconvtest.c nndsp.c lace_data.c nolace_data.c nnet.c nnet_default.c ../celt/pitch.c ../celt/celt_lpc.c parse_lpcnet_weights.c -lm -o adaconvtest */
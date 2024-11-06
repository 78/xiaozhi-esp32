#include <stdio.h>
#include <math.h>
#include "opus_types.h"
#include "arch.h"
#include "common.h"
#include "tansig_table.h"

#define LPCNET_TEST

// we need to call two versions of each functions that have the same
// name, so use #defines to temp rename them

#define lpcnet_exp2 lpcnet_exp2_fast
#define tansig_approx tansig_approx_fast
#define sigmoid_approx sigmoid_approx_fast
#define softmax softmax_fast
#define vec_tanh vec_tanh_fast
#define vec_sigmoid vec_sigmoid_fast
#define sgemv_accum16 sgemv_accum16_fast
#define sparse_sgemv_accum16 sparse_sgemv_accum16_fast

#ifdef __AVX__
#include "vec_avx.h"
#ifdef __AVX2__
const char simd[]="AVX2";
#else
const char simd[]="AVX";
#endif
#elif __ARM_NEON__
#include "vec_neon.h"
const char simd[]="NEON";
#else
const char simd[]="none";

#endif

#undef lpcnet_exp2
#undef tansig_approx
#undef sigmoid_approx
#undef softmax
#undef vec_tanh
#undef vec_sigmoid
#undef sgemv_accum16
#undef sparse_sgemv_accum16
#include "vec.h"

#define ROW_STEP 16
#define ROWS     ROW_STEP*10
#define COLS     2
#define ENTRIES  2

int test_sgemv_accum16() {
    float weights[ROWS*COLS];
    float x[COLS];
    float out[ROWS], out_fast[ROWS];
    int i;

    printf("sgemv_accum16.....................: ");
    for(i=0; i<ROWS*COLS; i++) {
	weights[i] = i;
    }
    for(i=0; i<ROWS; i++) {
	out[i] = 0;
	out_fast[i] = 0;
    }

    for(i=0; i<COLS; i++) {
	x[i] = i+1;
    }

    sgemv_accum16(out, weights, ROWS, COLS, 1, x);
    sgemv_accum16_fast(out_fast, weights, ROWS, COLS, 1, x);

    for(i=0; i<ROWS; i++) {
	if (out[i] != out_fast[i]) {
	    printf("fail\n");
	    for(i=0; i<ROWS; i++) {
		printf("%d %f %f\n", i, out[i], out_fast[i]);
		if (out[i] != out_fast[i])
		    return 1;
	    }
	}
    }

    printf("pass\n");
    return 0;
}


int test_sparse_sgemv_accum16() {
    int rows = ROW_STEP*ENTRIES;
    int indx[] = {1,0,2,0,1};
    float w[ROW_STEP*(1+2)];
    float x[ENTRIES] = {1,2};
    float out[ROW_STEP*(1+2)], out_fast[ROW_STEP*(1+2)];
    int i;

    printf("sparse_sgemv_accum16..............: ");
    for(i=0; i<ROW_STEP*(1+2); i++) {
	w[i] = i;
	out[i] = 0;
	out_fast[i] = 0;
    }

    sparse_sgemv_accum16(out, w, rows, indx, x);
    sparse_sgemv_accum16_fast(out_fast, w, rows, indx, x);

    for(i=0; i<ROW_STEP*ENTRIES; i++) {
	if (out[i] != out_fast[i]) {
	    printf("fail\n");
	    for(i=0; i<ROW_STEP*ENTRIES; i++) {
		printf("%d %f %f\n", i, out[i], out_fast[i]);
		if (out[i] != out_fast[i])
		    return 1;
	    }
	}
    }

    printf("pass\n");
    return 0;
}

int main() {
    printf("testing vector routines on SIMD: %s\n", simd);
    int test1 = test_sgemv_accum16();
    int test2 = test_sparse_sgemv_accum16();
    return test1 || test2;
}

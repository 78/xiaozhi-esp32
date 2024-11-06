/***********************************************************************
Copyright (c) 2017 Google Inc., Jean-Marc Valin
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include "celt/stack_alloc.h"
#include "cpu_support.h"
#include "SigProc_FIX.h"

/* Computes the impulse response of the filter so we
   can catch filters that are definitely unstable. Some
   unstable filters may be classified as stable, but not
   the other way around. */
int check_stability(opus_int16 *A_Q12, int order) {
    int i;
    int j;
    int sum_a, sum_abs_a;
    double y[SILK_MAX_ORDER_LPC] = {0};
    sum_a = sum_abs_a = 0;
    for( j = 0; j < order; j++ ) {
        sum_a += A_Q12[ j ];
        sum_abs_a += silk_abs( A_Q12[ j ] );
    }
    /* Check DC stability. */
    if( sum_a >= 4096 ) {
        return 0;
    }
    /* If the sum of absolute values is less than 1, the filter
       has to be stable. */
    if( sum_abs_a < 4096 ) {
        return 1;
    }
    y[0] = 1;
    for( i = 0; i < 10000; i++ ) {
        double sum = 0;
        for( j = 0; j < order; j++ ) {
            sum += y[ j ]*A_Q12[ j ];
        }
        for( j = order - 1; j > 0; j-- ) {
            y[ j ] = y[ j - 1 ];
        }
        y[ 0 ] = sum*(1./4096);
        /* If impulse response reaches +/- 10000, the filter
           is definitely unstable. */
        if( !(y[ 0 ] < 10000 && y[ 0 ] > -10000) ) {
            return 0;
        }
        /* Test every 8 sample for low amplitude. */
        if( ( i & 0x7 ) == 0 ) {
            double amp = 0;
            for( j = 0; j < order; j++ ) {
                amp += fabs(y[j]);
            }
            if( amp < 0.00001 ) {
                return 1;
            }
        }
    }
    return 1;
}

int main(void) {
    const int arch = opus_select_arch();
    /* Set to 10000 so all branches in C function are triggered */
    const int loop_num = 10000;
    int count = 0;
    ALLOC_STACK;

    /* FIXME: Make the seed random (with option to set it explicitly)
       so we get wider coverage. */
    srand(0);

    printf("Testing silk_LPC_inverse_pred_gain() optimization ...\n");
    for( count = 0; count < loop_num; count++ ) {
        unsigned int i;
        opus_int     order;
        unsigned int shift;
        opus_int16   A_Q12[ SILK_MAX_ORDER_LPC ];
        opus_int32 gain;

        for( order = 2; order <= SILK_MAX_ORDER_LPC; order += 2 ) { /* order must be even. */
            for( shift = 0; shift < 16; shift++ ) { /* Different dynamic range. */
                for( i = 0; i < SILK_MAX_ORDER_LPC; i++ ) {
                    A_Q12[i] = ((opus_int16)rand()) >> shift;
                }
                gain = silk_LPC_inverse_pred_gain(A_Q12, order, arch);
                /* Look for filters that silk_LPC_inverse_pred_gain() thinks are
                   stable but definitely aren't. */
                if( gain != 0 && !check_stability(A_Q12, order) ) {
                    fprintf(stderr, "**Loop %4d failed!**\n", count);
                    return 1;
                }
            }
        }
        if( !(count % 500) ) {
            printf("Loop %4d passed\n", count);
        }
    }
    printf("silk_LPC_inverse_pred_gain() optimization passed\n");
    RESTORE_STACK;
    return 0;
}

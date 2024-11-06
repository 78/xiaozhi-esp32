/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
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

#include <math.h>
#include <string.h>
#include <assert.h>

#include "arch.h"
#include "burg.h"

#define MAX_FRAME_SIZE              384 /* subfr_length * nb_subfr = ( 0.005 * 16000 + 16 ) * 4 = 384*/
#define SILK_MAX_ORDER_LPC          16
#define FIND_LPC_COND_FAC           1e-5f

/* sum of squares of a silk_float array, with result as double */
static double silk_energy_FLP(
    const float    *data,
    int            dataSize
)
{
    int i;
    double   result;

    /* 4x unrolled loop */
    result = 0.0;
    for( i = 0; i < dataSize - 3; i += 4 ) {
        result += data[ i + 0 ] * (double)data[ i + 0 ] +
                  data[ i + 1 ] * (double)data[ i + 1 ] +
                  data[ i + 2 ] * (double)data[ i + 2 ] +
                  data[ i + 3 ] * (double)data[ i + 3 ];
    }

    /* add any remaining products */
    for( ; i < dataSize; i++ ) {
        result += data[ i ] * (double)data[ i ];
    }

    assert( result >= 0.0 );
    return result;
}

/* inner product of two silk_float arrays, with result as double */
static double silk_inner_product_FLP(
    const float    *data1,
    const float    *data2,
    int            dataSize
)
{
    int i;
    double   result;

    /* 4x unrolled loop */
    result = 0.0;
    for( i = 0; i < dataSize - 3; i += 4 ) {
        result += data1[ i + 0 ] * (double)data2[ i + 0 ] +
                  data1[ i + 1 ] * (double)data2[ i + 1 ] +
                  data1[ i + 2 ] * (double)data2[ i + 2 ] +
                  data1[ i + 3 ] * (double)data2[ i + 3 ];
    }

    /* add any remaining products */
    for( ; i < dataSize; i++ ) {
        result += data1[ i ] * (double)data2[ i ];
    }

    return result;
}


/* Compute reflection coefficients from input signal */
float silk_burg_analysis(              /* O    returns residual energy                                     */
    float          A[],                /* O    prediction coefficients (length order)                      */
    const float    x[],                /* I    input signal, length: nb_subfr*(D+L_sub)                    */
    const float    minInvGain,         /* I    minimum inverse prediction gain                             */
    const int      subfr_length,       /* I    input signal subframe length (incl. D preceding samples)    */
    const int      nb_subfr,           /* I    number of subframes stacked in x                            */
    const int      D                   /* I    order                                                       */
)
{
    int         k, n, s, reached_max_gain;
    double           C0, invGain, num, nrg_f, nrg_b, rc, Atmp, tmp1, tmp2;
    const float *x_ptr;
    double           C_first_row[ SILK_MAX_ORDER_LPC ], C_last_row[ SILK_MAX_ORDER_LPC ];
    double           CAf[ SILK_MAX_ORDER_LPC + 1 ], CAb[ SILK_MAX_ORDER_LPC + 1 ];
    double           Af[ SILK_MAX_ORDER_LPC ];

    assert( subfr_length * nb_subfr <= MAX_FRAME_SIZE );

    /* Compute autocorrelations, added over subframes */
    C0 = silk_energy_FLP( x, nb_subfr * subfr_length );
    memset( C_first_row, 0, SILK_MAX_ORDER_LPC * sizeof( double ) );
    for( s = 0; s < nb_subfr; s++ ) {
        x_ptr = x + s * subfr_length;
        for( n = 1; n < D + 1; n++ ) {
            C_first_row[ n - 1 ] += silk_inner_product_FLP( x_ptr, x_ptr + n, subfr_length - n );
        }
    }
    memcpy( C_last_row, C_first_row, SILK_MAX_ORDER_LPC * sizeof( double ) );

    /* Initialize */
    CAb[ 0 ] = CAf[ 0 ] = C0 + FIND_LPC_COND_FAC * C0 + 1e-9f;
    invGain = 1.0f;
    reached_max_gain = 0;
    for( n = 0; n < D; n++ ) {
        /* Update first row of correlation matrix (without first element) */
        /* Update last row of correlation matrix (without last element, stored in reversed order) */
        /* Update C * Af */
        /* Update C * flipud(Af) (stored in reversed order) */
        for( s = 0; s < nb_subfr; s++ ) {
            x_ptr = x + s * subfr_length;
            tmp1 = x_ptr[ n ];
            tmp2 = x_ptr[ subfr_length - n - 1 ];
            for( k = 0; k < n; k++ ) {
                C_first_row[ k ] -= x_ptr[ n ] * x_ptr[ n - k - 1 ];
                C_last_row[ k ]  -= x_ptr[ subfr_length - n - 1 ] * x_ptr[ subfr_length - n + k ];
                Atmp = Af[ k ];
                tmp1 += x_ptr[ n - k - 1 ] * Atmp;
                tmp2 += x_ptr[ subfr_length - n + k ] * Atmp;
            }
            for( k = 0; k <= n; k++ ) {
                CAf[ k ] -= tmp1 * x_ptr[ n - k ];
                CAb[ k ] -= tmp2 * x_ptr[ subfr_length - n + k - 1 ];
            }
        }
        tmp1 = C_first_row[ n ];
        tmp2 = C_last_row[ n ];
        for( k = 0; k < n; k++ ) {
            Atmp = Af[ k ];
            tmp1 += C_last_row[  n - k - 1 ] * Atmp;
            tmp2 += C_first_row[ n - k - 1 ] * Atmp;
        }
        CAf[ n + 1 ] = tmp1;
        CAb[ n + 1 ] = tmp2;

        /* Calculate nominator and denominator for the next order reflection (parcor) coefficient */
        num = CAb[ n + 1 ];
        nrg_b = CAb[ 0 ];
        nrg_f = CAf[ 0 ];
        for( k = 0; k < n; k++ ) {
            Atmp = Af[ k ];
            num   += CAb[ n - k ] * Atmp;
            nrg_b += CAb[ k + 1 ] * Atmp;
            nrg_f += CAf[ k + 1 ] * Atmp;
        }
        assert( nrg_f > 0.0 );
        assert( nrg_b > 0.0 );

        /* Calculate the next order reflection (parcor) coefficient */
        rc = -2.0 * num / ( nrg_f + nrg_b );
        assert( rc > -1.0 && rc < 1.0 );

        /* Update inverse prediction gain */
        tmp1 = invGain * ( 1.0 - rc * rc );
        if( tmp1 <= minInvGain ) {
            /* Max prediction gain exceeded; set reflection coefficient such that max prediction gain is exactly hit */
            rc = sqrt( 1.0 - minInvGain / invGain );
            if( num > 0 ) {
                /* Ensure adjusted reflection coefficients has the original sign */
                rc = -rc;
            }
            invGain = minInvGain;
            reached_max_gain = 1;
        } else {
            invGain = tmp1;
        }

        /* Update the AR coefficients */
        for( k = 0; k < (n + 1) >> 1; k++ ) {
            tmp1 = Af[ k ];
            tmp2 = Af[ n - k - 1 ];
            Af[ k ]         = tmp1 + rc * tmp2;
            Af[ n - k - 1 ] = tmp2 + rc * tmp1;
        }
        Af[ n ] = rc;

        if( reached_max_gain ) {
            /* Reached max prediction gain; set remaining coefficients to zero and exit loop */
            for( k = n + 1; k < D; k++ ) {
                Af[ k ] = 0.0;
            }
            break;
        }

        /* Update C * Af and C * Ab */
        for( k = 0; k <= n + 1; k++ ) {
            tmp1 = CAf[ k ];
            CAf[ k ]          += rc * CAb[ n - k + 1 ];
            CAb[ n - k + 1  ] += rc * tmp1;
        }
    }

    if( reached_max_gain ) {
        /* Convert to float */
        for( k = 0; k < D; k++ ) {
            A[ k ] = (float)( -Af[ k ] );
        }
        /* Subtract energy of preceding samples from C0 */
        for( s = 0; s < nb_subfr; s++ ) {
            C0 -= silk_energy_FLP( x + s * subfr_length, D );
        }
        /* Approximate residual energy */
        nrg_f = C0 * invGain;
    } else {
        /* Compute residual energy and store coefficients as float */
        nrg_f = CAf[ 0 ];
        tmp1 = 1.0;
        for( k = 0; k < D; k++ ) {
            Atmp = Af[ k ];
            nrg_f += CAf[ k + 1 ] * Atmp;
            tmp1  += Atmp * Atmp;
            A[ k ] = (float)(-Atmp);
        }
        nrg_f -= FIND_LPC_COND_FAC * C0 * tmp1;
    }

    /* Return residual energy */
    return MAX32(0, (float)nrg_f);
}

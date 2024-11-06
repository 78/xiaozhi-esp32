#ifndef _SILK_RESAMPLER_H_
#define _SILK_RESAMPLER_H_

#include "opus.h"
#include "resampler_structs.h"
/*!
 * Initialize/reset the resampler state for a given pair of input/output sampling rates
*/
extern "C" opus_int silk_resampler_init(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int32                  Fs_Hz_in,           /* I    Input sampling rate (Hz)                                    */
    opus_int32                  Fs_Hz_out,          /* I    Output sampling rate (Hz)                                   */
    opus_int                    forEnc              /* I    If 1: encoder; if 0: decoder                                */
);

/*!
 * Resampler: convert from one sampling rate to another
 */
extern "C" opus_int silk_resampler(
    silk_resampler_state_struct *S,                 /* I/O  Resampler state                                             */
    opus_int16                  out[],              /* O    Output signal                                               */
    const opus_int16            in[],               /* I    Input signal                                                */
    opus_int32                  inLen               /* I    Number of input samples                                     */
);

#endif // _SILK_RESAMPLER_H_

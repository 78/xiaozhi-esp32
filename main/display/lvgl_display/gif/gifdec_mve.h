/**
 * @file gifdec_mve.h
 *
 */

#ifndef GIFDEC_MVE_H
#define GIFDEC_MVE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include "../../misc/lv_color.h"

/*********************
 *      DEFINES
 *********************/

#define GIFDEC_FILL_BG(dst, w, h, stride, color, opa) \
    _gifdec_fill_bg_mve(dst, w, h, stride, color, opa)

#define GIFDEC_RENDER_FRAME(dst, w, h, stride, frame, pattern, tindex) \
    _gifdec_render_frame_mve(dst, w, h, stride, frame, pattern, tindex)

/**********************
 *      MACROS
 **********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

static inline void _gifdec_fill_bg_mve(uint8_t * dst, uint16_t w, uint16_t h, uint16_t stride, uint8_t * color,
                                       uint8_t opa)
{
    lv_color32_t c = lv_color32_make(*(color + 0), *(color + 1), *(color + 2), opa);
    uint32_t color_32 = *(uint32_t *)&c;

    __asm volatile(
        ".p2align 2                                             \n"
        "vdup.32             q0, %[src]                         \n"
        "3:                                                     \n"
        "mov                 r0, %[dst]                         \n"

        "wlstp.32            lr, %[w], 1f                       \n"
        "2:                                                     \n"

        "vstrw.32            q0, [r0], #16                      \n"
        "letp                lr, 2b                             \n"
        "1:                                                     \n"
        "add                 %[dst], %[iTargetStride]           \n"
        "subs                %[h], #1                           \n"
        "bne                 3b                                 \n"
        : [dst] "+r"(dst),
        [h] "+r"(h)
        : [src] "r"(color_32),
        [w] "r"(w),
        [iTargetStride] "r"(stride * sizeof(uint32_t))
        : "r0", "q0", "memory", "r14", "cc");
}

static inline void _gifdec_render_frame_mve(uint8_t * dst, uint16_t w, uint16_t h, uint16_t stride, uint8_t * frame,
                                            uint8_t * pattern, uint16_t tindex)
{
    if(w == 0 || h == 0) {
        return;
    }

    __asm volatile(
        "vmov.u16       q3, #255                                \n"
        "vshl.u16       q3, q3, #8                              \n" /* left shift 8 for a*/

        "mov            r0, #2                                  \n"
        "vidup.u16      q6, r0, #4                              \n" /* [2, 6, 10, 14, 18, 22, 26, 30] */
        "mov            r0, #0                                  \n"
        "vidup.u16      q7, r0, #4                              \n" /* [0, 4, 8, 12, 16, 20, 24, 28] */

        "3:                                                     \n"
        "mov            r1, %[dst]                              \n"
        "mov            r2, %[frame]                            \n"

        "wlstp.16       lr, %[w], 1f                            \n"
        "2:                                                     \n"

        "mov            r0, #3                                  \n"
        "vldrb.u16      q4, [r2], #8                            \n"
        "vmul.u16       q5, q4, r0                              \n"

        "mov            r0, #1                                  \n"
        "vldrb.u16      q2, [%[pattern], q5]                    \n" /* load 8 pixel r*/

        "vadd.u16       q5, q5, r0                              \n"
        "vldrb.u16      q1, [%[pattern], q5]                    \n" /* load 8 pixel g*/

        "vadd.u16       q5, q5, r0                              \n"
        "vldrb.u16      q0, [%[pattern], q5]                    \n" /* load 8 pixel b*/

        "vshl.u16       q1, q1, #8                              \n" /* left shift 8 for g*/

        "vorr.u16       q0, q0, q1                              \n" /* make 8 pixel gb*/
        "vorr.u16       q1, q2, q3                              \n" /* make 8 pixel ar*/

        "vcmp.i16       ne, q4, %[tindex]                       \n"
        "vpstt                                                  \n"
        "vstrht.16      q0, [r1, q7]                            \n"
        "vstrht.16      q1, [r1, q6]                            \n"
        "add            r1, r1, #32                             \n"

        "letp           lr, 2b                                  \n"

        "1:                                                     \n"
        "mov            r0, %[stride], LSL #2                   \n"
        "add            %[dst], r0                              \n"
        "add            %[frame], %[stride]                     \n"
        "subs           %[h], #1                                \n"
        "bne            3b                                      \n"

        : [dst] "+r"(dst),
        [frame] "+r"(frame),
        [h] "+r"(h)
        : [pattern] "r"(pattern),
        [w] "r"(w),
        [stride] "r"(stride),
        [tindex] "r"(tindex)
        : "r0", "r1", "r2", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "memory", "r14", "cc");
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*GIFDEC_MVE_H*/

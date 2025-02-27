/*
 * RFC 1521 base64 encoding/decoding
 *
 * Copyright (C) 2018, Nordic Semiconductor ASA
 * Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Adapted for Zephyr by Carles Cufi (carles.cufi@nordicsemi.no)
 *  - Removed mbedtls_ prefixes
 *  - Reworked coding style
 */

 #include <stdint.h>
 #include <errno.h>
 #include "base64.h"
 
 static const uint8_t base64_enc_map[64] = {
   'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
   'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
   'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
   'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
   'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
   'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
   '8', '9', '+', '/'
 };
 
 static const uint8_t base64_dec_map[128] = {
   127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
   127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
   127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
   127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
   127, 127, 127,	62, 127, 127, 127,  63,  52,  53,
    54,  55,  56,	57,  58,  59,  60,  61, 127, 127,
   127,  64, 127, 127, 127,   0,	1,   2,   3,   4,
     5,   6,   7,	 8,   9,  10,  11,  12,  13,  14,
    15,  16,  17,	18,  19,  20,  21,  22,  23,  24,
    25, 127, 127, 127, 127, 127, 127,  26,  27,  28,
    29,  30,  31,	32,  33,  34,  35,  36,  37,  38,
    39,  40,  41,	42,  43,  44,  45,  46,  47,  48,
    49,  50,  51, 127, 127, 127, 127, 127
 };
 
 #define BASE64_SIZE_T_MAX	((size_t) -1) /* SIZE_T_MAX is not standard */
 
 /*
  * Encode a buffer into base64 format
  */
 int base64_encode(uint8_t *dst, size_t dlen, size_t *olen, const uint8_t *src,
       size_t slen)
 {
   size_t i, n;
   int C1, C2, C3;
   uint8_t *p;
 
   if (slen == 0) {
     *olen = 0;
     return 0;
   }
 
   n = slen / 3 + (slen % 3 != 0);
 
   if (n > (BASE64_SIZE_T_MAX - 1) / 4) {
     *olen = BASE64_SIZE_T_MAX;
     return -ENOMEM;
   }
 
   n *= 4;
 
   if ((dlen < n + 1) || (!dst)) {
     *olen = n + 1;
     return -ENOMEM;
   }
 
   n = (slen / 3) * 3;
 
   for (i = 0, p = dst; i < n; i += 3) {
     C1 = *src++;
     C2 = *src++;
     C3 = *src++;
 
     *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
     *p++ = base64_enc_map[(((C1 &  3) << 4) + (C2 >> 4)) & 0x3F];
     *p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
     *p++ = base64_enc_map[C3 & 0x3F];
   }
 
   if (i < slen) {
     C1 = *src++;
     C2 = ((i + 1) < slen) ? *src++ : 0;
 
     *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
     *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
 
     if ((i + 1) < slen) {
       *p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
     } else {
       *p++ = '=';
     }
 
     *p++ = '=';
   }
 
   *olen = p - dst;
   *p = 0U;
 
   return 0;
 }
 
 /*
  * Decode a base64-formatted buffer
  */
 int base64_decode(uint8_t *dst, size_t dlen, size_t *olen, const uint8_t *src,
       size_t slen)
 {
   size_t i, n;
   uint32_t j, x;
   uint8_t *p;
 
   /* First pass: check for validity and get output length */
   for (i = n = j = 0U; i < slen; i++) {
     /* Skip spaces before checking for EOL */
     x = 0U;
     while (i < slen && src[i] == ' ') {
       ++i;
       ++x;
     }
 
     /* Spaces at end of buffer are OK */
     if (i == slen) {
       break;
     }
 
     if ((slen - i) >= 2 && src[i] == '\r' && src[i + 1] == '\n') {
       continue;
     }
 
     if (src[i] == '\n') {
       continue;
     }
 
     /* Space inside a line is an error */
     if (x != 0U) {
       return -EINVAL;
     }
 
     if (src[i] == '=' && ++j > 2) {
       return -EINVAL;
     }
 
     if (src[i] > 127 || base64_dec_map[src[i]] == 127U) {
       return -EINVAL;
     }
 
     if (base64_dec_map[src[i]] < 64 && j != 0U) {
       return -EINVAL;
     }
 
     n++;
   }
 
   if (n == 0) {
     *olen = 0;
     return 0;
   }
 
   /* The following expression is to calculate the following formula
    * without risk of integer overflow in n:
    *	   n = ( ( n * 6 ) + 7 ) >> 3;
    */
   n = (6 * (n >> 3)) + ((6 * (n & 0x7) + 7) >> 3);
   n -= j;
 
   if (dst == NULL || dlen < n) {
     *olen = n;
     return -ENOMEM;
   }
 
   for (j = 3U, n = x = 0U, p = dst; i > 0; i--, src++) {
 
     if (*src == '\r' || *src == '\n' || *src == ' ') {
       continue;
     }
 
     j -= (base64_dec_map[*src] == 64U);
     x  = (x << 6) | (base64_dec_map[*src] & 0x3F);
 
     if (++n == 4) {
       n = 0;
       if (j > 0) {
         *p++ = (unsigned char)(x >> 16);
       }
       if (j > 1) {
         *p++ = (unsigned char)(x >> 8);
       }
       if (j > 2) {
         *p++ = (unsigned char)(x);
       }
     }
   }
 
   *olen = p - dst;
 
   return 0;
 }
 
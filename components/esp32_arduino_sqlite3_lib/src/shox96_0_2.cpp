/*
 * Copyright (C) 2019 Siara Logics (cc)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @author Arundale R.
 *
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "shox96_0_2.h"

typedef unsigned char byte;

static unsigned int  c_95[95] = {16384, 16256, 15744, 16192, 15328, 15344, 15360, 16064, 15264, 15296, 15712, 15200, 14976, 15040, 14848, 15104, 14528, 14592, 14656, 14688, 14720, 14752, 14784, 14816, 14832, 14464, 15552, 15488, 15616, 15168, 15680, 16000, 15872, 10752,  8576,  8192,  8320,  9728,  8672,  8608,  8384, 11264,  9024,  8992, 12160,  8544, 11520, 11008,  8512,  9008, 12032, 11776, 10240,  8448,  8960,  8640,  9040,  8688,  9048, 15840, 16288, 15856, 16128, 16224, 16368, 40960,  6144,     0,  2048, 24576,  7680,  6656,  3072, 49152, 13312, 12800, 63488,  5632, 53248, 45056,  5120, 13056, 61440, 57344, 32768,  4096, 12288,  7168, 13568,  7936, 13696, 15776, 16320, 15808, 16352};
static unsigned char l_95[95] = {    3,    11,    11,    11,    12,    12,     9,    10,    11,    11,    11,    11,    10,    10,     9,    10,    10,    10,    11,    11,    11,    11,    11,    12,    12,    10,    10,    10,    10,    11,    11,    10,     9,     8,    11,     9,    10,     7,    12,    11,    10,     8,    12,    12,     9,    11,     8,     8,    11,    12,     9,     8,     7,    10,    11,    11,    13,    12,    13,    12,    11,    12,    10,    11,    12,     4,     7,     5,     6,     3,     8,     7,     6,     4,     8,     8,     5,     7,     4,     4,     7,     8,     5,     4,     3,     6,     7,     7,     9,     8,     9,    11,    11,    11,    12};
//unsigned char c[]    = {  ' ',   '!',   '"',   '#',   '$',   '%',   '&',  '\'',   '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/',   '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',   '8',   '9',   ':',   ';',   '<',   '=',   '>',   '?',   '@',   'A',   'B',   'C',   'D',   'E',   'F',   'G',   'H',   'I',   'J',   'K',   'L',   'M',   'N',   'O',   'P',   'Q',   'R',   'S',   'T',   'U',   'V',   'W',   'X',   'Y',   'Z',   '[',  '\\',   ']',   '^',   '_',   '`',   'a',   'b',   'c',   'd',   'e',   'f',   'g',   'h',   'i',   'j',   'k',   'l',   'm',   'n',   'o',   'p',   'q',   'r',   's',   't',   'u',   'v',   'w',   'x',   'y',   'z',   '{',   '|',   '}',   '~'};
static char SET2_STR[] = {'9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '.', ',', '-', '/', '=', '+', ' ', '(', ')', '$', '%', '&', ';', ':', '<', '>', '*', '"', '{', '}', '[', ']', '@', '?', '\'', '^', '#', '_', '!', '\\', '|', '~', '`', '\0'};

enum {SHX_STATE_1 = 1, SHX_STATE_2};

byte to_match_repeats_earlier = 1;
byte to_match_repeats_within = 1;
#define USE_64K_LOOKUP 0
#if USE_64K_LOOKUP == 1
byte lookup[65536];
#endif
#define NICE_LEN_FOR_PRIOR 7
#define NICE_LEN_FOR_OTHER 12

unsigned int mask[] = {0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00};
int append_bits(char *out, int ol, unsigned int code, int clen, byte state) {

   byte cur_bit;
   byte blen;
   unsigned char a_byte;

   if (state == SHX_STATE_2) {
      // remove change state prefix
      if ((code >> 9) == 0x1C) {
         code <<= 7;
         clen -= 7;
      }
      //if (code == 14272 && clen == 10) {
      //   code = 9084;
      //   clen = 14;
      //}
   }
   while (clen > 0) {
     cur_bit = ol % 8;
     blen = (clen > 8 ? 8 : clen);
     a_byte = (code & mask[blen - 1]) >> 8;
     a_byte >>= cur_bit;
     if (blen + cur_bit > 8)
        blen = (8 - cur_bit);
     if (cur_bit == 0)
        out[ol / 8] = a_byte;
     else
        out[ol / 8] |= a_byte;
     code <<= blen;
     ol += blen;
     clen -= blen;
   }
   return ol;
}

int encodeCount(char *out, int ol, int count) {
  const byte codes[7] = {0x01, 0x82, 0xC3, 0xE5, 0xED, 0xF5, 0xFD};
  const byte bit_len[7] =  {2, 5,  7,   9,  12,   16,  17};
  const uint16_t adder[7] = {0, 4, 36, 164, 676, 4772,  0};
  int till = 0;
  for (int i = 0; i < 6; i++) {
    till += (1 << bit_len[i]);
    if (count < till) {
      ol = append_bits(out, ol, (codes[i] & 0xF8) << 8, codes[i] & 0x07, 1);
      ol = append_bits(out, ol, (count - adder[i]) << (16 - bit_len[i]), bit_len[i], 1);
      return ol;
    }
  }
  return ol;
}

int matchOccurance(const char *in, int len, int l, char *out, int *ol) {
  int j, k;
  for (j = 0; j < l; j++) {
    for (k = j; k < l && (l + k - j) < len; k++) {
      if (in[k] != in[l + k - j])
        break;
    }
    if ((k - j) > (NICE_LEN_FOR_PRIOR - 1)) {
      *ol = append_bits(out, *ol, 14144, 10, 1);
      *ol = encodeCount(out, *ol, k - j - NICE_LEN_FOR_PRIOR); // len
      *ol = encodeCount(out, *ol, l - j - NICE_LEN_FOR_PRIOR + 1); // dist
      l += (k - j);
      l--;
      return l;
    }
  }
  return -l;
}

int matchLine(const char *in, int len, int l, char *out, int *ol, struct lnk_lst *prev_lines) {
  int last_ol = *ol;
  int last_len = 0;
  int last_dist = 0;
  int last_ctx = 0;
  int line_ctr = 0;
  do {
    int i, j, k;
    int line_len = strlen(prev_lines->data);
    for (j = 0; j < line_len; j++) {
      for (i = l, k = j; k < line_len && i < len; k++, i++) {
        if (prev_lines->data[k] != in[i])
          break;
      }
      if ((k - j) >= NICE_LEN_FOR_OTHER) {
        if (last_len) {
          if (j > last_dist)
            continue;
          //int saving = ((k - j) - last_len) + (last_dist - j) + (last_ctx - line_ctr);
          //if (saving < 0) {
          //  //printf("No savng: %d\n", saving);
          //  continue;
          //}
          *ol = last_ol;
        }
        last_len = (k - j);
        last_dist = j;
        last_ctx = line_ctr;
        *ol = append_bits(out, *ol, 14080, 10, 1);
        *ol = encodeCount(out, *ol, last_len - NICE_LEN_FOR_OTHER);
        *ol = encodeCount(out, *ol, last_dist);
        *ol = encodeCount(out, *ol, last_ctx);
        /*
        if ((*ol - last_ol) > (last_len * 4)) {
          last_len = 0;
          *ol = last_ol;
        }*/
        //printf("Len: %d, Dist: %d, Line: %d\n", last_len, last_dist, last_ctx);
      }
    }
    line_ctr++;
    prev_lines = prev_lines->previous;
  } while (prev_lines && prev_lines->data != NULL);
  if (last_len) {
    l += last_len;
    l--;
    return l;
  }
  return -l;
}

int shox96_0_2_compress(const char *in, int len, char *out, struct lnk_lst *prev_lines) {

  char *ptr;
  byte bits;
  byte state;

  int l, ll, ol;
  char c_in, c_next, c_prev;
  byte is_upper, is_all_upper;

  ol = 0;
  c_prev = 0;
#if USE_64K_LOOKUP == 1
  memset(lookup, 0, sizeof(lookup));
#endif
  state = SHX_STATE_1;
  is_all_upper = 0;
  for (l=0; l<len; l++) {

    c_in = in[l];

    if (l < len - 4) {
      if (c_in == c_prev && c_in == in[l + 1] && c_in == in[l + 2] && c_in == in[l + 3]) {
        int rpt_count = l + 4;
        while (rpt_count < len && in[rpt_count] == c_in)
          rpt_count++;
        rpt_count -= l;
        ol = append_bits(out, ol, 14208, 10, 1);
        ol = encodeCount(out, ol, rpt_count - 4);
        l += rpt_count;
        l--;
        continue;
      }
    }

    if (l < (len - NICE_LEN_FOR_PRIOR) && to_match_repeats_within) {
#if USE_64K_LOOKUP == 1
        uint16_t to_lookup = c_in ^ in[l + 1] + ((in[l + 2] ^ in[l + 3]) << 8);
        if (lookup[to_lookup]) {
#endif
          l = matchOccurance(in, len, l, out, &ol);
          if (l > 0) {
            c_prev = in[l - 1];
            continue;
          }
          l = -l;
#if USE_64K_LOOKUP == 1
        } else
          lookup[to_lookup] = 1;
#endif
    }
    if (l < (len - NICE_LEN_FOR_OTHER) && to_match_repeats_earlier) {
        if (prev_lines != NULL) {
          l = matchLine(in, len, l, out, &ol, prev_lines);
          if (l > 0) {
            c_prev = in[l - 1];
            continue;
          }
          l = -l;
        }
    }
    if (state == SHX_STATE_2) {
      if (c_in == ' ' && len - 1 > l)
        ptr = (char *) memchr(SET2_STR, in[l+1], 42);
      else
        ptr = (char *) memchr(SET2_STR, c_in, 42);
      if (ptr == NULL) {
        state = SHX_STATE_1;
        ol = append_bits(out, ol, 8192, 4, 1);
      }
    }
    is_upper = 0;
    if (c_in >= 'A' && c_in <= 'Z')
      is_upper = 1;
    else {
      if (is_all_upper) {
        is_all_upper = 0;
        ol = append_bits(out, ol, 8192, 4, state);
      }
    }
    if (is_upper && !is_all_upper) {
      for (ll=l+5; ll>=l && ll<len; ll--) {
        if (in[ll] >= 'a' && in[ll] <= 'z')
          break;
      }
      if (ll == l-1) {
        ol = append_bits(out, ol, 8704, 8, state);
        is_all_upper = 1;
      }
    }
    if (state == SHX_STATE_1 && c_in >= '0' && c_in <= '9') {
      ol = append_bits(out, ol, 14336, 7, state);
      state = SHX_STATE_2;
    }
    c_next = 0;
    if (l+1 < len)
      c_next = in[l+1];

    c_prev = c_in;
    if (c_in >= 32 && c_in <= 126) {
      c_in -= 32;
      if (is_all_upper && is_upper)
        c_in += 32;
      if (c_in == 0 && state == SHX_STATE_2)
        ol = append_bits(out, ol, 15232, 11, state);
      else
        ol = append_bits(out, ol, c_95[c_in], l_95[c_in], state);
    } else
    if (c_in == 13 && c_next == 10) {
      ol = append_bits(out, ol, 13824, 9, state);
      l++;
      c_prev = 10;
    } else
    if (c_in == 10) {
      ol = append_bits(out, ol, 13952, 9, state);
    } else
    if (c_in == 13) {
      ol = append_bits(out, ol, 9064, 13, state);
    } else
    if (c_in == '\t') {
      ol = append_bits(out, ol, 9216, 7, state);
    }
  }
  bits = ol % 8;
  if (bits) {
    ol = append_bits(out, ol, 14272, 8 - bits, 1);
  }
  //printf("\n%ld\n", ol);
  return ol/8+(ol%8?1:0);

}

// Decoder is designed for using less memory, not speed
// Decode lookup table for code index and length
// First 2 bits 00, Next 3 bits indicate index of code from 0,
// last 3 bits indicate code length in bits
//                0,            1,            2,            3,            4,
char vcode[32] = {2 + (0 << 3), 3 + (3 << 3), 3 + (1 << 3), 4 + (6 << 3), 0,
//                5,            6,            7,            8, 9, 10
                  4 + (4 << 3), 3 + (2 << 3), 4 + (8 << 3), 0, 0,  0,
//                11,          12, 13,            14, 15
                  4 + (7 << 3), 0,  4 + (5 << 3),  0,  5 + (9 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0,  0,  0,  0,  0,  0,  0,  0,
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (10 << 3)};
//                0,            1,            2, 3,            4, 5, 6, 7,
char hcode[32] = {1 + (1 << 3), 2 + (0 << 3), 0, 3 + (2 << 3), 0, 0, 0, 5 + (3 << 3),
//                8, 9, 10, 11, 12, 13, 14, 15,
                  0, 0,  0,  0,  0,  0,  0,  5 + (5 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0, 0,  0,  0,  0,  0,  0,  5 + (4 << 3),
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (6 << 3)};

enum {SHX_SET1 = 0, SHX_SET1A, SHX_SET1B, SHX_SET2, SHX_SET3, SHX_SET4, SHX_SET4A};
char sets[][11] = {{' ', ' ', 'e', 't', 'a', 'o', 'i', 'n', 's', 'r', 'l'},
                   {'c', 'd', 'h', 'u', 'p', 'm', 'b', 'g', 'w', 'f', 'y'},
                   {'v', 'k', 'q', 'j', 'x', 'z', ' ', ' ', ' ', ' ', ' '},
                   {' ', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8'},
                   {'.', ',', '-', '/', '=', '+', ' ', '(', ')', '$', '%'},
                   {'&', ';', ':', '<', '>', '*', '"', '{', '}', '[', ']'},
                   {'@', '?', '\'', '^', '#', '_', '!', '\\', '|', '~', '`'}};

int getBitVal(const char *in, int bit_no, int count) {
   return (in[bit_no >> 3] & (0x80 >> (bit_no % 8)) ? 1 << count : 0);
}

int getCodeIdx(char *code_type, const char *in, int len, int *bit_no_p) {
  int code = 0;
  int count = 0;
  do {
    if (*bit_no_p >= len)
      return 199;
    code += getBitVal(in, *bit_no_p, count);
    (*bit_no_p)++;
    count++;
    if (code_type[code] &&
        (code_type[code] & 0x07) == count) {
      return code_type[code] >> 3;
    }
  } while (count < 5);
  return 1; // skip if code not found
}

int getNumFromBits(const char *in, int bit_no, int count) {
   int ret = 0;
   while (count--) {
     ret += getBitVal(in, bit_no++, count);
   }
   return ret;
}

int readCount(const char *in, int *bit_no_p, int len) {
  const byte bit_len[7]   = {5, 2,  7,   9,  12,   16, 17};
  const uint16_t adder[7] = {4, 0, 36, 164, 676, 4772,  0};
  int idx = getCodeIdx(hcode, in, len, bit_no_p);
  if (idx > 6)
    return 0;
  int count = getNumFromBits(in, *bit_no_p, bit_len[idx]) + adder[idx];
  (*bit_no_p) += bit_len[idx];
  return count;
}

int shox96_0_2_decompress(const char *in, int len, char *out, struct lnk_lst *prev_lines) {

  int dstate;
  int bit_no;
  byte is_all_upper;
  int ol = 0;
  bit_no = 0;
  dstate = SHX_SET1;
  is_all_upper = 0;

  len <<= 3;
  out[ol] = 0;
  while (bit_no < len) {
    int h, v;
    char c;
    byte is_upper = is_all_upper;
    int orig_bit_no = bit_no;
    v = getCodeIdx(vcode, in, len, &bit_no);
    if (v == 199) {
      bit_no = orig_bit_no;
      break;
    }
    h = dstate;
    if (v == 0) {
      h = getCodeIdx(hcode, in, len, &bit_no);
      if (h == 199) {
        bit_no = orig_bit_no;
        break;
      }
      if (h == SHX_SET1) {
         if (dstate == SHX_SET1) {
           if (is_all_upper) {
             is_upper = is_all_upper = 0;
             continue;
           }
           v = getCodeIdx(vcode, in, len, &bit_no);
           if (v == 199) {
             bit_no = orig_bit_no;
             break;
           }
           if (v == 0) {
              h = getCodeIdx(hcode, in, len, &bit_no);
              if (h == 199) {
                bit_no = orig_bit_no;
                break;
              }
              if (h == SHX_SET1) {
                 is_all_upper = 1;
                 continue;
              }
           }
           is_upper = 1;
         } else {
            dstate = SHX_SET1;
            continue;
         }
      } else
      if (h == SHX_SET2) {
         if (dstate == SHX_SET1)
           dstate = SHX_SET2;
         continue;
      }
      if (h != SHX_SET1) {
        v = getCodeIdx(vcode, in, len, &bit_no);
        if (v == 199) {
          bit_no = orig_bit_no;
          break;
        }
      }
    }
    if (h < 64 && v < 32)
      c = sets[h][v];
    if (c >= 'a' && c <= 'z') {
      if (is_upper)
        c -= 32;
    } else {
      if (is_upper && dstate == SHX_SET1 && v == 1)
        c = '\t';
      if (h == SHX_SET1B) {
         switch (v) {
           case 6:
             out[ol++] = '\r';
             c = '\n';
             break;
           case 7:
             c = is_upper ? '\r' : '\n';
             break;
           case 8:
             if (getBitVal(in, bit_no++, 0)) {
               int dict_len = readCount(in, &bit_no, len) + NICE_LEN_FOR_PRIOR;
               int dist = readCount(in, &bit_no, len) + NICE_LEN_FOR_PRIOR - 1;
               memcpy(out + ol, out + ol - dist, dict_len);
               ol += dict_len;
             } else {
               int dict_len = readCount(in, &bit_no, len) + NICE_LEN_FOR_OTHER;
               int dist = readCount(in, &bit_no, len);
               int ctx = readCount(in, &bit_no, len);
               struct lnk_lst *cur_line = prev_lines;
               while (ctx--)
                 cur_line = cur_line->previous;
               memmove(out + ol, cur_line->data + dist, dict_len);
               ol += dict_len;
             }
             continue;
           case 9: {
             int count = readCount(in, &bit_no, len);
             count += 4;
             char rpt_c = out[ol - 1];
             while (count--)
               out[ol++] = rpt_c;
             continue;
           }
           case 10:
             continue;
         }
      }
    }
    out[ol++] = c;
  }

  return ol;

}
#pragma GCC diagnostic pop

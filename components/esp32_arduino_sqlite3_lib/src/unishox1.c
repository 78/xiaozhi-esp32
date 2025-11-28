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

#define UNISHOX_VERSION "1.0"

#ifdef _MSC_VER
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "unishox1.h"

typedef unsigned char byte;

enum {SHX_SET1 = 0, SHX_SET1A, SHX_SET1B, SHX_SET2, SHX_SET3, SHX_SET4, SHX_SET4A};
char us_vcodes[] =     {0, 2, 3, 4, 10, 11, 12, 13, 14, 30, 31};
char us_vcode_lens[] = {2, 3, 3, 3,  4,  4,  4,  4,  4,  5,  5};
char us_sets[][11] = {{  0, ' ', 'e',   0, 't', 'a', 'o', 'i', 'n', 's', 'r'},
                   {  0, 'l', 'c', 'd', 'h', 'u', 'p', 'm', 'b', 'g', 'w'},
                   {'f', 'y', 'v', 'k', 'q', 'j', 'x', 'z',   0,   0,   0},
                   {  0, '9', '0', '1', '2', '3', '4', '5', '6', '7', '8'},
                   {'.', ',', '-', '/', '=', '+', ' ', '(', ')', '$', '%'},
                   {'&', ';', ':', '<', '>', '*', '"', '{', '}', '[', ']'},
                   {'@', '?', '\'', '^', '#', '_', '!', '\\', '|', '~', '`'}};

unsigned int  c_95[95]; // = {16384, 16256, 15744, 16192, 15328, 15344, 15360, 16064, 15264, 15296, 15712, 15200, 14976, 15040, 14848, 15104, 14528, 14592, 14656, 14688, 14720, 14752, 14784, 14816, 14832, 14464, 15552, 15488, 15616, 15168, 15680, 16000, 15872, 10752,  8576,  8192,  8320,  9728,  8672,  8608,  8384, 11264,  9024,  8992, 12160,  8544, 11520, 11008,  8512,  9008, 12032, 11776, 10240,  8448,  8960,  8640,  9040,  8688,  9048, 15840, 16288, 15856, 16128, 16224, 16368, 40960,  6144,     0,  2048, 24576,  7680,  6656,  3072, 49152, 13312, 12800, 63488,  5632, 53248, 45056,  5120, 13056, 61440, 57344, 32768,  4096, 12288,  7168, 13568,  7936, 13696, 15776, 16320, 15808, 16352};
unsigned char l_95[95]; // = {    3,    11,    11,    11,    12,    12,     9,    10,    11,    11,    11,    11,    10,    10,     9,    10,    10,    10,    11,    11,    11,    11,    11,    12,    12,    10,    10,    10,    10,    11,    11,    10,     9,     8,    11,     9,    10,     7,    12,    11,    10,     8,    12,    12,     9,    11,     8,     8,    11,    12,     9,     8,     7,    10,    11,    11,    13,    12,    13,    12,    11,    12,    10,    11,    12,     4,     7,     5,     6,     3,     8,     7,     6,     4,     8,     8,     5,     7,     4,     4,     7,     8,     5,     4,     3,     6,     7,     7,     9,     8,     9,    11,    11,    11,    12};
//unsigned char c[]    = {  ' ',   '!',   '"',   '#',   '$',   '%',   '&',  '\'',   '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/',   '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',   '8',   '9',   ':',   ';',   '<',   '=',   '>',   '?',   '@',   'A',   'B',   'C',   'D',   'E',   'F',   'G',   'H',   'I',   'J',   'K',   'L',   'M',   'N',   'O',   'P',   'Q',   'R',   'S',   'T',   'U',   'V',   'W',   'X',   'Y',   'Z',   '[',  '\\',   ']',   '^',   '_',   '`',   'a',   'b',   'c',   'd',   'e',   'f',   'g',   'h',   'i',   'j',   'k',   'l',   'm',   'n',   'o',   'p',   'q',   'r',   's',   't',   'u',   'v',   'w',   'x',   'y',   'z',   '{',   '|',   '}',   '~'};

const int UTF8_MASK[] = {0xE0, 0xF0, 0xF8};
const int UTF8_PREFIX[] = {0xC0, 0xE0, 0xF0};

enum {SHX_STATE_1 = 1, SHX_STATE_2, SHX_STATE_UNI};

byte to_match_repeats = 1;
#define USE_64K_LOOKUP 0
#if USE_64K_LOOKUP == 1
byte lookup[65536];
#endif
#define NICE_LEN 5

#define TERM_CODE 0x37C0
#define TERM_CODE_LEN 10
#define DICT_CODE 0x0000
#define DICT_CODE_LEN 5
#define DICT_OTHER_CODE 0x0000 // not used
#define DICT_OTHER_CODE_LEN 6
#define RPT_CODE 0x2370
#define RPT_CODE_LEN 13
#define BACK2_STATE1_CODE 0x2000
#define BACK2_STATE1_CODE_LEN 4
#define BACK_FROM_UNI_CODE 0xFE00
#define BACK_FROM_UNI_CODE_LEN 8
#define CRLF_CODE 0x3780
#define CRLF_CODE_LEN 10
#define LF_CODE 0x3700
#define LF_CODE_LEN 9
#define TAB_CODE 0x2400
#define TAB_CODE_LEN 7
#define UNI_CODE 0x8000
#define UNI_CODE_LEN 3
#define UNI_STATE_SPL_CODE 0xF800
#define UNI_STATE_SPL_CODE_LEN 5
#define UNI_STATE_DICT_CODE 0xFC00
#define UNI_STATE_DICT_CODE_LEN 7
#define CONT_UNI_CODE 0x2800
#define CONT_UNI_CODE_LEN 7
#define ALL_UPPER_CODE 0x2200
#define ALL_UPPER_CODE_LEN 8
#define SW2_STATE2_CODE 0x3800
#define SW2_STATE2_CODE_LEN 7
#define ST2_SPC_CODE 0x3B80
#define ST2_SPC_CODE_LEN 11
#define BIN_CODE 0x2000
#define BIN_CODE_LEN 9

//void checkPreus_vcodes(char c, int prev_code, char prev_code_len, int c_95, char l_95) {
//   if (prev_code != c_95 || prev_code_len != l_95) {
//     printf("Code mismatch: %d: %d!=%d, %d!=%d\n", c, prev_code, c_95, prev_code_len, l_95);
//   }
//}

byte is_inited = 0;
void init_coder() {
  if (is_inited)
    return;
  for (int i = 0; i < 7; i++) {
    for (int j = 0; j < 11; j++) {
      char c = us_sets[i][j];
      if (c != 0 && c != 32) {
        int ascii = c - 32;
        //int prev_code = c_95[ascii];
        //int prev_code_len = l_95[ascii];
        switch (i) {
          case SHX_SET1: // just us_vcode
            c_95[ascii] = (us_vcodes[j] << (16 - us_vcode_lens[j]));
            l_95[ascii] = us_vcode_lens[j];
            //checkPreus_vcodes(c, prev_code, prev_code_len, c_95[ascii], l_95[ascii]);
            if (c >= 'a' && c <= 'z') {
              ascii -= ('a' - 'A');
              //prev_code = c_95[ascii];
              //prev_code_len = l_95[ascii];
              c_95[ascii] = (2 << 12) + (us_vcodes[j] << (12 - us_vcode_lens[j]));
              l_95[ascii] = 4 + us_vcode_lens[j];
            }
            break;
          case SHX_SET1A: // 000 + us_vcode
            c_95[ascii] = 0 + (us_vcodes[j] << (13 - us_vcode_lens[j]));
            l_95[ascii] = 3 + us_vcode_lens[j];
            //checkPreus_vcodes(c, prev_code, prev_code_len, c_95[ascii], l_95[ascii]);
            if (c >= 'a' && c <= 'z') {
              ascii -= ('a' - 'A');
              //prev_code = c_95[ascii];
              //prev_code_len = l_95[ascii];
              c_95[ascii] = (2 << 12) + 0 + (us_vcodes[j] << (9 - us_vcode_lens[j]));
              l_95[ascii] = 4 + 3 + us_vcode_lens[j];
            }
            break;
          case SHX_SET1B: // 00110 + us_vcode
            c_95[ascii] = (6 << 11) + (us_vcodes[j] << (11 - us_vcode_lens[j]));
            l_95[ascii] = 5 + us_vcode_lens[j];
            //checkPreus_vcodes(c, prev_code, prev_code_len, c_95[ascii], l_95[ascii]);
            if (c >= 'a' && c <= 'z') {
              ascii -= ('a' - 'A');
              //prev_code = c_95[ascii];
              //prev_code_len = l_95[ascii];
              c_95[ascii] = (2 << 12) + (6 << 7) + (us_vcodes[j] << (7 - us_vcode_lens[j]));
              l_95[ascii] = 4 + 5 + us_vcode_lens[j];
            }
            break;
          case SHX_SET2: // 0011100 + us_vcode
            c_95[ascii] = (28 << 9) + (us_vcodes[j] << (9 - us_vcode_lens[j]));
            l_95[ascii] = 7 + us_vcode_lens[j];
            break;
          case SHX_SET3: // 0011101 + us_vcode
            c_95[ascii] = (29 << 9) + (us_vcodes[j] << (9 - us_vcode_lens[j]));
            l_95[ascii] = 7 + us_vcode_lens[j];
            break;
          case SHX_SET4: // 0011110 + us_vcode
            c_95[ascii] = (30 << 9) + (us_vcodes[j] << (9 - us_vcode_lens[j]));
            l_95[ascii] = 7 + us_vcode_lens[j];
            break;
          case SHX_SET4A: // 0011111 + us_vcode
            c_95[ascii] = (31 << 9) + (us_vcodes[j] << (9 - us_vcode_lens[j]));
            l_95[ascii] = 7 + us_vcode_lens[j];
        }
        //checkPreus_vcodes(c, prev_code, prev_code_len, c_95[ascii], l_95[ascii]);
      }
    }
  }
  c_95[0] = 16384;
  l_95[0] = 3;

  is_inited = 1;
}

unsigned int us_mask[] = {0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00};
int append_bits(char *out, int ol, unsigned int code, int clen, byte state) {

   byte cur_bit;
   byte blen;
   unsigned char a_byte;

   //printf("%d,%x,%d,%d\n", ol, code, clen, state);

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
     a_byte = (code & us_mask[blen - 1]) >> 8;
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
  // First five bits are code and Last three bits of codes represent length
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

//const byte uni_bit_len[4]   = {5, 12, 15, 23};
//const int32_t uni_adder[4] = {0, 32, 4128, 36896};
//const byte uni_bit_len[4]   = {6, 12, 15, 23};
//const int32_t uni_adder[4] = {0, 64, 4160, 36928};
//const byte uni_bit_len[4]   = {6, 12, 14, 21};
//const int32_t uni_adder[4] = {0, 64, 4160, 20544};
//const byte uni_bit_len[7]   = {0, 3, 6, 12, 14, 16, 21};
//const int32_t uni_adder[7] = {0, 1, 9, 73, 4169, 20553, 86089};
const byte uni_bit_len[5]   = {6, 12, 14, 16, 21};
const int32_t uni_adder[5] = {0, 64, 4160, 20544, 86080};

int encodeUnicode(char *out, int ol, int32_t code, int32_t prev_code) {
  uint16_t spl_code = (code == ',' ? 0xE000 : 
    ((code == '.' || code == 0x3002) ? 0xE800 : (code == ' ' ? 0 :
    (code == 13 ? 0xF000 : (code == 10 ? 0xF800 : 0xFFFF)))));
  if (spl_code != 0xFFFF) {
    uint16_t spl_code_len = (code == ',' ? 5 : 
      ((code == '.' || code == 0x3002) ? 5 : (code == ' ' ? 1 :
      (code == 13 ? 5 : (code == 10 ? 5 : 0xFFFF)))));
    ol = append_bits(out, ol, UNI_STATE_SPL_CODE, UNI_STATE_SPL_CODE_LEN, SHX_STATE_UNI);
    ol = append_bits(out, ol, spl_code, spl_code_len, 1);
    return ol;
  }
  // First five bits are code and Last three bits of codes represent length
  //const byte codes[8] = {0x00, 0x42, 0x83, 0xA3, 0xC3, 0xE4, 0xF5, 0xFD};
  const byte codes[6] = {0x01, 0x82, 0xC3, 0xE4, 0xF5, 0xFD};
  int32_t till = 0;
  int orig_ol = ol;
  for (int i = 0; i < 5; i++) {
    till += (1 << uni_bit_len[i]);
    int32_t diff = abs(code - prev_code);
    if (diff < till) {
      ol = append_bits(out, ol, (codes[i] & 0xF8) << 8, codes[i] & 0x07, 1);
      //if (diff) {
        ol = append_bits(out, ol, prev_code > code ? 0x8000 : 0, 1, 1);
        if (uni_bit_len[i] > 16) {
          int32_t val = diff - uni_adder[i];
          int excess_bits = uni_bit_len[i] - 16;
          ol = append_bits(out, ol, val >> excess_bits, 16, 1);
          ol = append_bits(out, ol, (val & ((1 << excess_bits) - 1)) << (16 - excess_bits), excess_bits, 1);
        } else
          ol = append_bits(out, ol, (diff - uni_adder[i]) << (16 - uni_bit_len[i]), uni_bit_len[i], 1);
      //}
      //printf("bits:%d\n", ol-orig_ol);
      return ol;
    }
  }
  return ol;
}

int readUTF8(const char *in, int len, int l, int *utf8len) {
  int bc = 0;
  int uni = 0;
  byte c_in = in[l];
  for (; bc < 3; bc++) {
    if (UTF8_PREFIX[bc] == (c_in & UTF8_MASK[bc]) && len - (bc + 1) > l) {
      int j = 0;
      uni = c_in & ~UTF8_MASK[bc] & 0xFF;
      do {
        uni <<= 6;
        uni += (in[l + j + 1] & 0x3F);
      } while (j++ < bc);
      break;
    }
  }
  if (bc < 3) {
    *utf8len = bc + 1;
    return uni;
  }
  return 0;
}

int matchOccurance(const char *in, int len, int l, char *out, int *ol, byte *state, byte *is_all_upper) {
  int j, k;
  int longest_dist = 0;
  int longest_len = 0;
  for (j = l - NICE_LEN; j >= 0; j--) {
    for (k = l; k < len && j + k - l < l; k++) {
      if (in[k] != in[j + k - l])
        break;
    }
    while ((((unsigned char) in[k]) >> 6) == 2)
      k--; // Skip partial UTF-8 matches
    //if ((in[k - 1] >> 3) == 0x1E || (in[k - 1] >> 4) == 0x0E || (in[k - 1] >> 5) == 0x06)
    //  k--;
    if (k - l > NICE_LEN - 1) {
      int match_len = k - l - NICE_LEN;
      int match_dist = l - j - NICE_LEN + 1;
      if (match_len > longest_len) {
        longest_len = match_len;
        longest_dist = match_dist;
      }
    }
  }
  if (longest_len) {
    if (*state == SHX_STATE_2 || *is_all_upper) {
      *is_all_upper = 0;
      *state = SHX_STATE_1;
      *ol = append_bits(out, *ol, BACK2_STATE1_CODE, BACK2_STATE1_CODE_LEN, *state);
    }
    if (*state == SHX_STATE_UNI)
      *ol = append_bits(out, *ol, UNI_STATE_DICT_CODE, UNI_STATE_DICT_CODE_LEN, SHX_STATE_UNI);
    else
      *ol = append_bits(out, *ol, DICT_CODE, DICT_CODE_LEN, 1);
    //printf("Len:%d / Dist:%d\n", longest_len, longest_dist);
    *ol = encodeCount(out, *ol, longest_len);
    *ol = encodeCount(out, *ol, longest_dist);
    l += (longest_len + NICE_LEN);
    l--;
    return l;
  }
  return -l;
}

int matchLine(const char *in, int len, int l, char *out, int *ol, struct us_lnk_lst *prev_lines, byte *state, byte *is_all_upper) {
  int last_ol = *ol;
  int last_len = 0;
  int last_dist = 0;
  int last_ctx = 0;
  int line_ctr = 0;
  int j = 0;
  do {
    int i, k;
    int line_len = strlen(prev_lines->data);
    int limit = (line_ctr == 0 ? l : line_len);
    for (; j < limit; j++) {
      for (i = l, k = j; k < line_len && i < len; k++, i++) {
        if (prev_lines->data[k] != in[i])
          break;
      }
      while ((((unsigned char) prev_lines->data[k]) >> 6) == 2)
        k--; // Skip partial UTF-8 matches
      if ((k - j) >= NICE_LEN) {
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
        if (*state == SHX_STATE_2 || *is_all_upper) {
          *is_all_upper = 0;
          *state = SHX_STATE_1;
          *ol = append_bits(out, *ol, BACK2_STATE1_CODE, BACK2_STATE1_CODE_LEN, *state);
        }
        if (*state == SHX_STATE_UNI)
          *ol = append_bits(out, *ol, UNI_STATE_DICT_CODE, UNI_STATE_DICT_CODE_LEN, SHX_STATE_UNI);
        else
          *ol = append_bits(out, *ol, DICT_CODE, DICT_CODE_LEN, 1);
        *ol = encodeCount(out, *ol, last_len - NICE_LEN);
        *ol = encodeCount(out, *ol, last_dist);
        *ol = encodeCount(out, *ol, last_ctx);
        /*
        if ((*ol - last_ol) > (last_len * 4)) {
          last_len = 0;
          *ol = last_ol;
        }*/
        printf("Len: %d, Dist: %d, Line: %d\n", last_len, last_dist, last_ctx);
        j += last_len;
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

int unishox1_compress(const char *in, int len, char *out, struct us_lnk_lst *prev_lines) {

  char *ptr;
  byte bits;
  byte state;

  int l, ll, ol;
  char c_in, c_next;
  int prev_uni;
  byte is_upper, is_all_upper;

  init_coder();
  ol = 0;
  prev_uni = 0;
#if USE_64K_LOOKUP == 1
  memset(lookup, 0, sizeof(lookup));
#endif
  state = SHX_STATE_1;
  is_all_upper = 0;
  for (l=0; l<len; l++) {

    c_in = in[l];

    if (state != SHX_STATE_UNI && l && l < len - 4) {
      if (c_in == in[l - 1] && c_in == in[l + 1] && c_in == in[l + 2] && c_in == in[l + 3]) {
        int rpt_count = l + 4;
        while (rpt_count < len && in[rpt_count] == c_in)
          rpt_count++;
        rpt_count -= l;
        if (state == SHX_STATE_2 || is_all_upper) {
          is_all_upper = 0;
          state = SHX_STATE_1;
          ol = append_bits(out, ol, BACK2_STATE1_CODE, BACK2_STATE1_CODE_LEN, state);
        }
        ol = append_bits(out, ol, RPT_CODE, RPT_CODE_LEN, 1);
        ol = encodeCount(out, ol, rpt_count - 4);
        l += rpt_count;
        l--;
        continue;
      }
    }

    if (to_match_repeats && l < (len - NICE_LEN + 1)) {
      if (prev_lines) {
        l = matchLine(in, len, l, out, &ol, prev_lines, &state, &is_all_upper);
        if (l > 0) {
          continue;
        }
        l = -l;
      } else {
    #if USE_64K_LOOKUP == 1
        uint16_t to_lookup = c_in ^ in[l + 1] + ((in[l + 2] ^ in[l + 3]) << 8);
        if (lookup[to_lookup]) {
    #endif
          l = matchOccurance(in, len, l, out, &ol, &state, &is_all_upper);
          if (l > 0) {
            continue;
          }
          l = -l;
    #if USE_64K_LOOKUP == 1
        } else
          lookup[to_lookup] = 1;
    #endif
      }
    }

    if (state == SHX_STATE_UNI && (c_in == '.' || c_in == ' ' 
                 || c_in == ',' || c_in == 13 || c_in == 10)) {
      ol = encodeUnicode(out, ol, c_in, prev_uni);
      continue;
    }

    if (state == SHX_STATE_2) {
      if ((c_in >= ' ' && c_in <= '@') ||
          (c_in >= '[' && c_in <= '`') ||
          (c_in >= '{' && c_in <= '~')) {
      } else {
        state = SHX_STATE_1;
        ol = append_bits(out, ol, BACK2_STATE1_CODE, BACK2_STATE1_CODE_LEN, state);
      }
    }
    if (state == SHX_STATE_UNI && c_in >= 0 && c_in <= 127) {
      ol = append_bits(out, ol, BACK_FROM_UNI_CODE, BACK_FROM_UNI_CODE_LEN, state);
      state = SHX_STATE_1;
    }
    is_upper = 0;
    if (c_in >= 'A' && c_in <= 'Z')
      is_upper = 1;
    else {
      if (is_all_upper) {
        is_all_upper = 0;
        ol = append_bits(out, ol, BACK2_STATE1_CODE, BACK2_STATE1_CODE_LEN, state);
      }
    }
    c_next = 0;
    if (l+1 < len)
      c_next = in[l+1];

    if (c_in >= 32 && c_in <= 126) {
      if (is_upper && !is_all_upper) {
        for (ll=l+5; ll>=l && ll<len; ll--) {
          if (in[ll] < 'A' || in[ll] > 'Z')
            break;
        }
        if (ll == l-1) {
          ol = append_bits(out, ol, ALL_UPPER_CODE, ALL_UPPER_CODE_LEN, state);
          is_all_upper = 1;
        }
      }
      if (state == SHX_STATE_1 && c_in >= '0' && c_in <= '9') {
        ol = append_bits(out, ol, SW2_STATE2_CODE, SW2_STATE2_CODE_LEN, state);
        state = SHX_STATE_2;
      }
      c_in -= 32;
      if (is_all_upper && is_upper)
        c_in += 32;
      if (c_in == 0 && state == SHX_STATE_2)
        ol = append_bits(out, ol, ST2_SPC_CODE, ST2_SPC_CODE_LEN, state);
      else
        ol = append_bits(out, ol, c_95[c_in], l_95[c_in], state);
    } else
    if (c_in == 13 && c_next == 10) {
      ol = append_bits(out, ol, CRLF_CODE, CRLF_CODE_LEN, state);
      l++;
    } else
    if (c_in == 10) {
      ol = append_bits(out, ol, LF_CODE, LF_CODE_LEN, state);
    } else
    if (c_in == '\t') {
      ol = append_bits(out, ol, TAB_CODE, TAB_CODE_LEN, state);
    } else {
      int utf8len;
      int uni = readUTF8(in, len, l, &utf8len);
      if (uni) {
        l += utf8len;
        if (state != SHX_STATE_UNI) {
          int uni2 = readUTF8(in, len, l + 1, &utf8len);
          if (uni2) {
            state = SHX_STATE_UNI;
            ol = append_bits(out, ol, CONT_UNI_CODE, CONT_UNI_CODE_LEN, 1);
          } else {
            ol = append_bits(out, ol, UNI_CODE, UNI_CODE_LEN, 1);
          }
        }
        ol = encodeUnicode(out, ol, uni, prev_uni);
        //printf("%d:%d:%d,", l, utf8len, uni);
        if (uni != 0x3002)
          prev_uni = uni;
      } else {
        if (state == SHX_STATE_UNI) {
          state = SHX_STATE_1;
          ol = append_bits(out, ol, BACK_FROM_UNI_CODE, BACK_FROM_UNI_CODE_LEN, state);
        }
        printf("Bin:%d:%x\n", (unsigned char) c_in, (unsigned char) c_in);
        ol = append_bits(out, ol, BIN_CODE, BIN_CODE_LEN, state);
        ol = encodeCount(out, ol, (unsigned char) c_in);
      }
    }
  }
  if (state == SHX_STATE_UNI) {
    ol = append_bits(out, ol, BACK_FROM_UNI_CODE, BACK_FROM_UNI_CODE_LEN, state);
  }
  bits = ol % 8;
  if (bits) {
    ol = append_bits(out, ol, TERM_CODE, 8 - bits, 1);
  }
  //printf("\n%ld\n", ol);
  return ol/8+(ol%8?1:0);

}

// Decoder is designed for using less memory, not speed
// Decode lookup table for code index and length
// First 2 bits 00, Next 3 bits indicate index of code from 0,
// last 3 bits indicate code length in bits
//                0,            1,            2,            3,            4,
char us_vcode[32] = {2 + (0 << 3), 3 + (3 << 3), 3 + (1 << 3), 4 + (6 << 3), 0,
//                5,            6,            7,            8, 9, 10
                  4 + (4 << 3), 3 + (2 << 3), 4 + (8 << 3), 0, 0,  0,
//                11,          12, 13,            14, 15
                  4 + (7 << 3), 0,  4 + (5 << 3),  0,  5 + (9 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0,  0,  0,  0,  0,  0,  0,  0,
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (10 << 3)};
//                0,            1,            2, 3,            4, 5, 6, 7,
char us_hcode[32] = {1 + (1 << 3), 2 + (0 << 3), 0, 3 + (2 << 3), 0, 0, 0, 5 + (3 << 3),
//                8, 9, 10, 11, 12, 13, 14, 15,
                  0, 0,  0,  0,  0,  0,  0,  5 + (5 << 3),
//                16, 17, 18, 19, 20, 21, 22, 23
                   0, 0,  0,  0,  0,  0,  0,  5 + (4 << 3),
//                24, 25, 26, 27, 28, 29, 30, 31
                   0, 0,  0,  0,  0,  0,  0,  5 + (6 << 3)};

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

int32_t getNumFromBits(const char *in, int bit_no, int count) {
   int32_t ret = 0;
   while (count--) {
     ret += getBitVal(in, bit_no, count);
     bit_no++;
   }
   return ret;
}

int readCount(const char *in, int *bit_no_p, int len) {
  const byte bit_len[7]   = {5, 2,  7,   9,  12,   16, 17};
  const uint16_t adder[7] = {4, 0, 36, 164, 676, 4772,  0};
  int idx = getCodeIdx(us_hcode, in, len, bit_no_p);
  if (idx > 6)
    return 0;
  int count = getNumFromBits(in, *bit_no_p, bit_len[idx]) + adder[idx];
  (*bit_no_p) += bit_len[idx];
  return count;
}

int32_t readUnicode(const char *in, int *bit_no_p, int len) {
  int code = 0;
  for (int i = 0; i < 5; i++) {
    code += getBitVal(in, *bit_no_p, i);
    (*bit_no_p)++;
    //int idx = (code == 0 && i == 1 ? 0 : (code == 2 && i == 1 ? 1 : 
    //            (code == 1 && i == 2 ? 2 : (code == 5 && i == 2 ? 3 :
    //            (code == 3 && i == 2 ? 4 : (code == 7 && i == 3 ? 5 :
    //            (code == 15 && i == 4 ? 6 : 
    //            (code == 31 && i == 4 ? 7 : -1))))))));
    int idx = (code == 0 && i == 0 ? 0 : (code == 1 && i == 1 ? 1 : 
                (code == 3 && i == 2 ? 2 : (code == 7 && i == 3 ? 3 :
                (code == 15 && i == 4 ? 4 : 
                (code == 31 && i == 4 ? 5 : -1))))));
    //printf("%d\n", code);
    //if (idx == 0)
    //  return 0;
    if (idx == 5) {
      int idx = getCodeIdx(us_hcode, in, len, bit_no_p);
      return 0x7FFFFF00 + idx;
    }
    if (idx >= 0) {
      int sign = getBitVal(in, *bit_no_p, 1);
      (*bit_no_p)++;
      int32_t count = getNumFromBits(in, *bit_no_p, uni_bit_len[idx]);
      count += uni_adder[idx];
      (*bit_no_p) += uni_bit_len[idx];
      return sign ? -count : count;
    }
  }
  return 0;
}

void writeUTF8(char *out, int *ol, int uni) {
  if (uni < (1 << 11)) {
    out[(*ol)++] = (0xC0 + (uni >> 6));
    out[(*ol)++] = (0x80 + (uni & 63));
  } else
  if (uni < (1 << 16)) {
    out[(*ol)++] = (0xE0 + (uni >> 12));
    out[(*ol)++] = (0x80 + ((uni >> 6) & 63));
    out[(*ol)++] = (0x80 + (uni & 63));
  } else {
    out[(*ol)++] = (0xF0 + (uni >> 18));
    out[(*ol)++] = (0x80 + ((uni >> 12) & 63));
    out[(*ol)++] = (0x80 + ((uni >> 6) & 63));
    out[(*ol)++] = (0x80 + (uni & 63));
  }
}

int decodeRepeat(const char *in, int len, char *out, int ol, int *bit_no, struct us_lnk_lst *prev_lines) {
  if (prev_lines) {
    int dict_len = readCount(in, bit_no, len) + NICE_LEN;
    int dist = readCount(in, bit_no, len);
    int ctx = readCount(in, bit_no, len);
    struct us_lnk_lst *cur_line = prev_lines;
    while (ctx--)
      cur_line = cur_line->previous;
    memmove(out + ol, cur_line->data + dist, dict_len);
    ol += dict_len;
  } else {
    int dict_len = readCount(in, bit_no, len) + NICE_LEN;
    int dist = readCount(in, bit_no, len) + NICE_LEN - 1;
    memcpy(out + ol, out + ol - dist, dict_len);
    ol += dict_len;
  }
  return ol;
}

int unishox1_decompress(const char *in, int len, char *out, struct us_lnk_lst *prev_lines) {

  int dstate;
  int bit_no;
  byte is_all_upper;

  init_coder();
  int ol = 0;
  bit_no = 0;
  dstate = SHX_SET1;
  is_all_upper = 0;

  int prev_uni = 0;

  len <<= 3;
  out[ol] = 0;
  while (bit_no < len) {
    int h, v;
    char c = 0;
    byte is_upper = is_all_upper;
    int orig_bit_no = bit_no;
    v = getCodeIdx(us_vcode, in, len, &bit_no);
    if (v == 199) {
      bit_no = orig_bit_no;
      break;
    }
    h = dstate;
    if (v == 0) {
      h = getCodeIdx(us_hcode, in, len, &bit_no);
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
           v = getCodeIdx(us_vcode, in, len, &bit_no);
           if (v == 199) {
             bit_no = orig_bit_no;
             break;
           }
           if (v == 0) {
              h = getCodeIdx(us_hcode, in, len, &bit_no);
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
        v = getCodeIdx(us_vcode, in, len, &bit_no);
        if (v == 199) {
          bit_no = orig_bit_no;
          break;
        }
      }
    }
    if (v == 0 && h == SHX_SET1A) {
      if (is_upper) {
        out[ol++] = readCount(in, &bit_no, len);
      } else {
        ol = decodeRepeat(in, len, out, ol, &bit_no, prev_lines);
      }
      continue;
    }
    if (h == SHX_SET1 && v == 3) {
      do {
        int32_t delta = readUnicode(in, &bit_no, len);
        if ((delta >> 8) == 0x7FFFFF) {
          int spl_code_idx = delta & 0x000000FF;
          if (spl_code_idx == 2)
            break;
          switch (spl_code_idx) {
            case 1:
              out[ol++] = ' ';
              break;
            case 0:
              ol = decodeRepeat(in, len, out, ol, &bit_no, prev_lines);
              break;
            case 3:
              out[ol++] = ',';
              break;
            case 4:
              if (prev_uni > 0x3000)
                writeUTF8(out, &ol, 0x3002);
              else
                out[ol++] = '.';
              break;
            case 5:
              out[ol++] = 13;
              break;
            case 6:
              out[ol++] = 10;
          }
        } else {
          prev_uni += delta;
          writeUTF8(out, &ol, prev_uni);
        }
      } while (is_upper);
      //printf("Sign: %d, bitno: %d\n", sign, bit_no);
      //printf("Code: %d\n", prev_uni);
      //printf("BitNo: %d\n", bit_no);
      continue;
    }
    if (h < 64 && v < 32)
      c = us_sets[h][v];
    if (c >= 'a' && c <= 'z') {
      if (is_upper)
        c -= 32;
    } else {
      if (is_upper && dstate == SHX_SET1 && v == 1)
        c = '\t';
      if (h == SHX_SET1B) {
         switch (v) {
           case 9:
             out[ol++] = '\r';
             out[ol++] = '\n';
             continue;
           case 8:
             if (is_upper) { // rpt
               int count = readCount(in, &bit_no, len);
               count += 4;
               char rpt_c = out[ol - 1];
               while (count--)
                 out[ol++] = rpt_c;
             } else {
               out[ol++] = '\n';
             }
             continue;
           case 10:
             continue;
         }
      }
    }
    out[ol++] = c;
  }

  return ol;

}

int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace((unsigned char)*s))
      return 0;
    s++;
  }
  return 1;
}

// From https://stackoverflow.com/questions/19758270/read-varint-from-linux-sockets#19760246
// Encode an unsigned 64-bit varint.  Returns number of encoded bytes.
// 'buffer' must have room for up to 10 bytes.
int encode_unsigned_varint(uint8_t *buffer, uint64_t value) {
  int encoded = 0;
  do {
    uint8_t next_byte = value & 0x7F;
    value >>= 7;
    if (value)
      next_byte |= 0x80;
    buffer[encoded++] = next_byte;
  } while (value);
  return encoded;
}

uint64_t decode_unsigned_varint(const uint8_t *data, int *decoded_bytes) {
  int i = 0;
  uint64_t decoded_value = 0;
  int shift_amount = 0;
  do {
    decoded_value |= (uint64_t)(data[i] & 0x7F) << shift_amount;     
    shift_amount += 7;
  } while ((data[i++] & 0x80) != 0);
  *decoded_bytes = i;
  return decoded_value;
}

void print_string_as_hex(char *in, int len) {

  int l;
  byte bit;
  printf("String in hex:\n");
  for (l=0; l<len; l++) {
    printf("%x, ", (unsigned char) in[l]);
  }
  printf("\n");

}

void print_compressed(char *in, int len) {

  int l;
  byte bit;
  printf("Compressed bytes in decimal:\n");
  for (l=0; l<len; l++) {
    printf("%d, ", in[l]);
  }
  printf("\n\nCompressed bytes in binary:\n");
  for (l=0; l<len*8; l++) {
    bit = (in[l/8]>>(7-l%8))&0x01;
    printf("%d", bit);
    if (l%8 == 7) printf(" ");
  }
  printf("\n");

}

uint32_t getTimeVal() {
#ifdef _MSC_VER
    return GetTickCount() * 1000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + tv.tv_usec;
#endif
}

double timedifference(uint32_t t0, uint32_t t1) {
    double ret = t1;
    ret -= t0;
    ret /= 1000;
    return ret;
}

int main(int argv, char *args[]) {

char cbuf[1024];
char dbuf[1024];
long len, tot_len, clen, ctot, dlen, l;
float perc;
FILE *fp, *wfp;
int bytes_read;
char c_in;
uint32_t tStart;

tStart = getTimeVal();

if (argv == 4 && strcmp(args[1], "-c") == 0) {
   tot_len = 0;
   ctot = 0;
   fp = fopen(args[2], "rb");
   if (fp == NULL) {
      perror(args[2]);
      return 1;
   }
   wfp = fopen(args[3], "wb+");
   if (wfp == NULL) {
      perror(args[3]);
      return 1;
   }
   do {
     bytes_read = fread(cbuf, 1, sizeof(cbuf), fp);
     if (bytes_read > 0) {
        clen = unishox1_compress(cbuf, bytes_read, dbuf, NULL);
        ctot += clen;
        tot_len += bytes_read;
        if (clen > 0) {
           fputc(clen >> 8, wfp);
           fputc(clen & 0xFF, wfp);
           if (clen != fwrite(dbuf, 1, clen, wfp)) {
              perror("fwrite");
              return 1;
           }
        }
     }
   } while (bytes_read > 0);
   perc = (tot_len-ctot);
   perc /= tot_len;
   perc *= 100;
   printf("\nBytes (Compressed/Original=Savings%%): %ld/%ld=", ctot, tot_len);
   printf("%.2f%%\n", perc);
} else
if (argv == 4 && strcmp(args[1], "-d") == 0) {
   fp = fopen(args[2], "rb");
   if (fp == NULL) {
      perror(args[2]);
      return 1;
   }
   wfp = fopen(args[3], "wb+");
   if (wfp == NULL) {
      perror(args[3]);
      return 1;
   }
   do {
     //memset(dbuf, 0, sizeof(dbuf));
     int len_to_read = fgetc(fp) << 8;
     len_to_read += fgetc(fp);
     bytes_read = fread(dbuf, 1, len_to_read, fp);
     if (bytes_read > 0) {
        dlen = unishox1_decompress(dbuf, bytes_read, cbuf, NULL);
        if (dlen > 0) {
           if (dlen != fwrite(cbuf, 1, dlen, wfp)) {
              perror("fwrite");
              return 1;
           }
        }
     }
   } while (bytes_read > 0);
} else
if (argv == 4 && (strcmp(args[1], "-g") == 0 || 
      strcmp(args[1], "-G") == 0)) {
   if (strcmp(args[1], "-g") == 0)
     to_match_repeats = 0;
   fp = fopen(args[2], "r");
   if (fp == NULL) {
      perror(args[2]);
      return 1;
   }
   sprintf(cbuf, "%s.h", args[3]);
   wfp = fopen(cbuf, "w+");
   if (wfp == NULL) {
      perror(args[3]);
      return 1;
   }
   tot_len = 0;
   ctot = 0;
   struct us_lnk_lst *cur_line = NULL;
   fputs("#ifndef __", wfp);
   fputs(args[3], wfp);
   fputs("_UNISHOX1_COMPRESSED__\n", wfp);
   fputs("#define __", wfp);
   fputs(args[3], wfp);
   fputs("_UNISHOX1_COMPRESSED__\n", wfp);
   int line_ctr = 0;
   int max_len = 0;
   while (fgets(cbuf, sizeof(cbuf), fp) != NULL) {
      // compress the line and look in previous lines
      // add to linked list
      len = strlen(cbuf);
      if (cbuf[len - 1] == '\n' || cbuf[len - 1] == '\r') {
         len--;
         cbuf[len] = 0;
      }
      if (is_empty(cbuf))
        continue;
      if (len > 0) {
        struct us_lnk_lst *ll;
        ll = cur_line;
        cur_line = (struct us_lnk_lst *) malloc(sizeof(struct us_lnk_lst));
        cur_line->data = (char *) malloc(len + 1);
        strncpy(cur_line->data, cbuf, len);
        cur_line->previous = ll;
        clen = unishox1_compress(cbuf, len, dbuf, cur_line);
        if (clen > 0) {
            perc = (len-clen);
            perc /= len;
            perc *= 100;
            //print_compressed(dbuf, clen);
            printf("len: %ld/%ld=", clen, len);
            printf("%.2f %s\n", perc, cbuf);
            tot_len += len;
            ctot += clen;
            char short_buf[strlen(args[3]) + 100];
            snprintf(short_buf, sizeof(short_buf), "const byte %s_%d[] PROGMEM = {", args[3], line_ctr++);
            fputs(short_buf, wfp);
            int len_len = encode_unsigned_varint((byte *) short_buf, clen);
            for (int i = 0; i < len_len; i++) {
              snprintf(short_buf, 10, "%u, ", (byte) short_buf[i]);
              fputs(short_buf, wfp);
            }
            for (int i = 0; i < clen; i++) {
              if (i) {
                strcpy(short_buf, ", ");
                fputs(short_buf, wfp);
              }
              snprintf(short_buf, 6, "%u", (byte) dbuf[i]);
              fputs(short_buf, wfp);
            }
            strcpy(short_buf, "};\n");
            fputs(short_buf, wfp);
        }
        if (len > max_len)
          max_len = len;
        dlen = unishox1_decompress(dbuf, clen, cbuf, cur_line);
        cbuf[dlen] = 0;
        printf("\n%s\n", cbuf);
      }
   }
   perc = (tot_len-ctot);
   perc /= tot_len;
   perc *= 100;
   printf("\nBytes (Compressed/Original=Savings%%): %ld/%ld=", ctot, tot_len);
   printf("%.2f%%\n", perc);
   char short_buf[strlen(args[3]) + 100];
   snprintf(short_buf, sizeof(short_buf), "const byte * const %s[] PROGMEM = {", args[3]);
   fputs(short_buf, wfp);
   for (int i = 0; i < line_ctr; i++) {
     if (i) {
       strcpy(short_buf, ", ");
       fputs(short_buf, wfp);
     }
     snprintf(short_buf, strlen(args[3]) + 15, "%s_%d", args[3], i);
     fputs(short_buf, wfp);
   }
   strcpy(short_buf, "};\n");
   fputs(short_buf, wfp);
   snprintf(short_buf, sizeof(short_buf), "#define %s_line_count %d\n", args[3], line_ctr);
   fputs(short_buf, wfp);
   snprintf(short_buf, sizeof(short_buf), "#define %s_max_len %d\n", args[3], max_len);
   fputs(short_buf, wfp);
   fputs("#endif\n", wfp);
} else
if (argv == 2) {
   len = strlen(args[1]);
   //printf("Len:%ld\n", len);
   //print_string_as_hex(args[1], len);
   memset(cbuf, 0, sizeof(cbuf));
   ctot = unishox1_compress(args[1], len, cbuf, NULL);
   print_compressed(cbuf, ctot);
   memset(dbuf, 0, sizeof(dbuf));
   dlen = unishox1_decompress(cbuf, ctot, dbuf, NULL);
   dbuf[dlen] = 0;
   printf("\nDecompressed: %s\n", dbuf);
   //print_compressed(dbuf, dlen);
   perc = (len-ctot);
   perc /= len;
   perc *= 100;
   printf("\nBytes (Compressed/Original=Savings%%): %ld/%ld=", ctot, len);
   printf("%.2f%%\n", perc);
} else {
   printf("Unishox (byte format version: %s)\n", UNISHOX_VERSION);
   printf("---------------------------------\n");
   printf("Usage: unishox1 \"string\" or unishox1 [action] [in_file] [out_file] [encoding]\n");
   printf("\n");
   printf("Actions:\n");
   printf("  -c    compress\n");
   printf("  -d    decompress\n");
   printf("  -g    generate C header file\n");
   printf("  -G    generate C header file using additional compression (slower)\n");
   return 1;
}

printf("\nElapsed: %0.3lf ms\n", timedifference(tStart, getTimeVal()));

return 0;

}
#pragma GCC diagnostic pop

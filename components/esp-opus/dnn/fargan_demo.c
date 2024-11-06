/* Copyright (c) 2018 Mozilla */
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
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "arch.h"
#include "lpcnet.h"
#include "freq.h"
#include "os_support.h"
#include "fargan.h"
#include "cpu_support.h"

#ifdef USE_WEIGHTS_FILE
# if __unix__
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#  include <sys/stat.h>
/* When available, mmap() is preferable to reading the file, as it leads to
   better resource utilization, especially if multiple processes are using the same
   file (mapping will be shared in cache). */
void *load_blob(const char *filename, int *len) {
  int fd;
  void *data;
  struct stat st;
  if (stat(filename, &st)) {
     *len = 0;
     return NULL;
  }
  *len = st.st_size;
  fd = open(filename, O_RDONLY);
  if (fd<0) {
     *len = 0;
     return NULL;
  }
  data = mmap(NULL, *len, PROT_READ, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
     *len = 0;
     data = NULL;
  }
  close(fd);
  return data;
}
void free_blob(void *blob, int len) {
  if (blob) munmap(blob, len);
}
# else
void *load_blob(const char *filename, int *len) {
  FILE *file;
  void *data;
  file = fopen(filename, "r");
  if (file == NULL)
  {
    perror("could not open blob file");
    *len = 0;
    return NULL;
  }
  fseek(file, 0L, SEEK_END);
  *len = ftell(file);
  fseek(file, 0L, SEEK_SET);
  if (*len <= 0) {
     *len = 0;
     return NULL;
  }
  data = malloc(*len);
  if (!data) {
     *len = 0;
     return NULL;
  }
  *len = fread(data, 1, *len, file);
  return data;
}
void free_blob(void *blob, int len) {
  free(blob);
  (void)len;
}
# endif
#endif

#define MODE_FEATURES 2
/*#define MODE_SYNTHESIS 3*/
#define MODE_ADDLPC 5
#define MODE_FWGAN_SYNTHESIS 6
#define MODE_FARGAN_SYNTHESIS 7

void usage(void) {
    fprintf(stderr, "usage: lpcnet_demo -features <input.pcm> <features.f32>\n");
    fprintf(stderr, "       lpcnet_demo -fargan-synthesis <features.f32> <output.pcm>\n");
    fprintf(stderr, "       lpcnet_demo -addlpc <features_without_lpc.f32> <features_with_lpc.lpc>\n\n");
    fprintf(stderr, "  plc_options:\n");
    fprintf(stderr, "       causal:       normal (causal) PLC\n");
    fprintf(stderr, "       codec:        normal (causal) PLC without cross-fade (will glitch)\n");
    exit(1);
}

int main(int argc, char **argv) {
    int mode=0;
    int arch;
    FILE *fin, *fout;
#ifdef USE_WEIGHTS_FILE
    int len;
    void *data;
    const char *filename = "weights_blob.bin";
#endif
    arch = opus_select_arch();
    if (argc < 4) usage();
    if (strcmp(argv[1], "-features") == 0) mode=MODE_FEATURES;
    else if (strcmp(argv[1], "-fargan-synthesis") == 0) mode=MODE_FARGAN_SYNTHESIS;
    else if (strcmp(argv[1], "-addlpc") == 0){
        mode=MODE_ADDLPC;
    } else {
        usage();
    }
    if (argc != 4) usage();
    fin = fopen(argv[2], "rb");
    if (fin == NULL) {
        fprintf(stderr, "Can't open %s\n", argv[2]);
        exit(1);
    }

    fout = fopen(argv[3], "wb");
    if (fout == NULL) {
        fprintf(stderr, "Can't open %s\n", argv[3]);
        exit(1);
    }
#ifdef USE_WEIGHTS_FILE
    data = load_blob(filename, &len);
#endif
    if (mode == MODE_FEATURES) {
        LPCNetEncState *net;
        net = lpcnet_encoder_create();
        while (1) {
            float features[NB_TOTAL_FEATURES];
            opus_int16 pcm[LPCNET_FRAME_SIZE];
            size_t ret;
            ret = fread(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fin);
            if (feof(fin) || ret != LPCNET_FRAME_SIZE) break;
            lpcnet_compute_single_frame_features(net, pcm, features, arch);
            fwrite(features, sizeof(float), NB_TOTAL_FEATURES, fout);
        }
        lpcnet_encoder_destroy(net);
    } else if (mode == MODE_FARGAN_SYNTHESIS) {
        FARGANState fargan;
        size_t ret, i;
        float in_features[5*NB_TOTAL_FEATURES];
        float zeros[320] = {0};
        fargan_init(&fargan);
#ifdef USE_WEIGHTS_FILE
        fargan_load_model(&fargan, data, len);
#endif
        /* uncomment the following to align with Python code */
        /*ret = fread(&in_features[0], sizeof(in_features[0]), NB_TOTAL_FEATURES, fin);*/
        for (i=0;i<5;i++) {
          ret = fread(&in_features[i*NB_FEATURES], sizeof(in_features[0]), NB_TOTAL_FEATURES, fin);
        }
        fargan_cont(&fargan, zeros, in_features);
        while (1) {
            float features[NB_FEATURES];
            float fpcm[LPCNET_FRAME_SIZE];
            opus_int16 pcm[LPCNET_FRAME_SIZE];
            ret = fread(in_features, sizeof(features[0]), NB_TOTAL_FEATURES, fin);
            if (feof(fin) || ret != NB_TOTAL_FEATURES) break;
            OPUS_COPY(features, in_features, NB_FEATURES);
            fargan_synthesize(&fargan, fpcm, features);
            for (i=0;i<LPCNET_FRAME_SIZE;i++) pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
            fwrite(pcm, sizeof(pcm[0]), LPCNET_FRAME_SIZE, fout);
        }
    } else if (mode == MODE_ADDLPC) {
        float features[36];
        size_t ret;

        while (1) {
            ret = fread(features, sizeof(features[0]), 36, fin);
            if (ret != 36 || feof(fin)) break;
            lpc_from_cepstrum(&features[20], &features[0]);
            fwrite(features, sizeof(features[0]), 36, fout);
        }

    } else {
        fprintf(stderr, "unknown action\n");
    }
    fclose(fin);
    fclose(fout);
#ifdef USE_WEIGHTS_FILE
    free_blob(data, len);
#endif
    return 0;
}

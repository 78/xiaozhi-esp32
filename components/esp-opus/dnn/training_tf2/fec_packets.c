/* Copyright (c) 2022 Amazon
   Written by Jan Buethe */
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
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <inttypes.h>

#include "fec_packets.h"

int get_fec_frame(const char * const filename, float *features, int packet_index, int subframe_index)
{

    int16_t version;
    int16_t header_size;
    int16_t num_packets;
    int16_t packet_size;
    int16_t subframe_size;
    int16_t subframes_per_packet;
    int16_t num_features;
    long offset;

    FILE *fid = fopen(filename, "rb");

    /* read header */
    if (fread(&version, sizeof(version), 1, fid) != 1) goto error;
    if (fread(&header_size, sizeof(header_size), 1, fid) != 1) goto error;
    if (fread(&num_packets, sizeof(num_packets), 1, fid) != 1) goto error;
    if (fread(&packet_size, sizeof(packet_size), 1, fid) != 1) goto error;
    if (fread(&subframe_size, sizeof(subframe_size), 1, fid) != 1) goto error;
    if (fread(&subframes_per_packet, sizeof(subframes_per_packet), 1, fid) != 1) goto error;
    if (fread(&num_features, sizeof(num_features), 1, fid) != 1) goto error;

    /* check if indices are valid */
    if (packet_index >= num_packets || subframe_index >= subframes_per_packet)
    {
        fprintf(stderr, "get_fec_frame: index out of bounds\n");
        goto error;
    }

    /* calculate offset in file (+ 2 is for rate) */
    offset = header_size + packet_index * packet_size + 2 + subframe_index * subframe_size;
    fseek(fid, offset, SEEK_SET);

    /* read features */
    if (fread(features, sizeof(*features), num_features, fid) != num_features) goto error;

    fclose(fid);
    return 0;

error:
    fclose(fid);
    return 1;
}

int get_fec_rate(const char * const filename, int packet_index)
{
    int16_t version;
    int16_t header_size;
    int16_t num_packets;
    int16_t packet_size;
    int16_t subframe_size;
    int16_t subframes_per_packet;
    int16_t num_features;
    long offset;
    int16_t rate;

    FILE *fid = fopen(filename, "rb");

    /* read header */
    if (fread(&version, sizeof(version), 1, fid) != 1) goto error;
    if (fread(&header_size, sizeof(header_size), 1, fid) != 1) goto error;
    if (fread(&num_packets, sizeof(num_packets), 1, fid) != 1) goto error;
    if (fread(&packet_size, sizeof(packet_size), 1, fid) != 1) goto error;
    if (fread(&subframe_size, sizeof(subframe_size), 1, fid) != 1) goto error;
    if (fread(&subframes_per_packet, sizeof(subframes_per_packet), 1, fid) != 1) goto error;
    if (fread(&num_features, sizeof(num_features), 1, fid) != 1) goto error;

    /* check if indices are valid */
    if (packet_index >= num_packets)
    {
        fprintf(stderr, "get_fec_rate: index out of bounds\n");
        goto error;
    }

    /* calculate offset in file (+ 2 is for rate) */
    offset = header_size + packet_index * packet_size;
    fseek(fid, offset, SEEK_SET);

    /* read rate */
    if (fread(&rate, sizeof(rate), 1, fid) != 1) goto error;

    fclose(fid);
    return (int) rate;

error:
    fclose(fid);
    return -1;
}

#if 0
int main()
{
    float features[20];
    int i;

    if (get_fec_frame("../test.fec", &features[0], 0, 127))
    {
        return 1;
    }

    for (i = 0; i < 20; i ++)
    {
        printf("%d %f\n", i, features[i]);
    }

    printf("rate: %d\n", get_fec_rate("../test.fec", 0));

}
#endif
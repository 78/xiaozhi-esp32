/*************************************************************
 * Author:	wangjiangyuan (wangjiangyuan@agora.io)
 * Date	 :	Oct 21th, 2020
 * Module:	Agora SD-RTN SDK RTC C API demo application.
 *
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_parser.h"
#include "file_parser_priv.h"

#define ADTS_HEADER_SIZE (7)

typedef struct {
  int data_offset_;
  int data_size_;
  uint8_t *data_buffer_;
  int fd_;
} ctx_t;

typedef struct AACAudioFrame_ {
  uint16_t syncword;
  uint8_t id;
  uint8_t layer;
  uint8_t protection_absent;
  uint8_t profile;
  uint8_t sampling_frequency_index;
  uint8_t private_bit;
  uint8_t channel_configuration;
  uint8_t original_copy;
  uint8_t home;

  uint8_t copyrighted_id_bit;
  uint8_t copyrighted_id_start;
  uint16_t aac_frame_length;
  uint16_t adts_buffer_fullness;
  uint8_t number_of_raw_data_blocks_in_frame;
} AACAudioFrame;

/* static const uint32_t AacFrameSampleRateMap[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000,
												  22000, 16000, 12000, 11025, 8000,	 7350 }; */

static int aac_open(media_parser_t *h, const char *path)
{
  int rval = -1;

  int fd = -1;
  do {
    struct stat sb;
    void *mapped;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
      AGO_LOGD("parser: invalid fd, path=%s", path);
      break;
    }
    AGO_LOGI("Open audio file %s successfully", path);

    if ((fstat(fd, &sb)) == -1) {
      break;
    }

    mapped = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == (void *)-1) {
      break;
    }

    ctx_t *p_ctx = (ctx_t *)malloc(sizeof(ctx_t));
    if (!p_ctx) {
      break;
    }
    p_ctx->data_size_ = sb.st_size;
    p_ctx->data_buffer_ = (uint8_t *)mapped;
    p_ctx->data_offset_ = 0;
    p_ctx->fd_ = fd;

    h->p_ctx = (void *)p_ctx;
    rval = 0;
  } while (0);

  return rval;
}

static int aac_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  int rval = -1;

  do {
    ctx_t *p_ctx = (ctx_t *)h->p_ctx;
    if (!p_ctx) {
      AGO_LOGD("parser: invalid ctx");
      break;
    }

    // Check data offset and rewind to the file start if necessary
    if (p_ctx->data_offset_ + ADTS_HEADER_SIZE > p_ctx->data_size_) {
      // p_ctx->data_offset_ = 0;
      rval = -2;
      break;
    }

    AACAudioFrame aacframe;

    // Begin by reading the 7-byte fixed_variable headers
    unsigned char *hdr = &p_ctx->data_buffer_[p_ctx->data_offset_];

    // parse adts_fixed_header()
    aacframe.syncword = (hdr[0] << 4) | (hdr[1] >> 4);
    if (aacframe.syncword != 0xfff) {
      AGO_LOGE("parser: Invalid AAC syncword 0x%x at 0x%x", aacframe.syncword, p_ctx->data_offset_);
      p_ctx->data_offset_ += 1;
      continue;
    }

    /* 		aacframe.id = (hdr[1] >> 3) & 0x01;
		aacframe.layer = (hdr[1] >> 1) & 0x03;
		aacframe.protection_absent = hdr[1] & 0x01;
		aacframe.profile = (hdr[2] >> 6) & 0x03;
		aacframe.sampling_frequency_index = (hdr[2] >> 2) & 0x0f;
		aacframe.private_bit = (hdr[2] >> 1) & 0x01;
		aacframe.channel_configuration = ((hdr[2] & 0x1) << 2) | (hdr[3] >> 6);
		aacframe.original_copy = (hdr[3] >> 5) & 0x01;
		aacframe.home = (hdr[3] >> 4) & 0x01;
		// parse adts_variable_header()
		aacframe.copyrighted_id_bit = (hdr[3] >> 3) & 0x01;
		aacframe.copyrighted_id_start = (hdr[3] >> 2) & 0x01; */
    aacframe.aac_frame_length = ((hdr[3] & 0x3) << 11) | (hdr[4] << 3) | (hdr[5] >> 5);
    /* 		aacframe.adts_buffer_fullness = ((hdr[5] & 0x1f) << 6) | (hdr[6] >> 2);
		aacframe.number_of_raw_data_blocks_in_frame = hdr[6] & 0x03; */

    p_frame->type = h->type;
    p_frame->ptr = p_ctx->data_buffer_ + p_ctx->data_offset_;
    p_frame->len = aacframe.aac_frame_length;

    /* 		p_frame->u.audio.sampleRateHz = AacFrameSampleRateMap[aacframe.sampling_frequency_index]; */

    p_ctx->data_offset_ += aacframe.aac_frame_length;

    rval = 0;
    break;
  } while (1);

  return rval;
}

static int aac_release_frame(media_parser_t *h, frame_t *p_frame)
{
  return 0;
}

static int aac_reset(media_parser_t *h)
{
  int rval = -1;
  do {
    ctx_t *p_ctx = (ctx_t *)h->p_ctx;
    if (!p_ctx) {
      break;
    }

    p_ctx->data_offset_ = 0;
    rval = 0;
  } while (0);

  return rval;
}

static int aac_close(media_parser_t *h)
{
  int rval = -1;
  do {
    ctx_t *p_ctx = (ctx_t *)h->p_ctx;
    if (!p_ctx) {
      break;
    }

    if (p_ctx->data_buffer_) {
      munmap(p_ctx->data_buffer_, p_ctx->data_size_);
      p_ctx->data_buffer_ = NULL;
      p_ctx->data_size_ = 0;
    }

    if (p_ctx->fd_) {
      close(p_ctx->fd_);
    }

    free(p_ctx);
    h->p_ctx = NULL;
    rval = 0;
  } while (0);

  return rval;
}

media_parser_t audio_parser_aac = {
  .codec = MEDIA_FILE_TYPE_AACLC,
  .name = "aac",
  .p_ctx = NULL,

  .open = aac_open,
  .obtain_frame = aac_obtain_frame,
  .release_frame = aac_release_frame,
  .reset = aac_reset,
  .close = aac_close,
};

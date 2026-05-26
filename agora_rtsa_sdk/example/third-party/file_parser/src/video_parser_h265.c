/*************************************************************
 * Author:	zhangguanxian (zhangguanxian@agora.io)
 * Date	 :	Mar 3th, 2022
 * Module:	Agora SD-RTN SDK RTC C API demo application.
 *
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2022 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include "file_parser.h"
#include "file_parser_priv.h"

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 Find the beginning and end of a NAL (Network Abstraction Layer) unit in a byte
 buffer containing H265 bitstream data.
 @param[in]   buf        the buffer
 @param[in]   size       the size of the buffer
 @param[out]  nal_start  the beginning offset of the nal
 @param[out]  nal_end    the end offset of the nal
 @return                 the length of the nal, or 0 if did not find start of
 nal, or -1 if did not find end of nal
 */
// DEPRECATED - this will be replaced by a similar function with a slightly
// different API
static int find_nal_unit(uint8_t *buf, int size, uint8_t *nal_type, int *nal_start, int *nal_end)
{
  if (size < 4) {
    return 0;
  }
  int i = 0;
  // find start
  *nal_start = 0;
  *nal_end = 0;

  while (buf[i] != 0 || buf[i + 1] != 0 ||
       !(buf[i + 2] == 1 || (buf[i + 2] == 0 && buf[i + 3] == 1))) {
    i++;
    if (size < i + 4) {
      return 0;
    } // did not find nal start
  }

  *nal_start = i;

  if (buf[i + 2] == 1) {
    *nal_type = (buf[i + 3] & 0x7e)>>1;
    i += 4;
  } else if (size > i + 4) {
    *nal_type = (buf[i + 4] & 0x7e)>>1;
    i += 5;
  } else {
    return 0;
  }
  if (size < i + 4) {
    *nal_end = size - 1;
    return -1;
  }

  while (buf[i] != 0 || buf[i + 1] != 0 ||
       !(buf[i + 2] == 1 || (buf[i + 2] == 0 && buf[i + 3] == 1))) {
    i++;
    if (size < i + 4) {
      *nal_end = size - 1;
      return -1;
    } // did not find nal end, stream ended first
  }

  *nal_end = i - 1;
  return (*nal_end - *nal_start);
}

#define BIT(num, bit) (((num) & (1 << (7 - bit))) > 0)
static int exp_golomb_decode(uint8_t *buffer, int size, int *bitOffset)
{
  int totalBits = size << 3;
  int leadingZeroBits = 0;
  for (int i = *bitOffset; i < totalBits && !BIT(buffer[i / 8], i % 8); i++) {
    leadingZeroBits++;
  }
  int offset = 0;
  int bitPos = *bitOffset + leadingZeroBits + 1;
  for (int i = 0; i < leadingZeroBits; i++) {
    offset = (offset << 1) + BIT(buffer[bitPos / 8], bitPos % 8);
    bitPos++;
  }
  *bitOffset += leadingZeroBits + 1 + leadingZeroBits;
  return (1 << leadingZeroBits) - 1 + offset;
}


typedef struct {
  int data_offset_;
  int data_size_;
  uint8_t *data_buffer_;
  int fd_;
} ctx_t;

static void _getH265Frame(media_parser_t *h, frame_t *p_frame, int is_key_frame, int frame_start, int frame_end)
{
  int datalen = frame_end - frame_start + 1;
  ctx_t *p_ctx = (ctx_t *)h->p_ctx;

  p_frame->type = h->type;
  p_frame->ptr = p_ctx->data_buffer_ + frame_start;
  p_frame->len = datalen;
  p_frame->u.video.is_key_frame = is_key_frame;
}

static int h265_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  uint8_t nal_type = 0;
  int nal_start = 0;
  int nal_end = 0;
  bool is_key_frame;
  int frame_start = 0;
  int frame_end = 0;
  int ret = 0;
  int rval = -1;

  ctx_t *p_ctx = (ctx_t *)h->p_ctx;
  if (!p_ctx) {
    AGO_LOGE("parser: invalid ctx");
    return -1;
  }

  // get first nalu for frame_start
  ret = find_nal_unit(p_ctx->data_buffer_ + p_ctx->data_offset_, p_ctx->data_size_ - p_ctx->data_offset_, &nal_type,
                      &nal_start, &nal_end);
  if (ret == 0) {
    printf("End of video file, offset:%d, size:%d\n", p_ctx->data_offset_, p_ctx->data_size_);
    rval = -2;
    return rval;
  }
  frame_start = p_ctx->data_offset_ + nal_start;

  // get first I slice or P slice for frame_type
  while (nal_type != 1 && nal_type != 19) {
    p_ctx->data_offset_ += nal_end + 1;
    ret = find_nal_unit(p_ctx->data_buffer_ + p_ctx->data_offset_, p_ctx->data_size_ - p_ctx->data_offset_, &nal_type,
                        &nal_start, &nal_end);
    if (ret == 0) {
      printf("End of video file, offset:%d, size:%d\n", p_ctx->data_offset_, p_ctx->data_size_);
      rval = -2;
      return rval;
    }
  }
  int offset = p_ctx->data_offset_ + nal_start;
  offset += p_ctx->data_buffer_[offset + 2] ? 3 : 4 + 1;

  int bitOffset = 0;
  int first_mb_in_slice =
      exp_golomb_decode(p_ctx->data_buffer_ + offset, p_ctx->data_size_ - offset, &bitOffset);
  int slice_type = exp_golomb_decode(p_ctx->data_buffer_ + offset, p_ctx->data_size_ - offset, &bitOffset);

  if (nal_type == 19) { // IDR
    is_key_frame = true;
  } else {
    is_key_frame = false;
  }
  int prev_first_mb_in_slice = first_mb_in_slice;
  int prev_nal_type = nal_type;

  frame_end = p_ctx->data_offset_ - 1;
  frame_end = p_ctx->data_offset_ + nal_end;
  p_ctx->data_offset_ += nal_end + 1;
  _getH265Frame(h, p_frame, is_key_frame, frame_start, frame_end);
  rval = 0;
  return rval;
}

static int h265_release_frame(media_parser_t *h, frame_t *p_frame)
{
  return 0;
}

static int h265_open(media_parser_t *h, const char *path)
{
  int rval = -1;

  int fd = -1;
  do {
    struct stat sb;
    void *mapped;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
      break;
    }

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

static int h265_reset(media_parser_t *h)
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

static int h265_close(media_parser_t *h)
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

    if (p_ctx->fd_ > 0) {
      close(p_ctx->fd_);
    }

    free(p_ctx);
    h->p_ctx = NULL;
    rval = 0;
  } while (0);

  return rval;
}

media_parser_t video_parser_h265 = {
  .codec = MEDIA_FILE_TYPE_H265,
  .name = "h265",
  .p_ctx = NULL,

  .open = h265_open,
  .obtain_frame = h265_obtain_frame,
  .release_frame = h265_release_frame,
  .reset = h265_reset,
  .close = h265_close,
};
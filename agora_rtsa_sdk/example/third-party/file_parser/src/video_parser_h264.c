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

typedef struct {
  int data_offset_;
  int data_size_;
  uint8_t *data_buffer_;
  int fd_;
} ctx_t;

/**
 Find the beginning and end of a NAL (Network Abstraction Layer) unit in a byte
 buffer containing H264 bitstream data.
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

  while (buf[i] != 0 || buf[i + 1] != 0 || !(buf[i + 2] == 1 || (buf[i + 2] == 0 && buf[i + 3] == 1))) {
    i++;
    if (size < i + 4) {
      return 0;
    } // did not find nal start
  }

  *nal_start = i;

  if (buf[i + 2] == 1) {
    *nal_type = buf[i + 3] & 0x1f;
    i += 4;
  } else if (size > i + 4) {
    *nal_type = buf[i + 4] & 0x1f;
    i += 5;
  } else {
    return 0;
  }

  if (size < i + 4) {
    *nal_end = size - 1;
    return -1;
  }

  while (buf[i] != 0 || buf[i + 1] != 0 || !(buf[i + 2] == 1 || (buf[i + 2] == 0 && buf[i + 3] == 1))) {
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
  int i;
  for (i = *bitOffset; i < totalBits && !BIT(buffer[i / 8], i % 8); i++) {
    leadingZeroBits++;
  }
  int offset = 0;
  int bitPos = *bitOffset + leadingZeroBits + 1;
  for (i = 0; i < leadingZeroBits; i++) {
    offset = (offset << 1) + BIT(buffer[bitPos / 8], bitPos % 8);
    bitPos++;
  }
  *bitOffset += leadingZeroBits + 1 + leadingZeroBits;
  return (1 << leadingZeroBits) - 1 + offset;
}

static int h264_open(media_parser_t *h, const char *path)
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

static void _getH264Frame(media_parser_t *h, frame_t *p_frame, int is_key_frame, int frame_start, int frame_end)
{
  int datalen = frame_end - frame_start + 1;
  ctx_t *p_ctx = (ctx_t *)h->p_ctx;

  p_frame->type = h->type;
  p_frame->ptr = p_ctx->data_buffer_ + frame_start;
  p_frame->len = datalen;
  p_frame->u.video.is_key_frame = is_key_frame;
}

static int h264_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  int rval = -1;

  ctx_t *p_ctx = (ctx_t *)h->p_ctx;
  if (!p_ctx) {
    AGO_LOGE("parser: invalid ctx");
    return -1;
  }

  uint8_t nal_type = 0;
  int nal_start = 0;
  int nal_end = 0;
  int is_key_frame;
  int frame_start = 0;
  int frame_end = 0;
  int ret;

  // get first nalu for frame_start
  ret = find_nal_unit(p_ctx->data_buffer_ + p_ctx->data_offset_, p_ctx->data_size_ - p_ctx->data_offset_, &nal_type,
                      &nal_start, &nal_end);
  if (ret == 0) {
    // AGO_LOGW("End of video file, offset:%d, size:%d", p_ctx->data_offset_, p_ctx->data_size_);
    // p_ctx->data_offset_ = 0;
    rval = -2;
    return rval;
  }
  frame_start = p_ctx->data_offset_ + nal_start;

  // get first I slice or P slice for frame_type
  while (nal_type != 1 && nal_type != 5) {
    p_ctx->data_offset_ += nal_end + 1;
    ret = find_nal_unit(p_ctx->data_buffer_ + p_ctx->data_offset_, p_ctx->data_size_ - p_ctx->data_offset_, &nal_type,
                        &nal_start, &nal_end);
    if (ret == 0) {
      // AGO_LOGW("End of video file, offset:%d, size:%d", p_ctx->data_offset_, p_ctx->data_size_);
      // p_ctx->data_offset_ = 0;
      rval = -2;
      return rval;
    }
  }

  int offset = p_ctx->data_offset_ + nal_start;
  offset += p_ctx->data_buffer_[offset + 2] ? 3 : 4 + 1;

  int bitOffset = 0;
  int first_mb_in_slice = exp_golomb_decode(p_ctx->data_buffer_ + offset, p_ctx->data_size_ - offset, &bitOffset);
  int slice_type = exp_golomb_decode(p_ctx->data_buffer_ + offset, p_ctx->data_size_ - offset, &bitOffset);

  if (nal_type == 5) { // IDR
    is_key_frame = 1;
  } else {
    slice_type %= 5;
    if (slice_type == 2 || slice_type == 4) { // I SLICE
      is_key_frame = 1;
    } else { // P SLICE
      is_key_frame = 0;
    }
  }
  int prev_first_mb_in_slice = first_mb_in_slice;
  int prev_nal_type = nal_type;

  // judge the slice is the last slice in a frame or not
  while (1) {
    p_ctx->data_offset_ += nal_end + 1;
    ret = find_nal_unit(p_ctx->data_buffer_ + p_ctx->data_offset_, p_ctx->data_size_ - p_ctx->data_offset_, &nal_type,
                        &nal_start, &nal_end);
    if (nal_type != prev_nal_type)
      break;
    offset = p_ctx->data_offset_ + nal_start;
    offset += p_ctx->data_buffer_[offset + 2] ? 3 : 4 + 1;
    bitOffset = 0;
    //int Last_FNIS = first_mb_in_slice;
    first_mb_in_slice = exp_golomb_decode(p_ctx->data_buffer_ + offset, p_ctx->data_size_ - offset, &bitOffset);
    if ((prev_first_mb_in_slice > first_mb_in_slice) ||
        (prev_first_mb_in_slice == first_mb_in_slice && prev_first_mb_in_slice == 0)) {
      break;
    }

    if (ret == 0) {
      // AGO_LOGW("End of video file, offset:%d, size:%d", p_ctx->data_offset_, p_ctx->data_size_);
      // //printf("num_I is %d,num_P is %d",num_I,num_P);
      // _getH264Frame(h, p_frame, is_key_frame, frame_start, frame_end);
      // p_ctx->data_offset_ = 0;
      // rval = 0;
      // return rval;
      break;
    }
  }

  frame_end = p_ctx->data_offset_ - 1;
  //frame_end = data_offset_ + nal_end;
  //data_offset_ += nal_end + 1;
  _getH264Frame(h, p_frame, is_key_frame, frame_start, frame_end);
  rval = 0;
  return rval;
}

static int h264_release_frame(media_parser_t *h, frame_t *p_frame)
{
  return 0;
}

static int h264_reset(media_parser_t *h)
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

static int h264_close(media_parser_t *h)
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

media_parser_t video_parser_h264 = {
  .codec = MEDIA_FILE_TYPE_H264,
  .name = "h264",
  .p_ctx = NULL,

  .open = h264_open,
  .obtain_frame = h264_obtain_frame,
  .release_frame = h264_release_frame,
  .reset = h264_reset,
  .close = h264_close,
};

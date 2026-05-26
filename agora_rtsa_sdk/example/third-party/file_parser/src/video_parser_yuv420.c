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
  int32_t data_offset_;
  int32_t data_size_;
  uint8_t *data_buffer_;
  int fd_;
} ctx_t;

int32_t yuv420_open(media_parser_t *h, const char *path)
{
  int32_t rval = -1;

  int32_t fd = -1;
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

#define LENGTH_PER_FRAME (16 * 1024)
static int32_t yuv420_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  int32_t rval = -1;

  ctx_t *p_ctx = (ctx_t *)h->p_ctx;
  if (!p_ctx) {
    AGO_LOGE("parser: invalid ctx");
    return -1;
  }

  if (p_ctx->data_offset_ + LENGTH_PER_FRAME > p_ctx->data_size_) {
    p_frame->ptr = p_ctx->data_buffer_ + p_ctx->data_offset_;
    p_frame->len = p_ctx->data_size_ - p_ctx->data_offset_;
    p_ctx->data_offset_ = p_ctx->data_size_;
    return -2;
  } else {
    p_frame->ptr = p_ctx->data_buffer_ + p_ctx->data_offset_;
    p_frame->len = LENGTH_PER_FRAME;
    p_ctx->data_offset_ += LENGTH_PER_FRAME;
  }

  rval = 0;
  return rval;
}

static int yuv420_release_frame(media_parser_t *h, frame_t *p_frame)
{
  return 0;
}

static int32_t yuv420_reset(media_parser_t *h)
{
  int32_t rval = -1;
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

static int32_t yuv420_close(media_parser_t *h)
{
  int32_t rval = -1;
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

media_parser_t video_parser_yuv420 = {
  .codec = MEDIA_FILE_TYPE_YUV420,
  .name = "yuv420",
  .p_ctx = NULL,

  .open = yuv420_open,
  .obtain_frame = yuv420_obtain_frame,
  .release_frame = yuv420_release_frame,
  .reset = yuv420_reset,
  .close = yuv420_close,
};

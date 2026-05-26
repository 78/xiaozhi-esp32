/*************************************************************
 * Author:	zhangguanxian (zhangguanxian@agora.io)
 * Date	 :	2023-10-06
 * Module:	Agora SD-RTN SDK RTC C API demo application.
 *
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2023 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include <stdio.h>

#include "file_parser.h"
#include "file_parser_priv.h"

typedef struct {
  FILE *file;
  size_t file_size;
  size_t frame_len;
} ctx_t;

extern long get_file_size(FILE *f);

static int g722_open(media_parser_t *h, const char *path)
{
  FILE *file = NULL;

  file = fopen(path, "r");

  if (!file) {
    AGO_LOGE("open %s failed", path);
    return -1;
  }

  ctx_t *p_ctx = (ctx_t *)malloc(sizeof(ctx_t));
  if (!p_ctx) {
    return -1;
  }

  p_ctx->file = file;
  p_ctx->file_size = get_file_size(file);
  // g722 is always 16k sample rate and compression ratio is 4:1
  p_ctx->frame_len = (16000 * h->parser_cfg.u.audio_cfg.framePeriodMs / 1000 * sizeof(int16_t) / 4);

  h->p_ctx = (void *)p_ctx;

  return 0;
}

static int g722_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  ctx_t *p_ctx = (ctx_t *)h->p_ctx;
  if (!p_ctx) {
    AGO_LOGE("parser: invalid ctx");
    return -1;
  }

  if (feof(p_ctx->file)) {
    return -2;
  }

  p_frame->ptr = malloc(p_ctx->frame_len);
  fread(p_frame->ptr, 1, p_ctx->frame_len, p_ctx->file);

  p_frame->type = h->type;
  p_frame->len = p_ctx->frame_len;

  return 0;
}

static int g722_release_frame(media_parser_t *h, frame_t *p_frame)
{
  free(p_frame->ptr);
  return 0;
}

static int g722_reset(media_parser_t *h)
{
  ctx_t *p_ctx = (ctx_t *)h->p_ctx;

  fseek(p_ctx->file, 0L, SEEK_SET);

  return 0;
}

static int g722_close(media_parser_t *h)
{
  ctx_t *p_ctx = (ctx_t *)h->p_ctx;

  fclose(p_ctx->file);

  free(p_ctx);
  h->p_ctx = NULL;

  return 0;
}

media_parser_t audio_parser_g722 = {
  .codec = MEDIA_FILE_TYPE_G722,
  .name = "g722",
  .p_ctx = NULL,

  .open = g722_open,
  .obtain_frame = g722_obtain_frame,
  .release_frame = g722_release_frame,
  .reset = g722_reset,
  .close = g722_close,
};

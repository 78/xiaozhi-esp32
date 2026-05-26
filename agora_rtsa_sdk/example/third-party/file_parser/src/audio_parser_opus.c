/*************************************************************
 * Author:	zhangguanxian (zhangguanxian@agora.io)
 * Date	 :	Oct 21th, 2020
 * Module:	Agora SD-RTN SDK RTC C API demo application.
 *
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include "file_parser.h"
#include "file_parser_priv.h"

#if 0
#include "string.h"
#include "opusfile/opusfile.h"


typedef struct {
  int current_packet;
  int total_length;

  int nsamples;
  int nchannels;
  int format;

  unsigned char packet[960 * 2 * 2];
  long bytes;
  long b_o_s;
  long e_o_s;

  ogg_int64_t granulepos;
  ogg_int64_t packetno;
} decode_context;

typedef struct opus_ctx {
  OggOpusFile* oggOpusFile_;
  decode_context decode_context_;
} opus_ctx_t;


int op_decode_cb(void* ctx, OpusMSDecoder* decoder, void* pcm, const ogg_packet* op, int nsamples,
                 int nchannels, int format, int li) {
  decode_context* context = (decode_context*)ctx;
  context->nsamples = nsamples;
  context->nchannels = nchannels;
  context->format = format;

  context->b_o_s = op->b_o_s;
  context->e_o_s = op->e_o_s;
  context->bytes = op->bytes;
  context->granulepos = op->granulepos;
  context->packetno = op->packetno;
  memcpy(context->packet, op->packet, op->bytes);

  context->total_length += op->bytes;
  ++context->current_packet;

  (void)pcm;
  (void)decoder;
  (void)li;

  return 0;
}

static int opus_open(media_parser_t *h, const char *path)
{
  int ret = 0;
  opus_ctx_t *p_ctx = (opus_ctx_t *)malloc(sizeof(opus_ctx_t));
  if (!p_ctx) {
    return -1;
  }
  memset(p_ctx, 0, sizeof(opus_ctx_t));

  p_ctx->oggOpusFile_ = op_open_file(path, &ret);
  if (!p_ctx->oggOpusFile_) {
    goto __tag_failed;
  }

  op_set_decode_callback(p_ctx->oggOpusFile_, op_decode_cb, &p_ctx->decode_context_);

  const OpusHead* head = op_head(p_ctx->oggOpusFile_, -1);
  if (head->channel_count != 1) {
    AGO_LOGE("channel_count=%d\n", head->channel_count);
    goto __tag_failed;
  }

  h->p_ctx = (void *)p_ctx;

  AGO_LOGI("chnum=%d sample=%u\n", head->channel_count, head->input_sample_rate);

  return 0;

__tag_failed:
  if (p_ctx->oggOpusFile_) {
    op_free(p_ctx->oggOpusFile_);
    p_ctx->oggOpusFile_ = NULL;
  }
  free(p_ctx);
  return -1;
}

static int opus_obtain_frame(media_parser_t *h, frame_t *p_frame)
{
  opus_ctx_t *p_ctx = (opus_ctx_t *)h->p_ctx;
  if (!p_ctx || !p_ctx->oggOpusFile_) {
    AGO_LOGE("parser: invalid ctx");
    return -1;
  }

  if (p_ctx->decode_context_.e_o_s) {
    return -2;
  }

  opus_int16 pcm[120 * 48 * 2];
  int ret = op_read_stereo(p_ctx->oggOpusFile_, pcm, sizeof(pcm) / sizeof(*pcm));
  if (ret < 0) {
    AGO_LOGE("read stereo failed\n");
    return -2;
  }

  p_frame->ptr = malloc(p_ctx->decode_context_.bytes);
  memcpy(p_frame->ptr, p_ctx->decode_context_.packet, p_ctx->decode_context_.bytes);
  p_frame->type = h->type;
  p_frame->len = p_ctx->decode_context_.bytes;

  return 0;
}

static int opus_release_frame(media_parser_t *h, frame_t *p_frame)
{
  free(p_frame->ptr);
  return 0;
}

static int opus_reset(media_parser_t *h)
{
  opus_ctx_t *p_ctx = (opus_ctx_t *)h->p_ctx;
  if (!p_ctx || !p_ctx->oggOpusFile_) {
    return -1;
  }

  if (op_pcm_seek(p_ctx->oggOpusFile_, 0)) {
    return -1;
  }

  return 0;
}

static int opus_close(media_parser_t *h)
{
  opus_ctx_t *p_ctx = (opus_ctx_t *)h->p_ctx;

  if (p_ctx && p_ctx->oggOpusFile_) {
    op_free(p_ctx->oggOpusFile_);
    p_ctx->oggOpusFile_ = NULL;
  }
  if (p_ctx) {
    free(p_ctx);
  }
  h->p_ctx = NULL;

  return 0;
}

media_parser_t audio_parser_opus = {
  .codec = MEDIA_FILE_TYPE_OPUS,
  .name = "opus",
  .p_ctx = NULL,

  .open = opus_open,
  .obtain_frame = opus_obtain_frame,
  .release_frame = opus_release_frame,
  .reset = opus_reset,
  .close = opus_close,
};

#else
media_parser_t audio_parser_opus = {
  .codec = -1,
  .name = "opus",
  .p_ctx = NULL,
};
#endif

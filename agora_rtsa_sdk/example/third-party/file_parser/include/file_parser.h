/*************************************************************
 * Author:    wangjiangyuan (wangjiangyuan@agora.io)
 * Date  :    Oct 21th, 2020
 * Module:    Agora SD-RTN SDK RTC C API demo application.
 *
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#ifndef __MEDIA_PARSER_H__
#define __MEDIA_PARSER_H__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * The definition of the ago_av_data_type_e enum.
 */
typedef enum {
  MEDIA_FILE_TYPE_YUV420,
  MEDIA_FILE_TYPE_H264,
  MEDIA_FILE_TYPE_JPEG,
  MEDIA_FILE_TYPE_H265,
  MEDIA_FILE_TYPE_PCM,
  MEDIA_FILE_TYPE_OPUS,
  MEDIA_FILE_TYPE_G711,
  MEDIA_FILE_TYPE_G722,
  MEDIA_FILE_TYPE_AACLC,
  MEDIA_FILE_TYPE_HEAAC,
} media_file_type_e;

typedef struct {
  union {
    struct {
      int sampleRateHz;
      int numberOfChannels;
      int framePeriodMs;
    } audio_cfg;
  } u;
} parser_cfg_t;

typedef struct {
  int type;
  uint8_t *ptr;
  uint32_t len;

  union {
    struct {
      bool is_key_frame;
    } video;

    struct {
    } audio;
  } u;
} frame_t;

void *create_file_parser(media_file_type_e type, const char *path, parser_cfg_t *p_parser_cfg);
int file_parser_obtain_frame(void *p_parser, frame_t *p_frame);
int file_parser_release_frame(void *p_parser, frame_t *p_frame);
void destroy_file_parser(void *p_parser);

#endif /* __MEDIA_PARSER_H__ */
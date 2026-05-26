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

#ifndef __FILE_PARSER_PRIV_H__
#define __FILE_PARSER_PRIV_H__

#include <stdio.h>
#include <stdint.h>
#include "file_parser.h"

#define AGO_LOGS(fmt, ...) fprintf(stdout, "" fmt "\n", ##__VA_ARGS__)
#define AGO_LOGD(fmt, ...) fprintf(stdout, "[DBG] " fmt "\n", ##__VA_ARGS__)
#define AGO_LOGI(fmt, ...) fprintf(stdout, "[INF] " fmt "\n", ##__VA_ARGS__)
#define AGO_LOGW(fmt, ...) fprintf(stdout, "[WRN] " fmt "\n", ##__VA_ARGS__)
#define AGO_LOGE(fmt, ...) fprintf(stdout, "[ERR] " fmt "\n", ##__VA_ARGS__)

typedef struct media_parser_s media_parser_t;
struct media_parser_s {
  int type;
  int codec;
  char name[16];
  void *p_ctx;
  parser_cfg_t parser_cfg;

  int (*open)(media_parser_t *h, const char *path);
  int (*obtain_frame)(media_parser_t *h, frame_t *p_frame);
  int (*release_frame)(media_parser_t *h, frame_t *p_frame);
  int (*reset)(media_parser_t *h);
  int (*close)(media_parser_t *h);
};

#endif /* __FILE_PARSER_PRIV_H__ */
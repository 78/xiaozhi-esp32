/*************************************************************
 * File  :  hello_stream_message.c
 * Module:  Agora SD-RTN SDK RTC C API demo application.
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include "agora_rtc_api.h"
#include "log.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


typedef struct {
  // common config
  const char *p_sdk_log_dir;
  int32_t  log_level;
  const char *p_appid;
  const char *p_token;
  const char *p_channel;
  uint32_t uid;

  // advanced config
  bool receive_only;
  bool ordered;
  bool reliable;
  uint16_t send_size;
  uint8_t pps;
} app_config_t;

static void app_print_usage(int argc, char **argv)
{
  LOGS("\nUsage: %s [OPTION]", argv[0]);
  // short options
  LOGS(" -h, --help                : show help info");
  LOGS(" -i, --app-id              : application id; either app-id OR token MUST be set");
  LOGS(" -t, --token               : token for authentication");
  LOGS(" -c, --channel-id          : channel name; default is 'demo'");
  LOGS(" -u, --user-id             : user id; default is 0");
  LOGS(" -L, --log-level           : set log level. if set -1 mean disable log");
  LOGS(" -R, --recv-only           : do not send stream message data");

  // long options
  LOGS(" --ordered                 : create ordered data stream");
  LOGS(" --reliable                : create reliable data stream");
  LOGS(" --log_path                : the log path, default: io.agora.rtc_sdk");
  LOGS(" --send_size               : the send stream message size");
  LOGS(" --pps                     : Packets Per Second");

  LOGS("\nExample:");
  LOGS("    %s --app-id xxx [--token xxx] --channel-id xxx [--recv-only] [--ordered] [--reliable] ", argv[0]);
}

static void app_print_config(app_config_t *config) {
  LOGS("---------------app config show start---------------------");
  LOGS("<common config info>:");
  LOGS("  appid                   : %s", config->p_appid);
  LOGS("  channel                 : %s", config->p_channel);
  LOGS("  token                   : %s", config->p_token);
  LOGS("  uid                     : %u", config->uid);
  LOGS("  log-level               : %d", config->log_level);
  LOGS("  log_path                : %s", config->p_sdk_log_dir);
  LOGS("<advanced config info>    -");
  LOGS("  received_only      : %d", config->receive_only);
  LOGS("  ordered                 : %d", config->ordered);
  LOGS("  reliable                : %d", config->reliable);
  LOGS("  send_size               : %u", config->send_size);
  LOGS("  pps                     : %u", config->pps);
  LOGS("---------------app config show end-----------------------");
}

static int app_parse_args(int argc, char **argv, app_config_t *config)
{
  const char *av_short_option = "hi:t:c:u:v:a:C:f:S:s:r:n:A:gmRl:dD:L:";
  int av_option_flag = 0;
  const struct option av_long_option[] = { { "help", 0, NULL, 'h' },
                                           { "app-id", 1, NULL, 'i' },
                                           { "token", 1, NULL, 't' },
                                           { "channel-id", 1, NULL, 'c' },
                                           { "user-id", 1, NULL, 'u' },
                                           { "recv-only", 0, NULL, 'R' },
                                           { "log-level", 1, NULL, 'L' },
                                           {"ordered",0,&av_option_flag,1},
                                           {"reliable",0,&av_option_flag,2},
                                           {"log_path",1,&av_option_flag,3},
                                           {"send_size",1,&av_option_flag,4},
                                           {"pps",1,&av_option_flag,5},
                                           { 0, 0, 0, 0 } };

  int ch = -1;
  int optidx = 0;
  int rval = 0;

  while (1) {
    ch = getopt_long(argc, argv, av_short_option, av_long_option, &optidx);
    if (ch == -1) {
      break;
    }

    switch (ch) {
    case 'h':
      return -1;
    case 'i':
      config->p_appid = optarg;
      break;
    case 't':
      config->p_token = optarg;
      break;
    case 'c':
      config->p_channel = optarg;
      break;
    case 'u':
      config->uid = strtoul(optarg, NULL, 10);
      break;
    case 'R':
      config->receive_only = true;
      break;
    case 'L':
      config->log_level = strtol(optarg, NULL, 10);
      break;
    case 0:
      /* Pure long options */
      switch (av_option_flag) {
      case 1: // ordered
        config->ordered = true;
        break;
      case 2: // reliable
        config->reliable = true;
        break;
      case 3: // log_path
        config->p_sdk_log_dir = optarg;
        break;
      case 4: // send_size
        config->send_size = strtoul(optarg, NULL, 10);
        break;
      case 5: // pps
        config->pps = strtoul(optarg, NULL, 10);
        break;
      default:
        LOGE("Unknown cmd param %s", av_long_option[optidx].name);
        return -1;
      }
      break;
    default:
      LOGS("Unknown cmd param %s", av_long_option[optidx].name);
      return -1;
    }
  }

  // check parameter sanity
  if (strcmp(config->p_appid, "") == 0) {
    LOGE("MUST provide App ID");
    return -1;
  }

  if (strcmp(config->p_channel, "") == 0) {
    LOGE("MUST provide channel name");
    return -1;
  }

  if(config->pps <= 0){
    LOGE("pps must > 0");
    config->pps = 1;
  }

  if(config->send_size < 4){
    config->send_size = 4;
  }
  return 0;
}

typedef struct {
  app_config_t config;
  connection_id_t conn_id;
  bool b_stop_flag;
  bool b_connected_flag;
} app_t;

static app_t g_app = {
    .config = {
        // common config
        .p_sdk_log_dir              = "io.agora.rtc_sdk",
        .log_level                  = RTC_LOG_DEBUG,
        .p_appid                    = "",
        .p_channel                  = "",
        .p_token                    = "",
        .uid                        = 0,
        
        // advanced config
        .receive_only               = false,
        .ordered                    = false,
        .reliable                   = false,
        .send_size                  = 4,
        .pps                        = 10,
    },

    .b_stop_flag            = false,
    .b_connected_flag       = false,
};

static void app_signal_handler(int sig)
{
  switch (sig) {
  case SIGINT:
    g_app.b_stop_flag = true;
    break;
  default:
    LOGW("no handler, sig=%d", sig);
  }
}

static int app_init(void)
{
  signal(SIGINT, app_signal_handler);
  return 0;
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
  g_app.b_connected_flag = true;
  connection_info_t conn_info = { 0 };
  agora_rtc_get_connection_info(g_app.conn_id, &conn_info);
  LOGI("[conn-%u] Join the channel %s successfully, uid %u elapsed %d ms", conn_id,
       conn_info.channel_name, uid, elapsed);
}

static void __on_reconnecting(connection_id_t conn_id)
{
  g_app.b_connected_flag = false;
  LOGW("[conn-%u] connection timeout, reconnecting", conn_id);
}

static void __on_connection_lost(connection_id_t conn_id)
{
  g_app.b_connected_flag = false;
  LOGW("[conn-%u] Lost connection from the channel", conn_id);
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  g_app.b_connected_flag = true;
  LOGI("[conn-%u] Rejoin the channel successfully, uid %u elapsed %d ms", conn_id, uid, elapsed_ms);
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  LOGI("[conn-%u] Remote user \"%u\" has joined the channel, elapsed %d ms", conn_id, uid,
       elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
  LOGI("[conn-%u] Remote user \"%u\" has left the channel, reason %d", conn_id, uid, reason);
}

static void __on_user_mute_audio(connection_id_t conn_id, uint32_t uid, bool muted)
{
  LOGI("[conn-%u] audio: uid=%u muted=%d", conn_id, uid, muted);
}

static void __on_user_mute_video(connection_id_t conn_id, uint32_t uid, bool muted)
{
  LOGI("[conn-%u] video: uid=%u muted=%d", conn_id, uid, muted);
}

static void __on_error(connection_id_t conn_id, int code, const char *msg)
{
  if (code == ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT) {
    LOGE("Not enough uplink bandwdith. Error msg \"%s\"", msg);
    return;
  } else if(code == ERR_DATA_STREAM_TIME_OUT){
    LOGE("stream message time out. Error msg \"%s\"", msg);
    return;
  }

  if (code == ERR_INVALID_APP_ID) {
    LOGE("Invalid App ID. Please double check. Error msg \"%s\"", msg);
  } else if (code == ERR_INVALID_CHANNEL_NAME) {
    LOGE("Invalid channel name for conn_id %u. Please double check. Error msg \"%s\"", conn_id,
         msg);
  } else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
    LOGE("Invalid token. Please double check. Error msg \"%s\"", msg);
  } else if (code == ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY) {
    LOGE("Dynamic token is enabled but is not provided. Error msg \"%s\"", msg);
  } else {
    LOGW("Error %d is captured. Error msg \"%s\"", code, msg);
  }

  g_app.b_stop_flag = true;
}

static void __on_stream_message(connection_id_t conn_id, uint32_t uid, int stream_id, const char* data, size_t length, uint64_t sentTs)
{
  uint32_t* p = (uint32_t *)data;
  LOGI("message :%d, length: %zu", *p, length);
}

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler, app_config_t *config)
{
  event_handler->on_join_channel_success = __on_join_channel_success;
  event_handler->on_reconnecting = __on_reconnecting,
  event_handler->on_connection_lost = __on_connection_lost;
  event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
  event_handler->on_user_joined = __on_user_joined;
  event_handler->on_user_offline = __on_user_offline;
  event_handler->on_error = __on_error;
  event_handler->on_stream_message = __on_stream_message;
}

int main(int argc, char **argv)
{
  app_config_t *config = &g_app.config;
  int rval = 0;
  char priv_params[512] = { 0 };

  LOGI("Welcome to RTSA SDK v%s", agora_rtc_get_version());

  // 1. app parse args
  rval = app_parse_args(argc, argv, config);
  if (rval < 0) {
    app_print_usage(argc, argv);
    return -1;
  }

  app_print_config(config);

  // 2. app init
  rval = app_init();
  if (rval < 0) {
    LOGE("Failed to initialize application");
    return -1;
  }

  // 3. API: init agora rtc sdk
  int appid_len = strlen(config->p_appid);
  void *p_appid = (void *)(appid_len == 0 ? NULL : config->p_appid);

  agora_rtc_event_handler_t event_handler = { 0 };
  app_init_event_handler(&event_handler, config);

  rtc_service_option_t service_opt = { 0 };
  service_opt.log_cfg.log_path = config->p_sdk_log_dir;
  service_opt.log_cfg.log_disable = config->log_level == -1;
  service_opt.log_cfg.log_level = config->log_level;

  rval = agora_rtc_init(p_appid, &event_handler, &service_opt);
  if (rval < 0) {
    LOGE("Failed to initialize Agora sdk, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  // 4. API: Create connection
  rval = agora_rtc_create_connection(&g_app.conn_id);
  if (rval < 0) {
    LOGE("Failed to create connection, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  // 5. API: join channel
  int token_len = strlen(config->p_token);
  void *p_token = (void *)(token_len == 0 ? NULL : config->p_token);

  rtc_channel_options_t channel_options = { 0 };
  memset(&channel_options, 0, sizeof(channel_options));

#if 0 // enable media encryption
  // get key from server by cmd: openssl rand -hex 32
  const char *key_str = "dba643c8ba6b6dc738df43d9fd624293b4b12d87a60f518253bd10ba98c48453";
  // get salt from server by cmd: openssl rand -base64 32
  const char *salt_base64_str = "X5w9T+50kzxVOnkJKiY/lUk82/bES2kATOt3vBuGEDw=";
  // enable
  channel_options.crypto_opt.enable = true;
  // mode
  channel_options.crypto_opt.mode =AES_128_GCM2;
  // key
  sprintf(channel_options.crypto_opt.key, "%s", key_str);
  // salt
  uint8_t *salt_bytes = NULL;
  uint32_t salt_bytes_len = 0;
  salt_bytes = util_base64_decode(salt_base64_str, strlen(salt_base64_str), &salt_bytes_len);
  memcpy(channel_options.crypto_opt.salt, salt_bytes, salt_bytes_len);
  free(salt_bytes);
#endif

  rval = agora_rtc_join_channel(g_app.conn_id, config->p_channel, config->uid, p_token,
                                &channel_options);
  if (rval < 0) {
    LOGE("Failed to join channel \"%s\", reason: %s", config->p_channel, agora_rtc_err_2_str(rval));
    return -1;
  }
  
  // make sure create data stream after join channel
  int streamId = 0;
  if (!config->receive_only) {
    rval = agora_rtc_create_data_stream(g_app.conn_id, &streamId, g_app.config.reliable,g_app.config.ordered);
    LOGI("agora_rtc_create_stream, streamId[%d], rval[%d]", streamId, rval);
    if(rval != 0){
      return -1;
    }
  }

  // 6. wait until we join channel successfully
  while (!g_app.b_connected_flag && !g_app.b_stop_flag) {
    usleep(100 * 1000);
  }

  uint32_t count = 0;
  char* data = malloc(g_app.config.send_size);
  memset(data, 0, g_app.config.send_size);
  int sleep_ms = 1000 / g_app.config.pps;
  while(!g_app.b_stop_flag){
    if (!config->receive_only) {
      uint32_t* p = (uint32_t *)data;
      *p = count++;
      int ret = agora_rtc_send_stream_message(g_app.conn_id, streamId, data, g_app.config.send_size);
      if(ret !=0){
        printf("agora_rtc_send_stream_message count[%d] error[%d]\n", count, ret);
      }
      usleep(sleep_ms * 1000);
    } else {
      usleep(100 * 1000);
    }
  }
  free(data);

  // 7. API: leave channel
  agora_rtc_leave_channel(g_app.conn_id);

  // 8: API: Destroy connection
  agora_rtc_destroy_connection(g_app.conn_id);

  // 9. API: fini rtc sdk
  agora_rtc_fini();

  return 0;
}

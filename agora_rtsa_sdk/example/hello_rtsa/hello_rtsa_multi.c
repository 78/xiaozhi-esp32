/*************************************************************
 * File  :  test_rtsa_multi.c
 * Module:  RTSA Lite test application.
 *
 * This is a part of the Agora IoT SDK.
 * Copyright (C) 2022 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include "app_config.h"
#include <pthread.h>

#ifdef __APPLE__
#define SET_THREAD_NAME(name) pthread_setname_np(name)
#else
#define SET_THREAD_NAME(name) pthread_setname_np(pthread_self(), name)
#endif

typedef struct {
  app_config_t config;

  void *video_file_parser;
  void *video_file_writer;

  void *audio_file_parser;
  void *audio_file_writer;

  uint32_t conn_cnt;
  connection_id_t conn_ids[10];
  connection_info_t conn_infos[10];
  bool conn_flags[10];
  bool b_connected_flag;
  bool b_stop_flag;
} app_t;

static app_t g_app = {
    .config = {
        // common config
        .p_sdk_log_dir              = "io.agora.rtc_sdk",
        .p_appid                    = "",
        .p_channel                  = DEFAULT_CHANNEL_NAME,
        .p_token                    = "",
        .p_license                  = "",
        .uid                        = 0,
        .area                       = AREA_CODE_GLOB,

        // video related config
        .video_data_type            = VIDEO_DATA_TYPE_H264,
        .send_video_frame_rate      = DEFAULT_SEND_VIDEO_FRAME_RATE,
        .send_video_file_path       = DEFAULT_SEND_VIDEO_FILENAME,

        // audio related config
        .audio_data_type            = AUDIO_DATA_TYPE_PCM,
        .audio_codec_type           = AUDIO_CODEC_TYPE_OPUS,
        .send_audio_file_path       = DEFAULT_SEND_AUDIO_FILENAME,

        // pcm related config
        .pcm_sample_rate            = DEFAULT_PCM_SAMPLE_RATE,
        .pcm_channel_num            = DEFAULT_PCM_CHANNEL_NUM,
        .pcm_duration               = DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS,

        // advanced config
        .enable_audio_mixer         = false,
        .receive_data_only          = false,
        .local_ap                   = "",
        .domain_limit               = false,
        .conn_cnt                   = 3,
    },

    .video_file_parser      = NULL,
    .video_file_writer      = NULL,

    .audio_file_parser      = NULL,
    .audio_file_writer      = NULL,

    .b_stop_flag            = false,
    .b_connected_flag       = false,
};

static void app_signal_handler(int sig)
{
  LOGW("catch signal %d", sig);
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
  app_config_t *config = &g_app.config;
  parser_cfg_t parser_cfg = { 0 };

  signal(SIGINT, app_signal_handler);

  parser_cfg.u.audio_cfg.framePeriodMs = DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS;
  if (config->audio_data_type == AUDIO_DATA_TYPE_PCM) {
    parser_cfg.u.audio_cfg.sampleRateHz = config->pcm_sample_rate;
    parser_cfg.u.audio_cfg.numberOfChannels = config->pcm_channel_num;
  }

  g_app.video_file_parser =
          create_file_parser(video_data_type_to_file_type(config->video_data_type), config->send_video_file_path, NULL);
  if (!g_app.video_file_parser) {
    return -1;
  }

  // g_app.video_file_writer = create_file_writer(DEFAULT_RECV_VIDEO_BASENAME);

  g_app.audio_file_parser = create_file_parser(audio_data_type_to_file_type(config->audio_data_type),
                                               config->send_audio_file_path, &parser_cfg);
  if (!g_app.audio_file_parser) {
    return -1;
  }

  // g_app.audio_file_writer = create_file_writer(DEFAULT_RECV_AUDIO_BASENAME);

  return 0;
}

static int app_send_video(void)
{
  app_config_t *config = &g_app.config;
  int i = 0;

  frame_t frame;
  if (file_parser_obtain_frame(g_app.video_file_parser, &frame) < 0) {
    LOGE("The file parser failed to obtain audio frame");
    return -1;
  }

  // API: send vido data
  video_frame_info_t info = {0};
  info.frame_type = frame.u.video.is_key_frame ? VIDEO_FRAME_KEY : VIDEO_FRAME_DELTA;
  info.frame_rate = config->send_video_frame_rate;
  info.data_type = config->video_data_type;
  info.stream_type = VIDEO_STREAM_HIGH;

  for (i = 0; i < g_app.conn_cnt; i++) {
    if (!g_app.conn_flags[i]) {
      continue;
    }
    int rval = agora_rtc_send_video_data(g_app.conn_ids[i], frame.ptr, frame.len, &info);
    if (rval < 0) {
      LOGE("Failed to send video data, reason: %s", agora_rtc_err_2_str(rval));
    }
  }

  file_parser_release_frame(g_app.video_file_parser, &frame);
  return 0;
}

static void *video_send_thread(void *threadid)
{
  SET_THREAD_NAME("send-video");

  int video_send_interval_us = 1000000 / g_app.config.send_video_frame_rate;
  void *pacer = pacer_create(0, video_send_interval_us);

  while (!g_app.b_stop_flag) {
    if (g_app.b_connected_flag && is_time_to_send_video(pacer)) {
      app_send_video();
    }
    // sleep and wait until time is up for next send
    wait_before_next_send(pacer);
  }
  pacer_destroy(pacer);
  return NULL;
}


static int app_send_audio(void)
{
  app_config_t *config = &g_app.config;
  int i = 0;

  frame_t frame;
  if (file_parser_obtain_frame(g_app.audio_file_parser, &frame) < 0) {
    LOGE("The file parser failed to obtain audio frame");
    return -1;
  }

  // API: send audio data
  audio_frame_info_t info = { 0 };
  info.data_type = config->audio_data_type;

  for (i = 0; i < g_app.conn_cnt; i++) {
    if (!g_app.conn_flags[i]) {
      continue;
    }
    int rval = agora_rtc_send_audio_data(g_app.conn_ids[i], frame.ptr, frame.len, &info);
    if (rval < 0) {
      LOGE("Failed to send audio data, reason: %s", agora_rtc_err_2_str(rval));
    }
  }

  file_parser_release_frame(g_app.audio_file_parser, &frame);
  return 0;
}

static void *audio_send_thread(void *threadid)
{
  SET_THREAD_NAME("send-audio");

  int audio_send_interval_us = g_app.config.pcm_duration * 1000;
  if (g_app.config.audio_data_type == AUDIO_DATA_TYPE_AACLC_8K ||
      g_app.config.audio_data_type == AUDIO_DATA_TYPE_AACLC_16K ||
      g_app.config.audio_data_type == AUDIO_DATA_TYPE_AACLC ||
      g_app.config.audio_data_type == AUDIO_DATA_TYPE_HEAAC) {
    audio_send_interval_us = 1000000 * 1024 / g_app.config.pcm_sample_rate;
  }
  void *pacer = pacer_create(audio_send_interval_us, 0);

  while (!g_app.b_stop_flag) {
    if (g_app.b_connected_flag && is_time_to_send_audio(pacer)) {
      app_send_audio();
    }
    // sleep and wait until time is up for next send
    wait_before_next_send(pacer);
  }
  pacer_destroy(pacer);
  return NULL;
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed)
{
  int i = 0;
  for (i = 0; i < g_app.conn_cnt; i++) {
    if (conn_id == g_app.conn_ids[i]) {
      g_app.conn_flags[i] = true;
    }
  }
  g_app.b_connected_flag = true;
  LOGI("[conn-%u] Join the channel successfully, uid %u elapsed %d ms", conn_id, uid, elapsed);
}

static void __on_reconnecting(connection_id_t conn_id)
{
	LOGW("[conn-%u] connection timeout, reconnecting", conn_id);

  int i = 0;
  bool any_connected = false;
  for (i = 0; i < g_app.conn_cnt; i++) {
    if (conn_id == g_app.conn_ids[i]) {
      g_app.conn_flags[i] = false;
    }
    if (g_app.conn_flags[i] && !any_connected) {
      any_connected = true;
    }
  }
  if (!any_connected) {
    g_app.b_connected_flag = false;
  }
}

static void __on_connection_lost(connection_id_t conn_id)
{
  LOGW("[conn-%u] Lost connection from the channel", conn_id);

  int i = 0;
  bool any_connected = false;
  for (i = 0; i < g_app.conn_cnt; i++) {
    if (conn_id == g_app.conn_ids[i]) {
      g_app.conn_flags[i] = false;
    }
    if (g_app.conn_flags[i] && !any_connected) {
      any_connected = true;
    }
  }
  if (!any_connected) {
    g_app.b_connected_flag = false;
  }
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  int i = 0;
  for (i = 0; i < g_app.conn_cnt; i++) {
    if (conn_id == g_app.conn_ids[i]) {
      g_app.conn_flags[i] = true;
    }
  }
  g_app.b_connected_flag = true;
  LOGI("[conn-%u] Rejoin the channel successfully, uid %u elapsed %d ms", conn_id, uid, elapsed_ms);
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
  LOGI("[conn-%u] Remote user \"%u\" has joined the channel, elapsed %d ms", conn_id, uid, elapsed_ms);
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
  }

  if (code == ERR_INVALID_APP_ID) {
    LOGE("Invalid App ID. Please double check. Error msg \"%s\"", msg);
  } else if (code == ERR_INVALID_CHANNEL_NAME) {
    LOGE("Invalid channel name for conn_id %u. Please double check. Error msg \"%s\"", conn_id, msg);
  } else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
    LOGE("Invalid token. Please double check. Error msg \"%s\"", msg);
  } else if (code == ERR_DYNAMIC_TOKEN_BUT_USE_STATIC_KEY) {
    LOGE("Dynamic token is enabled but is not provided. Error msg \"%s\"", msg);
  } else {
    LOGW("Error %d is captured. Error msg \"%s\"", code, msg);
  }

  g_app.b_stop_flag = true;
}

static void __on_audio_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const audio_frame_info_t *info_ptr)
{
  LOGI("[conn-%u] on_audio_data: uid %u sent_ts %u data_type %d, len %zu", conn_id, uid, sent_ts,
           info_ptr->data_type, len);
  //write_file(g_app.audio_file_writer, data_type, data, len);
}

static void __on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                                  const audio_frame_info_t *info_ptr)
{
  //LOGD(TAG, "[conn-%u] on_mixed_audio_data: data_type %d, len %zu", conn_id, info_ptr->data_type, len);
  //write_file(g_app.audio_file_writer, data_type, data, len);
}

static void __on_video_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts, const void *data, size_t len,
                            const video_frame_info_t *info_ptr)
{
  LOGI("[conn-%u] on_video_data: uid %u sent_ts %u data_type %d frame_type %d stream_type %d len %zu", conn_id,
           uid, sent_ts, info_ptr->data_type, info_ptr->frame_type, info_ptr->stream_type, len);
  write_file(g_app.video_file_writer, info_ptr->data_type, data, len);
}

static void __on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
  LOGI("[conn-%u] Bandwidth change detected. Please adjust encoder bitrate to %u kbps", conn_id,
           target_bps / 1000);
}

static void __on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e stream_type)
{
  LOGW("[conn-%u] Frame loss detected. Please notify the encoder to generate key frame immediately", conn_id);
}

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler, app_config_t *config)
{
  event_handler->on_join_channel_success = __on_join_channel_success;
  event_handler->on_reconnecting = __on_reconnecting;
  event_handler->on_connection_lost = __on_connection_lost;
  event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
  event_handler->on_user_joined = __on_user_joined;
  event_handler->on_user_offline = __on_user_offline;
  event_handler->on_user_mute_audio = __on_user_mute_audio;
  event_handler->on_user_mute_video = __on_user_mute_video;
  event_handler->on_target_bitrate_changed = __on_target_bitrate_changed;
  event_handler->on_key_frame_gen_req = __on_key_frame_gen_req;
  event_handler->on_video_data = __on_video_data;
  event_handler->on_error = __on_error;

  if (config->enable_audio_mixer) {
    event_handler->on_mixed_audio_data = __on_mixed_audio_data;
  } else {
    event_handler->on_audio_data = __on_audio_data;
  }
}

int main(int argc, char **argv)
{
  app_config_t *config = &g_app.config;
  int rval = 0;
  int i = 0;

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
  service_opt.area_code = config->area;
  service_opt.log_cfg.log_path = config->p_sdk_log_dir;
  service_opt.log_cfg.log_disable = config->log_level == -1;
  service_opt.log_cfg.log_level = config->log_level;
  snprintf(service_opt.license_value, sizeof(service_opt.license_value), "%s", config->p_license);

  rval = agora_rtc_init(p_appid, &event_handler, &service_opt);
  if (rval < 0) {
    LOGE("Failed to initialize Agora sdk, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  agora_rtc_set_bwe_param(CONNECTION_ID_ALL, DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE,
                          DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE, DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE);

  g_app.conn_cnt = g_app.config.conn_cnt;
  for (i = 0; i < g_app.conn_cnt; i++) {
    // 4. API: Create connection
    rval = agora_rtc_create_connection(&g_app.conn_ids[i]);
    if (rval < 0) {
      LOGE("Failed to create connection, reason: %s", agora_rtc_err_2_str(rval));
      return -1;
    }
  }

  // 5. API: join channel
  rtc_channel_options_t channel_options;
  memset(&channel_options, 0, sizeof(channel_options));
  channel_options.auto_subscribe_audio = true;
  channel_options.auto_subscribe_video = true;

  if (config->audio_data_type == AUDIO_DATA_TYPE_PCM) {
    /* If we want to send PCM data instead of encoded audio like AAC or Opus, here please enable
     * audio codec, as well as configure the PCM sample rate and number of channels
     */
    channel_options.audio_codec_opt.audio_codec_type = config->audio_codec_type;
    channel_options.audio_codec_opt.pcm_sample_rate = config->pcm_sample_rate;
    channel_options.audio_codec_opt.pcm_channel_num = config->pcm_channel_num;
  }

  for (i = 0; i < g_app.conn_cnt; i++) {
    rval = agora_rtc_join_channel(g_app.conn_ids[i], config->p_channel, config->uid + i + 1, config->p_token,
                                  &channel_options);
    if (rval < 0) {
      LOGE("Failed to join channel \"%s\", reason: %s", config->p_channel, agora_rtc_err_2_str(rval));
      return -1;
    }
  }

  // 6. wait until we join channel successfully
  while (!g_app.b_connected_flag && !g_app.b_stop_flag) {
    usleep(100 * 1000);
  }

  for (i = 0; i < g_app.conn_cnt; i++) {
    agora_rtc_get_connection_info(g_app.conn_ids[i], &g_app.conn_infos[i]);
    LOGI("joined-after: conn_id=%u uid=%u cname=%s", g_app.conn_infos[i].conn_id, g_app.conn_infos[i].uid,
             g_app.conn_infos[i].channel_name);
  }

  // 7. create tasks sending video and audio frames
  pthread_t video_thread_id;
  rval = pthread_create(&video_thread_id, NULL, video_send_thread, 0);
  if (rval < 0) {
    printf("Unable to create video push thread\n");
    return -1;
  }

  pthread_t audio_thread_id;
  rval = pthread_create(&audio_thread_id, NULL, audio_send_thread, 0);
  if (rval < 0) {
    printf("Unable to create audio push thread\n");
    return -1;
  }

  pthread_join(video_thread_id, NULL);
  pthread_join(audio_thread_id, NULL);

  for (i = 0; i < g_app.conn_cnt; i++) {
    // 8. API: leave channel
    agora_rtc_leave_channel(g_app.conn_ids[i]);
    // 9: API: Destroy connection
    agora_rtc_destroy_connection(g_app.conn_ids[i]);
  }

  // 10. API: fini rtc sdk
  agora_rtc_fini();

  return 0;
}

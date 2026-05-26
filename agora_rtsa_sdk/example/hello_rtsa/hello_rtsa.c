/*************************************************************
 * File  :  hello_rtsa.c
 * Module:  Agora SD-RTN SDK RTC C API demo application.
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

#include "app_config.h"

typedef struct {
  app_config_t config;

  void *video_file_parser;
  void *video_file_writer;

  void *audio_file_parser;
  void *audio_file_writer;

  connection_id_t conn_id;
  bool b_stop_flag;
  bool b_connected_flag;
} app_t;

static app_t g_app = {
    .config = {
        // common config
        .p_sdk_log_dir              = "io.agora.rtc_sdk",
        .log_level                  = RTC_LOG_INFO,
        .p_appid                    = "",
        .p_channel                  = DEFAULT_CHANNEL_NAME,
        .p_token                    = "",
				.p_license                  = "",
        .uid                        = 0,
        .uname                      = NULL,
        .area                       = AREA_CODE_GLOB,

        // video related config
        .video_data_type            = VIDEO_DATA_TYPE_H264,
        .send_video_frame_rate      = DEFAULT_SEND_VIDEO_FRAME_RATE,
        .send_video_file_path       = NULL,

        // audio related config
        .audio_data_type            = AUDIO_DATA_TYPE_PCM,
        .audio_codec_type           = AUDIO_CODEC_TYPE_OPUS,
        .send_audio_file_path       = NULL,

        // pcm related config
        .pcm_sample_rate            = DEFAULT_PCM_SAMPLE_RATE,
        .pcm_channel_num            = DEFAULT_PCM_CHANNEL_NUM,
        .pcm_duration               = DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS,

        // advanced config
        .enable_audio_mixer         = false,
        .receive_data_only          = false,
				.local_ap                   = "",
				.domain_limit               = false,
        .lan_accelerate             = false,
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

	if (!config->receive_data_only) {
    g_app.video_file_parser =
        create_file_parser(video_data_type_to_file_type(config->video_data_type), config->send_video_file_path, NULL);
    if (!g_app.video_file_parser) {
      LOGE("create file parser for video failed");
      return -1;
    }

		parser_cfg.u.audio_cfg.sampleRateHz = config->pcm_sample_rate;
		parser_cfg.u.audio_cfg.numberOfChannels = config->pcm_channel_num;
		parser_cfg.u.audio_cfg.framePeriodMs = config->pcm_duration;

    g_app.audio_file_parser =
        create_file_parser(audio_data_type_to_file_type(config->audio_data_type), config->send_audio_file_path, &parser_cfg);
    if (!g_app.audio_file_parser) {
      LOGE("create file parser for audio failed");
      return -1;
    }
  }

  g_app.video_file_writer = create_file_writer(FILE_TYPE_VIDEO, DEFAULT_RECV_VIDEO_BASENAME);
  g_app.audio_file_writer = create_file_writer(FILE_TYPE_AUDIO, DEFAULT_RECV_AUDIO_BASENAME);

  return 0;
}

static int app_send_audio(void)
{
  app_config_t *config = &g_app.config;
  audio_frame_info_t info = { 0 };
  info.data_type = config->audio_data_type;
  frame_t frame;

  if (file_parser_obtain_frame(g_app.audio_file_parser, &frame) < 0) {
    LOGE("The file parser failed to obtain audio frame");
    return -1;
  }

  // API: send audio data
  int rval = agora_rtc_send_audio_data(g_app.conn_id, frame.ptr, frame.len, &info);
  if (rval < 0) {
    LOGE("Failed to send audio data, reason: %s", agora_rtc_err_2_str(rval));
    file_parser_release_frame(g_app.audio_file_parser, &frame);
    return -1;
  }

  file_parser_release_frame(g_app.audio_file_parser, &frame);
  return 0;
}

static int app_send_video(void)
{
  app_config_t *config = &g_app.config;

  frame_t frame;
  if (file_parser_obtain_frame(g_app.video_file_parser, &frame) < 0) {
    LOGE("The file parser failed to obtain video frame");
    return -1;
  }

	// API: send vido data
	video_frame_info_t info = {0};
	info.frame_type = frame.u.video.is_key_frame ? VIDEO_FRAME_KEY : VIDEO_FRAME_DELTA;
	info.frame_rate = config->send_video_frame_rate;
	info.data_type = config->video_data_type;
	info.stream_type = VIDEO_STREAM_HIGH;
	int rval = agora_rtc_send_video_data(g_app.conn_id, frame.ptr, frame.len, &info);
	if (rval < 0) {
		LOGE("Failed to send video data, reason: %s", agora_rtc_err_2_str(rval));
		file_parser_release_frame(g_app.video_file_parser, &frame);
		return -1;
	}

  file_parser_release_frame(g_app.video_file_parser, &frame);
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
  LOGI("[conn-%u] Remote user \"%u\" has joined the channel, elapsed %d ms", conn_id, uid, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
  LOGI("[conn-%u] Remote user \"%u\" has left the channel, reason %d", conn_id, uid, reason);
}

static void __on_user_joined_with_user_account(connection_id_t conn_id, const user_info_t *user, int elapsed_ms)
{
  LOGI("[conn-%u] Remote user %s - %u has joined the channel, elapsed %d ms", conn_id, user->user_account, user->uid, elapsed_ms);
}

static void __on_user_offline_with_user_account(connection_id_t conn_id, const user_info_t *user, int reason)
{
  LOGI("[conn-%u] Remote user %s - %u has left the channel, reason %d", conn_id, user->user_account, user->uid, reason);
}

static void __on_user_info_updated(connection_id_t conn_id, const user_info_t *user)
{
  LOGI("[conn-%u] Remote user info updated: %s - %u", conn_id, user->user_account, user->uid);
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

static void __on_license_failed(connection_id_t conn_id, int reason)
{
  LOGE("License verified failed, reason: %d", reason);
  g_app.b_stop_flag = true;
}

static void __on_audio_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts,
                            const void *data, size_t len, const audio_frame_info_t *info_ptr)
{
  LOGD("[conn-%u] on_audio_data: uid %u sent_ts %u data_type %d, len %zu", conn_id, uid, sent_ts,
       info_ptr->data_type, len);
  write_file(g_app.audio_file_writer, info_ptr->data_type, data, len);
}

static void __on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                                  const audio_frame_info_t *info_ptr)
{
  LOGD("[conn-%u] on_mixed_audio_data: data_type %d, len %zu", conn_id, info_ptr->data_type, len);
  write_file(g_app.audio_file_writer, info_ptr->data_type, data, len);
}

static void __on_video_data(connection_id_t conn_id, const uint32_t uid, uint16_t sent_ts,
                            const void *data, size_t len, const video_frame_info_t *info_ptr)
{
  LOGD("[conn-%u] on_video_data: uid %u sent_ts %u data_type %d frame_type %d stream_type %d len %zu",
       conn_id, uid, sent_ts, info_ptr->data_type, info_ptr->frame_type, info_ptr->stream_type, len);
  write_file(g_app.video_file_writer, info_ptr->data_type, data, len);
}

static void __on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps)
{
  LOGI("[conn-%u] Bandwidth change detected. Please adjust encoder bitrate to %u kbps", conn_id,
       target_bps / 1000);
}

static void __on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid,
                                   video_stream_type_e stream_type)
{
  LOGW("[conn-%u] Frame loss detected. Please notify the encoder to generate key frame immediately",
       conn_id);
}

static void __on_rtc_stats(connection_id_t conn_id, rtc_stats_t stats)
{
  if (g_app.config.lan_accelerate) {
    LOGI("[conn-%u] rtc_stats: lan_accelerate=%d", conn_id, stats.lan_accelerate_state);
  }
}

static void app_init_event_handler(agora_rtc_event_handler_t *event_handler, app_config_t *config)
{
	event_handler->on_join_channel_success = __on_join_channel_success;
	event_handler->on_reconnecting = __on_reconnecting,
	event_handler->on_connection_lost = __on_connection_lost;
	event_handler->on_rejoin_channel_success = __on_rejoin_channel_success;
	event_handler->on_user_joined = __on_user_joined;
	event_handler->on_user_offline = __on_user_offline;
  if (config->uname) {
  event_handler->on_user_joined_with_user_account = __on_user_joined_with_user_account;
  event_handler->on_user_offline_with_user_account = __on_user_offline_with_user_account;
  event_handler->on_user_info_updated = __on_user_info_updated;
  }
	event_handler->on_user_mute_audio = __on_user_mute_audio;
	event_handler->on_user_mute_video = __on_user_mute_video;
  event_handler->on_target_bitrate_changed = __on_target_bitrate_changed;
  event_handler->on_key_frame_gen_req = __on_key_frame_gen_req;
  event_handler->on_video_data = __on_video_data;
  event_handler->on_error = __on_error;
  event_handler->on_license_validation_failure = __on_license_failed;
  event_handler->on_rtc_stats = __on_rtc_stats;

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
  char priv_params[512] = { 0 };

	LOGI("Welcome to RTSA SDK v%s", agora_rtc_get_version());

  // 0. app parse args
  rval = app_parse_args(argc, argv, config);
  if (rval < 0) {
    app_print_usage(argc, argv);
    return -1;
  }

  app_print_config(config);

  // 1. app init
  rval = app_init();
  if (rval < 0) {
    LOGE("Failed to initialize application");
    return -1;
  }

  // 2. API: init agora rtc sdk
  int appid_len = strlen(config->p_appid);
  void *p_appid = (void *)(appid_len == 0 ? NULL : config->p_appid);

  agora_rtc_event_handler_t event_handler = { 0 };
  app_init_event_handler(&event_handler, config);

  rtc_service_option_t service_opt = { 0 };
  service_opt.area_code = config->area;
  service_opt.log_cfg.log_path = config->p_sdk_log_dir;
  service_opt.log_cfg.log_disable = config->log_level == -1;
  service_opt.log_cfg.log_level = config->log_level;
  service_opt.use_string_uid = config->uname ? true : false;
  snprintf(service_opt.license_value, sizeof(service_opt.license_value), "%s", config->p_license);

  rval = agora_rtc_init(p_appid, &event_handler, &service_opt);
  if (rval < 0) {
    LOGE("Failed to initialize Agora sdk, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  // 3. Set Global private parameters
  if (config->local_ap && config->local_ap[0]) {
    snprintf(priv_params, sizeof(priv_params), "{\"rtc.local_ap_list\": %s}", config->local_ap);
    rval = agora_rtc_set_params(CONNECTION_ID_ALL, priv_params);
    if (rval != 0) {
      LOGE("set %s failed, reason: %s", priv_params, agora_rtc_err_2_str(rval));
      return -1;
    }
  }

#if 0 // can set mtu when network mtu is small to avoid udp fragment
  snprintf(priv_params, sizeof(priv_params), "{\"rtc.net_mtu\": %d}", 1260);
  rval = agora_rtc_set_params(CONNECTION_ID_ALL, priv_params);
  if (rval != 0) {
    LOGE("set %s failed, reason: %s", priv_params, agora_rtc_err_2_str(rval));
    return -1;
  }
#endif

  // 4. API: Create connection
  rval = agora_rtc_create_connection(&g_app.conn_id);
  if (rval < 0) {
    LOGE("Failed to create connection, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  // 5. API: Set Connection config
  rval = agora_rtc_set_bwe_param(g_app.conn_id, DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE,
                                 DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE,
                                 DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE);
  if (rval != 0) {
    LOGE("Failed set bwe param, reason: %s", agora_rtc_err_2_str(rval));
    return -1;
  }

  // 6. API: join channel
  int token_len = strlen(config->p_token);
  void *p_token = (void *)(token_len == 0 ? NULL : config->p_token);

  rtc_channel_options_t channel_options = { 0 };
  memset(&channel_options, 0, sizeof(channel_options));
  channel_options.auto_subscribe_audio = true;
  channel_options.auto_subscribe_video = true;
  channel_options.enable_audio_jitter_buffer = config->enable_audio_jitter_buffer;
  channel_options.enable_audio_mixer = config->enable_audio_mixer;
  channel_options.enable_audio_decode = config->enable_audio_decode;
  channel_options.enable_audio_ai_qos = config->enable_audio_ai_qos;

  channel_options.audio_codec_opt.pcm_duration = config->pcm_duration;
  if (config->audio_data_type == AUDIO_DATA_TYPE_PCM) {
    /* If we want to send PCM data instead of encoded audio like AAC or Opus, here please enable
           audio codec, as well as configure the PCM sample rate and number of channels */
    channel_options.audio_codec_opt.audio_codec_type = config->audio_codec_type;
    channel_options.audio_codec_opt.pcm_sample_rate = config->pcm_sample_rate;
    channel_options.audio_codec_opt.pcm_channel_num = config->pcm_channel_num;
  }

#if 0 // enable media encryption
  // get key from server by cmd: openssl rand -hex 32
  const char *key_str = "dba643c8ba6b6dc738df43d9fd624293b4b12d87a60f518253bd10ba98c48453";
  // get salt from server by cmd: openssl rand -base64 32
  const char *salt_base64_str = "X5w9T+50kzxVOnkJKiY/lUk82/bES2kATOt3vBuGEDw=";
  // enable
  channel_options.crypto_opt.enable = true;
  // mode
  channel_options.crypto_opt.mode = AES_128_GCM2;
  // key
  sprintf(channel_options.crypto_opt.key, "%s", key_str);
  // salt
  uint8_t *salt_bytes = NULL;
  uint32_t salt_bytes_len = 0;
  salt_bytes = util_base64_decode(salt_base64_str, strlen(salt_base64_str), &salt_bytes_len);
  memcpy(channel_options.crypto_opt.salt, salt_bytes, salt_bytes_len);
  free(salt_bytes);
#endif

  if (!config->uname) {
    rval = agora_rtc_join_channel(g_app.conn_id, config->p_channel, config->uid, p_token, &channel_options);
  } else {
    rval = agora_rtc_join_channel_with_user_account(g_app.conn_id, config->p_channel, config->uname, p_token,
                                                    &channel_options);
  }
  if (rval < 0) {
    LOGE("Failed to join channel \"%s\", reason: %s", config->p_channel, agora_rtc_err_2_str(rval));
    return -1;
  }

  // 7. Set subscription rules for audio and video for a specific user
  //    When you set auto_subscribe false by agora_rtc_mute_remote_xxx.
  //    example:
  //      agora_rtc_mute_remote_audio(g_app.conn_id, remote_uid, false);
  //      agora_rtc_mute_remote_video(g_app.conn_id, remote_uid, false);

  // 8. wait until we join channel successfully
  while (!g_app.b_connected_flag && !g_app.b_stop_flag) {
    usleep(100 * 1000);
  }

  // 9. send video and audio data in a loop
  int audio_send_interval_us = config->pcm_duration*1000;
  int video_send_interval_us = 1000000 / config->send_video_frame_rate;
  if (config->audio_data_type == AUDIO_DATA_TYPE_AACLC_8K ||
      config->audio_data_type == AUDIO_DATA_TYPE_AACLC_16K ||
      config->audio_data_type == AUDIO_DATA_TYPE_AACLC ||
      config->audio_data_type == AUDIO_DATA_TYPE_HEAAC) {
    audio_send_interval_us = 1000000 * 1024 / config->pcm_sample_rate;
  }
  void *pacer = pacer_create(audio_send_interval_us, video_send_interval_us);

  while (!g_app.b_stop_flag) {
    // skip sending data if receive_data_only flag is on
    if (config->receive_data_only) {
      util_sleep_ms(1000);
      continue;
    }
    if (g_app.b_connected_flag && is_time_to_send_audio(pacer)) {
      app_send_audio();
    }
    if (g_app.b_connected_flag && is_time_to_send_video(pacer)) {
      app_send_video();
    }
    // sleep and wait until time is up for next send
    wait_before_next_send(pacer);
  }

  // 10. API: leave channel
  agora_rtc_leave_channel(g_app.conn_id);

  // 11: API: Destroy connection
  agora_rtc_destroy_connection(g_app.conn_id);

  // 12. API: fini rtc sdk
  agora_rtc_fini();

  // 13. app clean up
  pacer_destroy(pacer);
  return 0;
}

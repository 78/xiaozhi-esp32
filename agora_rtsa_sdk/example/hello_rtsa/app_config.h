#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "agora_rtc_api.h"
#include "file_parser.h"
#include "file_writer.h"
#include "utility.h"
#include "pacer.h"
#include "log.h"

#define DEFAULT_CHANNEL_NAME "hello_demo"
#define DEFAULT_CERTIFACTE_FILENAME "certificate.bin"
#define DEFAULT_TEST_DATA_DIR "../../test_data/"
#define DEFAULT_SEND_AUDIO_FILENAME_PCM_16K DEFAULT_TEST_DATA_DIR"send_audio_16k_1ch.pcm"
#define DEFAULT_SEND_AUDIO_FILENAME_PCM_08K DEFAULT_TEST_DATA_DIR"send_audio_8k_1ch.pcm"
#define DEFAULT_SEND_VIDEO_FILENAME DEFAULT_TEST_DATA_DIR"send_video.h264"
#define DEFAULT_SEND_AUDIO_FILENAME DEFAULT_TEST_DATA_DIR"send_audio_16k_1ch.pcm"
#define DEFAULT_SEND_AUDIO_BASENAME DEFAULT_TEST_DATA_DIR"send_audio"
#define DEFAULT_SEND_VIDEO_BASENAME DEFAULT_TEST_DATA_DIR"send_video"
#define DEFAULT_RECV_AUDIO_BASENAME "recv_audio"
#define DEFAULT_RECV_VIDEO_BASENAME "recv_video"
#define DEFAULT_SEND_VIDEO_FRAME_RATE (25)
#define DEFAULT_BANDWIDTH_ESTIMATE_MIN_BITRATE (100000)
#define DEFAULT_BANDWIDTH_ESTIMATE_MAX_BITRATE (1000000)
#define DEFAULT_BANDWIDTH_ESTIMATE_START_BITRATE (500000)
#define DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS (20)
#define DEFAULT_PCM_SAMPLE_RATE (16000)
#define DEFAULT_PCM_CHANNEL_NUM (1)

typedef struct {
  // common config
  const char *p_sdk_log_dir;
  int32_t  log_level;
  const char *p_appid;
  const char *p_token;
  const char *p_channel;
  const char *p_license;
  uint32_t uid;
  const char *uname;
  uint32_t area;

  // video related config
  video_data_type_e video_data_type;
  int send_video_frame_rate;
  const char *send_video_file_path;

  // audio related config
  audio_data_type_e audio_data_type;
  audio_codec_type_e audio_codec_type;
  const char *send_audio_file_path;
  uint32_t pcm_sample_rate;
  uint32_t pcm_channel_num;
  uint32_t pcm_duration;

  // advanced config
  bool enable_audio_jitter_buffer;
  bool enable_audio_mixer;
  bool enable_audio_decode;
  bool enable_audio_ai_qos;
  bool receive_data_only;
  const char *local_ap;
  bool domain_limit;
  bool lan_accelerate;
  int conn_cnt;
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
  LOGS(" -U, --user-name           : user name");
  LOGS(" -l, --license             : license value MUST be set when release");
  LOGS(" -L, --log-level           : set log level. if set -1 mean disable log");
  LOGS(" -v, --video-type          : video data type for the input video file; default is 2");
  LOGS("                             support: 2=H264, 3=H265, 20=JPEG");
  LOGS(" -a, --audio-type          : audio data type for the input audio file; default is 100=pcm");
  LOGS("                             support: 3=PCMA 4=PCMU 5=G722 6=AACLC-8K 7=AACLC-16K 8=AACLC-48K 9=HEAAC-32K 100=PCM");
  LOGS(" -C, --audio-codec         : audio codec type; only valid when audio type is PCM; default is 1=opus");
  LOGS("                             support: 0:Disable 1=OPUS, 2=G722 3=G711A 4=G711U");
  LOGS(" -f  --fps                 : video frame rate; default is 30");
  LOGS(" -s, --send-video-file     : send video file path; default is '%s' (run from out/ directory)",
       DEFAULT_SEND_VIDEO_FILENAME);
  LOGS(" -S, --send-audio-file     : send audio file path; default is '%s' (run from out/ directory)",
       DEFAULT_SEND_AUDIO_FILENAME);
  LOGS(" -A, --area                : hex format with 0x header, supported area_code list:");
  LOGS("                             CN (Mainland China) : 0x00000001");
  LOGS("                             NA (North America)  : 0x00000002");
  LOGS("                             EU (Europe)         : 0x00000004");
  LOGS("                             AS (Asia)           : 0x00000008");
  LOGS("                             JP (Japan)          : 0x00000010");
  LOGS("                             IN (India)          : 0x00000020");
  LOGS("                             OC (Oceania)        : 0x00000040");
  LOGS("                             SA (South-American) : 0x00000080");
  LOGS("                             AF (Africa)         : 0x00000100");
  LOGS("                             KR (South Korea)    : 0x00000200");
  LOGS("                             OVS (Global except China): 0xFFFFFFFE");
  LOGS("                             GLOB (Global)       : 0xFFFFFFFF");
  LOGS(" -j, --audio-jitter-buffer : enable audio jitter buffer");
  LOGS(" -m, --audio-mixer         : enable audio mixer to mix multiple incoming audio streams");
  LOGS(" -d, --audio-decode        : enable audio decode");
  LOGS(" -R, --recv-only           : do not send video and audio data");
  LOGS(" -D, --domain-limit        : domain limit is enabled");

  // long options
  LOGS(" --pcm-sample-rate         : sample rate for the input PCM data; only valid when audio type is PCM");
  LOGS(" --pcm-channel-num         : channel number for the input PCM data; only valid when audio type is PCM");
  LOGS(" --pcm-duration            : sample duration; default is %d", DEFAULT_SEND_AUDIO_FRAME_PERIOD_MS);
  LOGS(" --lan-accelerate          : enable lan accelerate");
  LOGS(" --audio-ai-qos            : enable audio ai qos feature");
  LOGS(" --local-ap                : params_str = {\"ipList\": [\"ip1\", \"ip2\"], \"domainList\":[\"domain1\", \"domain2\"], \"mode\": 1}");
  LOGS("                             mode: 0: ConnectivityFirst, 1: LocalOnly");
  LOGS("\nExample:");
  LOGS("    %s --app-id xxx [--token xxx] --channel-id xxx --send-video-file ./video.h264 --fps 15 \
--send-audio-file ./audio.pcm --license <your license>",
       argv[0]);
}

static void app_print_config(app_config_t *config) {
	LOGS("---------------app config show start---------------------");
	LOGS("<common config info>:");
	LOGS("  appid                   : %s", config->p_appid);
	LOGS("  channel                 : %s", config->p_channel);
	LOGS("  token                   : %s", config->p_token);
	LOGS("  uid                     : %u", config->uid);
  LOGS("  uname                   : %s", config->uname);
	LOGS("  area                    : 0x%x", config->area);
  LOGS("  log-level               : %d", config->log_level);
	LOGS("<video config info>       -");
	LOGS("  video_data_type         : %d", config->video_data_type);
	LOGS("  send_video_frame_rate   : %d", config->send_video_frame_rate);
	LOGS("  send_video_file_path    : %s", config->send_video_file_path);
	LOGS("<audio config info>       -");
	LOGS("  audio_data_type         : %d", config->audio_data_type);
	LOGS("  audio_codec_type        : %d", config->audio_codec_type);
	LOGS("  pcm_sample_rate         : %u", config->pcm_sample_rate);
  LOGS("  pcm_duration            : %u", config->pcm_duration);
  LOGS("  enable_audio_jitter_buffer : %d", config->enable_audio_jitter_buffer);
  LOGS("  enable_audio_mixer      : %d", config->enable_audio_mixer);
  LOGS("  enable_audio_decode     : %d", config->enable_audio_decode);
  LOGS("  enable_audio_ai_qos     : %d", config->enable_audio_ai_qos);
	LOGS("  send_audio_file_path    : %s", config->send_audio_file_path);
	LOGS("<advanced config info>    -");
	LOGS("  received_data_only      : %d", config->receive_data_only);
  LOGS("  lan-accelerate          : %d", config->lan_accelerate);
	LOGS("---------------app config show end-----------------------");
}

static int app_parse_args(int argc, char **argv, app_config_t *config)
{
  enum {
    LONG_OPT_LOCAL_AP = 1,
    LONG_OPT_CONN_CNT,
    LONG_OPT_LAN_ACCELERATE,
    LONG_OPT_PCM_SAMPLE_RATE,
    LONG_OPT_PCM_CHANNEL_NUM,
    LONG_OPT_PCM_DURATION,
    LONG_OPT_AUDIO_AI_QOS,
  };
  const char *av_short_option = "hi:t:c:u:U:v:a:C:f:S:s:A:gmRl:dDdL:j";
  int av_option_flag = 0;
  const struct option av_long_option[] = { { "help", 0, NULL, 'h' },
                                           { "app-id", 1, NULL, 'i' },
                                           { "token", 1, NULL, 't' },
                                           { "channel-id", 1, NULL, 'c' },
                                           { "user-id", 1, NULL, 'u' },
                                           { "user-name", 1, NULL, 'U' },
                                           { "video-type", 1, NULL, 'v' },
                                           { "audio-type", 1, NULL, 'a' },
                                           { "audio-codec", 1, NULL, 'C' },
                                           { "fps", 1, NULL, 'f' },
                                           { "send-audio-file", 1, NULL, 'S' },
                                           { "send-video-file", 1, NULL, 's' },
                                           { "area", 0, NULL, 'A' },
                                           { "audio-jitter-buffer", 0, NULL, 'j' },
                                           { "audio-mixer", 0, NULL, 'm' },
                                           { "audio-decode", 0, NULL, 'd' },
                                           { "recv-only", 0, NULL, 'R' },
                                           { "license", 1, NULL, 'l' },
                                           { "log-level", 1, NULL, 'L' },
                                           { "domain-limit", 0, NULL, 'D' },
                                           { "local-ap", 1, &av_option_flag, LONG_OPT_LOCAL_AP },
                                           { "conn-cnt", 1, &av_option_flag, LONG_OPT_CONN_CNT },
                                           { "lan-accelerate", 0, &av_option_flag, LONG_OPT_LAN_ACCELERATE },
                                           { "pcm-sample-rate", 1, &av_option_flag, LONG_OPT_PCM_SAMPLE_RATE },
                                           { "pcm-channel-num", 1, &av_option_flag, LONG_OPT_PCM_CHANNEL_NUM },
                                           { "pcm-duration", 1, &av_option_flag, LONG_OPT_PCM_DURATION },
                                           { "audio-ai-qos", 0, &av_option_flag, LONG_OPT_AUDIO_AI_QOS },
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
    case 'U':
      config->uname = optarg;
      break;
    case 'v':
      config->video_data_type = strtol(optarg, NULL, 10);
      break;
    case 'a':
      config->audio_data_type = strtol(optarg, NULL, 10);
      break;
    case 'C':
      config->audio_codec_type = strtol(optarg, NULL, 10);
      break;
    case 'f':
      config->send_video_frame_rate = strtol(optarg, NULL, 10);
      break;
    case 'S':
      config->send_audio_file_path = optarg;
      break;
    case 's':
      config->send_video_file_path = optarg;
      break;
    case 'A':
      config->area = strtol(optarg, NULL, 16);
      break;
    case 'j':
      config->enable_audio_jitter_buffer = true;
      break;
    case 'm':
      config->enable_audio_mixer = true;
      break;
    case 'd':
      config->enable_audio_decode = true;
      break;
    case 'R':
      config->receive_data_only = true;
      break;
    case 'l':
      config->p_license = optarg;
      break;
    case 'D':
      config->domain_limit = true;
      break;
    case 'L':
      config->log_level = strtol(optarg, NULL, 10);
      break;
    case 0:
      /* Pure long options */
      switch (av_option_flag) {
      case LONG_OPT_LOCAL_AP: // local-ap
        config->local_ap = optarg;
        break;
      case LONG_OPT_CONN_CNT: // conn-cnt
        config->conn_cnt = strtoul(optarg, NULL, 10);
        config->conn_cnt = config->conn_cnt <= 5 ? config->conn_cnt : 5;
        break;
      case LONG_OPT_LAN_ACCELERATE: // lan-accelerate
        config->lan_accelerate = true;
        break;
      case LONG_OPT_PCM_SAMPLE_RATE:
        config->pcm_sample_rate = atoi(optarg);
        break;
      case LONG_OPT_PCM_CHANNEL_NUM:
        config->pcm_channel_num = atoi(optarg);
        break;
      case LONG_OPT_PCM_DURATION:
        config->pcm_duration = strtol(optarg, NULL, 10);
        break;
      case LONG_OPT_AUDIO_AI_QOS:
        config->enable_audio_ai_qos = true;
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

  if (config->send_video_frame_rate <= 0) {
    LOGE("Invalid video frame rate: %d", config->send_video_frame_rate);
    return -1;
  }

  // Get send video filepath
  if (config->send_video_file_path == NULL) {
    switch (config->video_data_type) {
      case VIDEO_DATA_TYPE_H264:
        config->send_video_file_path = DEFAULT_SEND_VIDEO_BASENAME".h264";
        break;
      case VIDEO_DATA_TYPE_H265:
        config->send_video_file_path = DEFAULT_SEND_VIDEO_BASENAME".h265";
        break;
      case VIDEO_DATA_TYPE_GENERIC_JPEG:
        config->send_video_file_path = DEFAULT_SEND_VIDEO_BASENAME".jpeg";
        break;
      default:
        LOGE("Invalid video data type %d", config->video_data_type);
        exit(1);
      }
  }

  // Get send audio filepath
  if (config->send_audio_file_path == NULL) {
    switch (config->audio_data_type) {
    case AUDIO_DATA_TYPE_OPUS:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME".opus";
      break;
    case AUDIO_DATA_TYPE_OPUSFB:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME".opus";
      break;
    case AUDIO_DATA_TYPE_PCMA:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME".pcma";
      break;
    case AUDIO_DATA_TYPE_PCMU:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME".pcmu";
      break;
    case AUDIO_DATA_TYPE_G722:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME".g722";
      break;
    case AUDIO_DATA_TYPE_AACLC:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME"_48k.aac";
      break;
    case AUDIO_DATA_TYPE_HEAAC:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME"_32k.aac";
      break;
    case AUDIO_DATA_TYPE_AACLC_8K:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME"_8k.aac";
      break;
    case AUDIO_DATA_TYPE_AACLC_16K:
      config->send_audio_file_path = DEFAULT_SEND_AUDIO_BASENAME"_16k.aac";
      break;
    case AUDIO_DATA_TYPE_PCM:
      // sample_rate is default 16k except g711u is 8k
      if (config->audio_codec_type == AUDIO_CODEC_TYPE_G711A ||
          config->audio_codec_type == AUDIO_CODEC_TYPE_G711U) {
        config->send_audio_file_path = DEFAULT_SEND_AUDIO_FILENAME_PCM_08K;
      } else {
        config->send_audio_file_path = DEFAULT_SEND_AUDIO_FILENAME_PCM_16K;
      }
      break;
    default:
      LOGE("Invalid audio data type %d", config->audio_data_type);
      exit(1);
    }
  }

  // Fix pcm_sample_rate from audio_data_type
  switch (config->audio_data_type) {
  case AUDIO_DATA_TYPE_OPUS:
    break;
  case AUDIO_DATA_TYPE_OPUSFB:
    config->pcm_sample_rate = 48000;
    break;
  case AUDIO_DATA_TYPE_PCMA:
    config->pcm_sample_rate = 8000;
    break;
  case AUDIO_DATA_TYPE_PCMU:
    config->pcm_sample_rate = 8000;
    break;
  case AUDIO_DATA_TYPE_G722:
    config->pcm_sample_rate = 16000;
    break;
  case AUDIO_DATA_TYPE_AACLC:
    config->pcm_sample_rate = 48000;
    break;
  case AUDIO_DATA_TYPE_HEAAC:
    config->pcm_sample_rate = 32000;
    break;
  case AUDIO_DATA_TYPE_AACLC_8K:
    config->pcm_sample_rate = 8000;
    break;
  case AUDIO_DATA_TYPE_AACLC_16K:
    config->pcm_sample_rate = 16000;
    break;
  case AUDIO_DATA_TYPE_PCM:
    // sample_rate is default 16k except g711u is 8k
    if (config->audio_codec_type == AUDIO_CODEC_TYPE_G711A ||
        config->audio_codec_type == AUDIO_CODEC_TYPE_G711U) {
      config->pcm_sample_rate = 8000;
    } else {
      config->pcm_sample_rate = 16000;
    }
    break;
  default:
    break;
  }

  return 0;
}

static media_file_type_e video_data_type_to_file_type(video_data_type_e type)
{
  media_file_type_e file_type;
  switch (type) {
  case VIDEO_DATA_TYPE_H264:
    file_type = MEDIA_FILE_TYPE_H264;
    break;
	case VIDEO_DATA_TYPE_H265:
		file_type = MEDIA_FILE_TYPE_H265;
		break;
	case VIDEO_DATA_TYPE_GENERIC_JPEG:
		file_type = MEDIA_FILE_TYPE_JPEG;
		break;
  default:
    file_type = MEDIA_FILE_TYPE_H264;
    break;
  }
  return file_type;
}

static media_file_type_e audio_data_type_to_file_type(audio_data_type_e type)
{
  media_file_type_e file_type;
  switch (type) {
  case AUDIO_DATA_TYPE_PCM:
    file_type = MEDIA_FILE_TYPE_PCM;
    break;
  case AUDIO_DATA_TYPE_OPUS:
  case AUDIO_DATA_TYPE_OPUSFB:
    file_type = MEDIA_FILE_TYPE_OPUS;
    break;
  case AUDIO_DATA_TYPE_AACLC:
  case AUDIO_DATA_TYPE_HEAAC:
  case AUDIO_DATA_TYPE_AACLC_8K:
  case AUDIO_DATA_TYPE_AACLC_16K:
    file_type = MEDIA_FILE_TYPE_AACLC;
    break;
  case AUDIO_DATA_TYPE_G722:
    file_type = MEDIA_FILE_TYPE_G722;
    break;
  case AUDIO_DATA_TYPE_PCMA:
  case AUDIO_DATA_TYPE_PCMU:
    file_type = MEDIA_FILE_TYPE_G711;
    break;
  default:
    file_type = MEDIA_FILE_TYPE_PCM;
    break;
  }
  return file_type;
}

#endif

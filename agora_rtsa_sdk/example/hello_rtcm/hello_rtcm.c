/*************************************************************
 * File  :  hello_rtsa.c
 * Module:  Agora SD-RTN SDK RTC C API demo application.
 *
 * This is a part of the Agora RTC Service SDK.
 * Copyright (C) 2020 Agora IO
 * All rights reserved.
 *
 *************************************************************/

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
#include <sys/types.h>
#include <sys/stat.h>

#include "agora_rtc_api.h"

#define TAG_APP "[app]"
#define TAG_API "[api]"
#define TAG_EVENT "[event]"

#define INVALID_FD -1

#define DEFAULT_CHANNEL_NAME "demo"
#define LOG_DIR_MAX_LENGTH			(256)
#define SEND_MESSAGE_MAX_LENGTH		(256)

#define LOGS(fmt, ...) fprintf(stdout, "" fmt "\n", ##__VA_ARGS__)
#define LOGI(fmt, ...)                                                                             \
	fprintf(stdout, "I/ " fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...)                                                                             \
	fprintf(stdout, "D/ " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...)                                                                             \
	fprintf(stdout, "E/ " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...)                                                                             \
	fprintf(stdout, "W/ " fmt "\n", ##__VA_ARGS__)

typedef struct {
	const char *p_sdk_log_dir;
	const char *p_appid;
	const char *p_token;
	const char *p_channel;
	uint32_t    uid;
	uint32_t    peerid;
	uint32_t    conn_id;
	// send or receive custom message
	int			is_enable_send;
	const char 	*p_send_str;
	int			is_enbale_receive;
} app_config_t;

typedef struct {
	app_config_t config;
	int32_t b_stop_flag;
	int32_t b_join_success_flag;
} app_t;

static app_t g_app_instance = {
    .config = {
        .p_sdk_log_dir              =   "io.agora.rtc_sdk",
        .p_appid                    =   "",
        .p_channel                  =   DEFAULT_CHANNEL_NAME,
        .p_token                    =   "",
        .uid                        =   0,
				.peerid                     =   0,
				.conn_id                    =   0,
        .is_enable_send				      =	  0,
				.is_enbale_receive			    =	  1,
				.p_send_str					=	"hello agora",
    },
    .b_stop_flag            = 0,
    .b_join_success_flag    = 0,
};

app_t *app_get_instance(void)
{
	return &g_app_instance;
}

static void app_signal_handler(int32_t sig)
{
	app_t *p_app = app_get_instance();
	switch (sig) {
	case SIGQUIT:
	case SIGABRT:
	case SIGINT:
		p_app->b_stop_flag = 1;
		break;
	default:
		LOGW("no handler, sig=%d", sig);
	}
}

void app_print_usage(int32_t argc, char **argv)
{
	LOGS("\nUsage: %s [OPTION]", argv[0]);
	LOGS(" -h, --help                : show help info");
	LOGS(" -i, --app-id              : application id, either app-id OR token MUST be set");
	LOGS(" -t, --token               : token for authentication");
	LOGS(" -c, --channel-id          : channel, default is 'demo'");
	LOGS(" -u, --local-id            : local id, default is 0");
	LOGS(" -p, --peer-id             : peer id, default is 0");
	LOGS(" -s, --send                : enable custom message send function");
	LOGS(" -r, --receive             : enable custom message receive function");
	LOGS(" -l, --log-dir             : agora SDK log directory, default is '.'");

	LOGS("\nExample:");
	LOGS("    %s --app-id xxx [--token xxx] --channel-id xxx --send --receive",
		 argv[0]);
}

int32_t app_parse_args(app_config_t *p_config, int32_t argc, char **argv)
{
	const char *av_short_option = "hi:t:c:u:p:s:rl:";
	const struct option av_long_option[] = { { "help", 0, NULL, 'h' },
											 { "app-id", 1, NULL, 'i' },
											 { "token", 1, NULL, 't' },
											 { "channel-id", 1, NULL, 'c' },
											 { "local-id", 1, NULL, 'u' },
											 { "peer-id", 1, NULL, 'p' },
											 { "send", 1, NULL, 's' },
											 { "receive", 0, NULL, 'r' },
											 { "log-dir", 1, NULL, 'l' },
											 { 0, 0, 0, 0 } };

	int32_t ch = -1;
	int32_t optidx = 0;
	int32_t rval = 0;

	while (1) {
		ch = getopt_long(argc, argv, av_short_option, av_long_option, &optidx);
		if (ch == -1) {
			break;
		}

		switch (ch) {
		case 'h': {
			return -1;
		} break;
		case 'i': {
			p_config->p_appid = optarg;
		} break;
		case 't': {
			p_config->p_token = optarg;
		} break;
		case 'c': {
			p_config->p_channel = optarg;
		} break;
		case 'u': {
			p_config->uid = strtoul(optarg, NULL, 10);
		} break;
		case 'p': {
			p_config->peerid = strtoul(optarg, NULL, 10);
		} break;
		case 's': {
			p_config->is_enable_send = 1;
			p_config->p_send_str = optarg;
		} break;
		case 'r': {
			p_config->is_enbale_receive = 1;
		} break;
		case 'l': {
			p_config->p_sdk_log_dir = optarg;
		} break;
		default: {
			LOGS("%s parse cmd param: %s error.", TAG_APP, av_long_option[optidx].name);
			return -1;
		}
		}
	}

	// check key parameters
	if (strcmp(p_config->p_appid, "") == 0) {
		LOGE("%s appid MUST be provided", TAG_APP);
		return -1;
	}

	if (!p_config->p_channel || strcmp(p_config->p_channel, "") == 0) {
		LOGE("%s invalid channel", TAG_APP);
		return -1;
	}

	if (p_config->uid == 0 || p_config->peerid == 0) {
		LOGE("%s invalid local-id and peer-id", TAG_APP);
		return -1;
	}

	return 0;
}

static int32_t app_init(app_t *p_app)
{
	int32_t rval = 0;

	signal(SIGQUIT, app_signal_handler);
	signal(SIGABRT, app_signal_handler);
	signal(SIGINT, app_signal_handler);

	app_config_t *p_config = &p_app->config;

	return 0;
}

static void app_deinit(app_t *p_app)
{
	p_app->b_join_success_flag = 0;
	p_app->b_stop_flag = 0;
}

void app_sleep_ms(int64_t ms)
{
	usleep(ms * 1000);
}

static void __on_join_channel_success(connection_id_t conn_id, uint32_t uid, int32_t elapsed)
{
	app_t *p_app = app_get_instance();
	p_app->b_join_success_flag = 1;
	LOGI("%s join success, conn_id=%u uid=%u elapsed=%d", TAG_EVENT, conn_id, uid, elapsed);
}

static void __on_error(connection_id_t conn_id, int code, const char *msg)
{
	app_t *p_app = app_get_instance();
	if (code == ERR_INVALID_APP_ID) {
		p_app->b_stop_flag = 1;
		LOGE("%s invalid app-id, please double-check, code=%u msg=%s", TAG_EVENT, code, msg);
	} else if (code == ERR_INVALID_CHANNEL_NAME) {
		p_app->b_stop_flag = 1;
		LOGE("%s invalid channel, please double-check, conn_id=%u code=%u msg=%s", TAG_EVENT, conn_id,
			 code, msg);
	} else if (code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED) {
		p_app->b_stop_flag = 1;
		LOGE("%s invalid token, please double-check, code=%u msg=%s", TAG_EVENT, code, msg);
	} else if (code == ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT) {
		LOGW("%s send video over bandwdith limit, code=%u msg=%s", TAG_EVENT, code, msg);
	} else {
		LOGW("%s conn_id=%u code=%u msg=%s", TAG_EVENT, conn_id, code, msg);
	}
}

static void __on_user_joined(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
	LOGD("%s on_user_joined: conn_id=%u uid=%u elapsed_ms=%d", TAG_EVENT, conn_id, uid, elapsed_ms);
}

static void __on_user_offline(connection_id_t conn_id, uint32_t uid, int reason)
{
	LOGD("%s on_user_offline: conn_id=%u uid=%u reason=%d", TAG_EVENT, conn_id, uid, reason);
}

static void __on_connection_lost(connection_id_t conn_id)
{
	LOGD("%s on_connection_lost: conn_id=%u", TAG_EVENT, conn_id);
}

static void __on_rejoin_channel_success(connection_id_t conn_id, uint32_t uid, int elapsed_ms)
{
	LOGD("%s on_rejoin_success: conn_id=%u elapsed_ms=%d", TAG_EVENT, conn_id, elapsed_ms);
}

static void __on_media_ctrl_msg(connection_id_t conn_id, uint32_t uid,
										const void *payload, size_t length)
{
	LOGD("%s on_media_ctrl_msg: conn_id=%u uid=%u length=%zu payload=%s ", TAG_EVENT, conn_id, uid, length, (char*)payload);
}

static agora_rtc_event_handler_t event_handler = {
	.on_join_channel_success = __on_join_channel_success,
	.on_connection_lost = __on_connection_lost,
	.on_rejoin_channel_success = __on_rejoin_channel_success,
	.on_user_joined = __on_user_joined,
	.on_user_offline = __on_user_offline,
	.on_media_ctrl_msg = __on_media_ctrl_msg,
	.on_error = __on_error,
};

int32_t main(int32_t argc, char **argv)
{
	app_t *p_app = app_get_instance();
	app_config_t *p_config = &p_app->config;

	// 0. app parse args
	int32_t rval = app_parse_args(p_config, argc, argv);
	if (rval != 0) {
		app_print_usage(argc, argv);
		goto EXIT;
	}

	LOGS("%s Welcome to RTSA SDK v%s", TAG_APP, agora_rtc_get_version());

	// 1. app init
	rval = app_init(p_app);
	if (rval < 0) {
		LOGE("%s init failed, rval=%d", TAG_APP, rval);
		goto EXIT;
	}

	// 3. API: init agora rtc sdk
	int32_t appid_len = strlen(p_config->p_appid);
	void *p_appid = (void *)(appid_len == 0 ? NULL : p_config->p_appid);
	if (!p_config->is_enbale_receive) {
		event_handler.on_media_ctrl_msg = NULL;
	}
	rtc_service_option_t serv_opt = {0};
	serv_opt.log_cfg.log_path = p_config->p_sdk_log_dir;
	rval = agora_rtc_init(p_appid, &event_handler, &serv_opt);
	if (rval < 0) {
		LOGE("%s agora sdk init failed, rval=%d error=%s", TAG_API, rval,
			 agora_rtc_err_2_str(rval));
		goto EXIT;
	}

	agora_rtc_set_log_level(RTC_LOG_INFO);

	rval = agora_rtc_create_connection(&p_config->conn_id);
	if (rval != 0) {
		LOGE("%s agora_rtc_create_connection failed, rval=%d error=%s", TAG_API, rval,
			 agora_rtc_err_2_str(rval));
		goto EXIT;
	}

	// 4. API: join channel
	rtc_channel_options_t channel_options = { 0 };
	channel_options.auto_subscribe_audio = false;
	channel_options.auto_subscribe_video = false;
	rval = agora_rtc_join_channel(p_config->conn_id, p_config->p_channel, p_config->uid, p_config->p_token, &channel_options);
	if (rval < 0) {
		LOGE("%s join channel %s failed, rval=%d error=%s", TAG_API, p_config->p_channel, rval,
			 agora_rtc_err_2_str(rval));
		goto EXIT;
	}

	// 5. wait until join channel success or Ctrl-C trigger stop
	while (1) {
		if (p_app->b_stop_flag || p_app->b_join_success_flag) {
			break;
		}
		app_sleep_ms(10);
	}

	// 6. custom message transmit loop with pace sender
	while (!p_app->b_stop_flag) {
		if (p_config->is_enable_send) {
			// broadcast
			agora_rtc_send_media_ctrl_msg(p_config->conn_id, 0, p_config->p_send_str, strlen(p_config->p_send_str));
			// unicast
			agora_rtc_send_media_ctrl_msg(p_config->conn_id, p_config->peerid, p_config->p_send_str, strlen(p_config->p_send_str));
		}
		app_sleep_ms(1000);
	}

	// 7. API: leave channel
	agora_rtc_leave_channel(p_config->conn_id);

	// 8. API: fini rtc sdk
	agora_rtc_fini();

EXIT:
	// 9. app deinit
	app_deinit(p_app);
	return rval;
}

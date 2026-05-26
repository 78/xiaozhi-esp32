
#include <stdlib.h>
#include <sys/time.h>
#include "utility.h"
#include "pacer.h"

typedef struct {
	uint32_t audio_send_interval_us;
	uint32_t video_send_interval_us;
	int64_t audio_predict_time_us;
	int64_t video_predict_time_us;
} pacer_t;

void *pacer_create(uint32_t audio_send_interval_us, uint32_t video_send_interval_us)
{
	pacer_t *pacer = (pacer_t *)malloc(sizeof(pacer_t));

	pacer->audio_send_interval_us = audio_send_interval_us;
	pacer->video_send_interval_us = video_send_interval_us;
	pacer->audio_predict_time_us = 0;
	pacer->video_predict_time_us = 0;

	return pacer;
}

void pacer_destroy(void *pacer)
{
	if (pacer) {
		free(pacer);
	}
}

bool is_time_to_send_audio(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t cur_time_us = util_get_time_us();

	if (pc->audio_predict_time_us == 0) {
		pc->audio_predict_time_us = cur_time_us;
	}

	if (cur_time_us >= pc->audio_predict_time_us) {
		pc->audio_predict_time_us += pc->audio_send_interval_us;
		return true;
	}

	return false;
}

bool is_time_to_send_video(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t cur_time_us = util_get_time_us();

	if (pc->video_predict_time_us == 0) {
		pc->video_predict_time_us = cur_time_us;
	}

	if (cur_time_us >= pc->video_predict_time_us) {
		pc->video_predict_time_us += pc->video_send_interval_us;
		return true;
	}

	return false;
}

void wait_before_next_send(void *pacer)
{
	pacer_t *pc = pacer;
	int64_t sleep_us = 0, sleep_us_1 = 0, sleep_us_2 = 0;
	int64_t cur_time_us = util_get_time_us();

	// only audio
	if (pc->audio_send_interval_us != 0 && pc->video_send_interval_us == 0) {
		sleep_us = pc->audio_predict_time_us - cur_time_us;
		goto __tag_out;
	}

	// only video
	if (pc->audio_send_interval_us == 0 && pc->video_send_interval_us != 0) {
		sleep_us = pc->video_predict_time_us - cur_time_us;
		goto __tag_out;
	}

	sleep_us_1 = pc->audio_predict_time_us - cur_time_us;
	sleep_us_2 = pc->video_predict_time_us - cur_time_us;
	sleep_us = sleep_us_1 < sleep_us_2 ? sleep_us_1 : sleep_us_2;

__tag_out:
	if (sleep_us < 0) {
		sleep_us = 0;
	}

	util_sleep_us(sleep_us);
}
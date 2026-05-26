#include <stdint.h>
#include <stdbool.h>

void *pacer_create(uint32_t audio_send_interval_us, uint32_t video_send_interval_us);
void pacer_destroy(void *pacer);
bool is_time_to_send_audio(void *pacer);
bool is_time_to_send_video(void *pacer);
void wait_before_next_send(void *pacer);
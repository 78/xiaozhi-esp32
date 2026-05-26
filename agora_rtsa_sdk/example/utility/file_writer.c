#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "agora_rtc_api.h"
#include "log.h"
#include "file_writer.h"

typedef struct {
	char base_name[128];
	uint8_t file_type; // audio / video
	FILE *file;
} file_writer_t;

void *create_file_writer(uint8_t file_type, const char *base_name)
{
	file_writer_t *file_writer = (file_writer_t *)malloc(sizeof(file_writer_t));
	if (!file_writer) {
		return NULL;
	}

	memset(file_writer, 0, sizeof(file_writer_t));
	strncpy(file_writer->base_name, base_name, sizeof(file_writer->base_name));
	file_writer->file_type = file_type;
	return file_writer;
}

int write_file(void *file_writer, uint8_t data_type, const void *data, size_t size)
{
	if (!file_writer) {
		return -1;
	}
	file_writer_t *fw = (file_writer_t *)file_writer;

	// if file is not created yet, creat it
	if (!fw->file) {
		char file_name[128];
		char file_suffix[8];
		strncpy(file_name, fw->base_name, sizeof(file_name));

		if (fw->file_type == FILE_TYPE_VIDEO) { // video
			switch (data_type) {
			case VIDEO_DATA_TYPE_H264:
				strncpy(file_suffix, ".h264", sizeof(file_suffix));
				break;
			case VIDEO_DATA_TYPE_H265:
				strncpy(file_suffix, ".h265", sizeof(file_suffix));
				break;
			case VIDEO_DATA_TYPE_GENERIC_JPEG:
				strncpy(file_suffix, ".mjpg", sizeof(file_suffix));
				break;
			default:
				strncpy(file_suffix, ".bin", sizeof(file_suffix));
				break;
			}
		} else if (fw->file_type == FILE_TYPE_AUDIO) { // audio
			switch (data_type) {
			case AUDIO_DATA_TYPE_OPUS:
				strncpy(file_suffix, ".opus", sizeof(file_suffix));
				break;
			case AUDIO_DATA_TYPE_AACLC:
			case AUDIO_DATA_TYPE_HEAAC:
				strncpy(file_suffix, ".aac", sizeof(file_suffix));
				break;
			case AUDIO_DATA_TYPE_PCMA:
			case AUDIO_DATA_TYPE_PCMU:
				strncpy(file_suffix, ".g711", sizeof(file_suffix));
				break;
			case AUDIO_DATA_TYPE_G722:
				strncpy(file_suffix, ".g722", sizeof(file_suffix));
				break;
			case AUDIO_DATA_TYPE_PCM:
				strncpy(file_suffix, ".pcm", sizeof(file_suffix));
				break;
			default:
				strncpy(file_suffix, ".bin", sizeof(file_suffix));
				break;
			}
		} else {
			LOGE("invalid file type: %u", fw->file_type);
			return -1;
		}

		strcat(file_name, file_suffix);
		if ((fw->file = fopen(file_name, "w")) == NULL) {
			LOGE("Failed to create file \"%s\"", file_name);
			return -1;
		}
		LOGI("Create file \"%s\" successfully", file_name);
	}

	return fwrite(data, 1, size, fw->file);
}

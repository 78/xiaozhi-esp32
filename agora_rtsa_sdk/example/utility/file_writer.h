#ifndef __AGORA_FILE_WRITER_H__
#define __AGORA_FILE_WRITER_H__

enum {
  FILE_TYPE_AUDIO = 1,
  FILE_TYPE_VIDEO = 2,
};

void *create_file_writer(uint8_t file_type, const char *base_name);
int write_file(void *file_writer, uint8_t data_type, const void *data, size_t size);


#endif
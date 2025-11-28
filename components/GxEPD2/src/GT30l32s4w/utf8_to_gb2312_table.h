// UTF-8 -> GB2312 Mapping Table (Auto-generated)
#pragma once

typedef struct {
    unsigned char utf8[3]; // UTF-8 字节 (最多3字节)
    unsigned char gb[2];   // GB2312 两字节
} utf8_gb2312_map_t;

extern const utf8_gb2312_map_t utf8_gb2312_table[7445];

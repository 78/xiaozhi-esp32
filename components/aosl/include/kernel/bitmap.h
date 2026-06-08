/***************************************************************************
 * Module:	bitmap header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#ifndef ___AOSL_BITMAP_H
#define ___AOSL_BITMAP_H

#include <api/aosl_types.h>

typedef struct bitmap_s {
	uint8_t *bit_arr;   // bit curr value
	uint8_t bit_arr_cnt; // bit array cnt
	uint8_t bit_cnt;     // bit cnt
} bitmap_t;

bitmap_t* bitmap_create(uint8_t bit_cnt);
void bitmap_destroy(bitmap_t *self);
void bitmap_set(bitmap_t *self, uint8_t i);
void bitmap_clear(bitmap_t *self, uint8_t i);
void bitmap_reset(bitmap_t *self);
bool bitmap_get(bitmap_t *self, uint8_t i);
void bitmap_copy(bitmap_t *self, bitmap_t *src);
int bitmap_find_first_zero_bit(bitmap_t *self);

#endif /* ___AOSL_BITMAP_H */

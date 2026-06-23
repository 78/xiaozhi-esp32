/***************************************************************************
 * Module:	AOSL simple bitmap implementations.
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/
#include <string.h>
#include <api/aosl_mm.h>
#include <kernel/bitmap.h>
#include <kernel/bug.h>

#define BIT_ARRAY_CNT(bit_cnt) ((bit_cnt + 8 - 1) / 8)

bitmap_t* bitmap_create(uint8_t bit_cnt)
{
	if (bit_cnt == 0) {
		return NULL;
	}

	int arr_cnt = BIT_ARRAY_CNT(bit_cnt);
	bitmap_t *self = (bitmap_t *)aosl_malloc_impl(sizeof(bitmap_t));
	if (!self) {
		return NULL;
	}
	memset(self, 0, sizeof(bitmap_t));

	self->bit_arr = (uint8_t *)aosl_malloc_impl(arr_cnt);
	if (!self->bit_arr) {
		goto __tag_failed;
	}
	memset(self->bit_arr, 0, arr_cnt);
	self->bit_arr_cnt = arr_cnt;
	self->bit_cnt = bit_cnt;
	return self;

__tag_failed:
	bitmap_destroy(self);
	return NULL;
}

void bitmap_destroy(bitmap_t *self)
{
	if (!self) {
		return;
	}

	if (self->bit_arr) {
		aosl_free(self->bit_arr);
		self->bit_arr = NULL;
	}
	aosl_free(self);
}

void bitmap_set(bitmap_t *self, uint8_t i)
{
	if(!self || i >= self->bit_cnt) {
		return;
	}

	uint8_t index = i / 8;
	uint8_t offset = i % 8;

	self->bit_arr[index] |= ((uint8_t)1 << offset);
}

void bitmap_clear(bitmap_t *self, uint8_t i)
{
	if(!self || i >= self->bit_cnt) {
		return;
	}

	uint8_t index = i / 8;
	uint8_t offset = i % 8;

	self->bit_arr[index] &= ~((uint8_t)1 << offset);
}

void bitmap_reset(bitmap_t *self)
{
	BUG_ON(!self);
	if (!self->bit_arr || !self->bit_arr_cnt) {
		return;
	}
	memset(self->bit_arr, 0, self->bit_arr_cnt);
}

bool bitmap_get(bitmap_t *self, uint8_t i)
{
	BUG_ON(!self);
	BUG_ON(i > self->bit_cnt);

	uint8_t index = i / 8;
	uint8_t offset = i % 8;

	return (self->bit_arr[index] & ((uint8_t)1 << offset)) != 0;
}

void bitmap_copy(bitmap_t *self, bitmap_t *src)
{
	BUG_ON(!self || !src);
	BUG_ON(self->bit_cnt < src->bit_cnt);

	bitmap_reset(self);

	memcpy(self->bit_arr, src->bit_arr, src->bit_arr_cnt);
}

int bitmap_find_first_zero_bit(bitmap_t *self)
{
	BUG_ON(!self);

	unsigned int byte_idx;
	int bitpos = 0;
	
	for (byte_idx = 0; byte_idx < self->bit_arr_cnt; byte_idx++) {
		uint8_t byte_val = self->bit_arr[byte_idx];
		
		/* Skip if all bits are set */
		if (byte_val == 0xFF) {
			bitpos += 8;
			continue;
		}
		
		/* Find first zero bit in this byte using bit manipulation */
		unsigned int bit_idx;
		for (bit_idx = 0; bit_idx < 8; bit_idx++) {
			if (bitpos >= self->bit_cnt) {
				return -1;
			}
			
			if (!(byte_val & ((uint8_t)1 << bit_idx))) {
				return bitpos;
			}
			bitpos++;
		}
	}

	return -1;
}

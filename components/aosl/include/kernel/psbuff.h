/***************************************************************************
 * Module:		packet piece buffer header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __PSBUFF_H__
#define __PSBUFF_H__


#include <kernel/err.h>


#define PSB_USER_BUFFER (0x00800000u)

/* NPTS packet piece buffer */
struct ps_buff {
	/* MUST KEEP THESE 3 FIELDS IDENTICAL WITH aosl_psb_t */
	void *data;
	size_t len;
	struct ps_buff *next;

	void *head;
	size_t size;
	unsigned int flags;
};


extern struct ps_buff *alloc_user_psb (void *buf, size_t bufsz);
extern struct ps_buff *alloc_psb (size_t size);
extern void psb_attach_buf (struct ps_buff *psb, void *buf, size_t bufsz);
extern void psb_detach_buf (struct ps_buff *psb);
extern void free_psb (struct ps_buff *psb);


/**
 *	psb_headroom - bytes at buffer head
 *	@psb: buffer to check
 *
 *	Return the number of bytes of free space at the head of an ps_buff.
 */
static inline unsigned int psb_headroom (const struct ps_buff *psb)
{
	return (const uint8_t *)psb->data - (const uint8_t *)psb->head;
}

/**
 *	psb_tailroom - bytes at buffer end
 *	@psb: buffer to check
 *
 *	Return the number of bytes of free space at the tail of an ps_buff
 */
static inline unsigned int psb_tailroom (const struct ps_buff *psb)
{
	return psb->size - psb->len - ((const uint8_t *)psb->data - (const uint8_t *)psb->head);
}

/**
 *	psb_reserve - adjust headroom
 *	@psb: buffer to alter
 *	@len: bytes to move
 *
 *	Increase the headroom of an empty ps_buff by reducing the tail
 *	room. This is only allowed for an empty buffer.
 */
static inline int psb_reserve(struct ps_buff *psb, unsigned int len)
{
	if (psb->len > 0)
		return -AOSL_EPERM;

	if (psb->size - ((const uint8_t *)psb->data - (const uint8_t *)psb->head) >= len) {
		psb->data = (uint8_t *)psb->data + len;
		return 0;
	}

	return -AOSL_ENOSPC;
}

/**
 *	psb_put - add data to a buffer
 *	@psb: buffer to use
 *	@len: amount of data to add
 **/
static inline void *psb_put (struct ps_buff *psb, unsigned int len)
{
	if (likely ((uint8_t *)psb->data + psb->len + len <= (uint8_t *)psb->head + psb->size)) {
		void *tmp = (uint8_t *)psb->data + psb->len;
		psb->len += len;
		return tmp;
	}

	return ERR_PTR (-AOSL_ENOSPC);
}

/**
 *	psb_get - remove data from a buffer
 *	@psb: buffer to use
 *	@len: amount of data to remove
 **/
static inline void *psb_get (struct ps_buff *psb, unsigned int len)
{
	if (likely (psb->len >= len)) {
		void *tmp = psb->data;
		psb->data = (uint8_t *)psb->data + len;
		psb->len -= len;
		return tmp;
	}

	return ERR_PTR (-AOSL_ENOSPC);
}

/**
 *	psb_peek - peek data from a buffer
 *	@psb: buffer to use
 *	@len: amount of data to peek
 **/
static inline void *psb_peek (const struct ps_buff *psb, unsigned int len)
{
	if (likely (psb->len >= len))
		return psb->data;

	return ERR_PTR (-AOSL_ENOSPC);
}

/**
 *	psb_push - add data to the start of a buffer
 *	@psb: buffer to use
 *	@len: amount of data to add
 **/
static inline void *psb_push (struct ps_buff *psb, unsigned int len)
{
	if ((uint8_t *)psb->data - (uint8_t *)psb->head < (int)len)
		return ERR_PTR (-AOSL_ENOSPC);

	psb->len += len;
	psb->data = (uint8_t *)psb->data - len;
	return psb->data;
}

/**
 *	psb_pull - remove data from the start of a buffer
 *	@psb: buffer to use
 *	@len: amount of data to remove
 *
 *	This function removes data from the start of a buffer, returning
 *	the memory to the headroom. A pointer to the next data in the buffer
 *	is returned. Once the data has been pulled future pushes will overwrite
 *	the old data.
 */
static inline void *psb_pull (struct ps_buff *psb, unsigned int len)
{
	if (psb->len < len)
		return ERR_PTR (-AOSL_ENOSPC);

	psb->len -= len;
	psb->data = (uint8_t *)psb->data + len;
	return psb->data;
}

static inline void *psb_data (const struct ps_buff *psb)
{
	return psb->data;
}

static inline void *psb_tail (const struct ps_buff *psb)
{
	return (uint8_t *)psb->data + psb->len;
}

static inline unsigned int psb_len (const struct ps_buff *psb)
{
	return psb->len;
}




#endif /* __PSBUFF_H__ */

/***************************************************************************
 * Module		:		packet piece buffer implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <kernel/err.h>
#include <kernel/kernel.h>
#include <api/aosl_mm.h>
#include <kernel/psbuff.h>
#include <api/aosl_psb.h>

struct ps_buff *alloc_user_psb (void *buf, size_t bufsz)
{
	struct ps_buff *psb = aosl_malloc (sizeof (struct ps_buff));
	if (psb != NULL) {
		if (bufsz > 0) {
			psb->head = buf;
		} else {
			psb->head = NULL;
		}

		psb->size = bufsz;
		psb->flags = PSB_USER_BUFFER;

		psb->data = psb->head;
		psb->len = 0;
		psb->next = NULL;
		return psb;
	}

	return ERR_PTR (-AOSL_ENOMEM);
}


#define MAX_SPB_SIZE (8 << 20)

struct ps_buff *alloc_psb (size_t size)
{
	struct ps_buff *psb;

	if (size > MAX_SPB_SIZE)
		return ERR_PTR (-AOSL_E2BIG);

	psb = aosl_malloc (sizeof (struct ps_buff));
	if (psb != NULL) {
		if (size > 0) {
			psb->head = aosl_malloc (size);
			if (!psb->head) {
				aosl_free (psb);
				return ERR_PTR (-AOSL_ENOMEM);
			}
		} else {
			psb->head = NULL;
		}

		psb->size = size;
		psb->flags = 0;

		psb->data = psb->head;
		psb->len = 0;
		psb->next = NULL;
		return psb;
	}

	return ERR_PTR (-AOSL_ENOMEM);
}

void free_psb (struct ps_buff *psb)
{
	if (!(psb->flags & PSB_USER_BUFFER) && psb->head != NULL)
		aosl_free (psb->head);

	aosl_free (psb);
}

void psb_attach_buf (struct ps_buff *psb, void *buf, size_t bufsz)
{
	if (psb->size > 0 && !(psb->flags & PSB_USER_BUFFER))
		aosl_free (psb->head);

	if (bufsz > 0) {
		psb->head = buf;
		psb->data = buf;
	} else {
		psb->head = NULL;
		psb->data = NULL;
	}

	psb->size = bufsz;
	psb->len = 0;
}

static __inline__ void __psb_detach_buf (struct ps_buff *psb)
{
	if (!(psb->flags & PSB_USER_BUFFER))
		aosl_free (psb->head);

	psb->head = NULL;
	psb->size = 0;
	psb->data = NULL;
	psb->len = 0;
}

void psb_detach_buf (struct ps_buff *psb)
{
	if (psb->size > 0)
		__psb_detach_buf (psb);
}

__export_in_so__ aosl_psb_t *aosl_alloc_user_psb (void *buf, size_t bufsz)
{
	return_ptr_err (alloc_user_psb (buf, bufsz));
}

__export_in_so__ aosl_psb_t *aosl_alloc_psb (size_t size)
{
	return_ptr_err (alloc_psb (size));
}

__export_in_so__ void aosl_psb_attach_buf (aosl_psb_t *psb, void *buf, size_t bufsz)
{
	psb_attach_buf ((struct ps_buff *)psb, buf, bufsz);
}

__export_in_so__ void aosl_psb_detach_buf (aosl_psb_t *psb)
{
	psb_detach_buf ((struct ps_buff *)psb);
}

__export_in_so__ size_t aosl_psb_headroom (const aosl_psb_t *psb)
{
	return psb_headroom ((struct ps_buff *)psb);
}

__export_in_so__ size_t aosl_psb_tailroom (const aosl_psb_t *psb)
{
	return psb_tailroom ((struct ps_buff *)psb);
}

__export_in_so__ void *aosl_psb_data (const aosl_psb_t *psb)
{
	return psb->data;
}

__export_in_so__ size_t aosl_psb_len (const aosl_psb_t *psb)
{
	return psb->len;
}

__export_in_so__ size_t aosl_psb_total_len (const aosl_psb_t *psb)
{
	size_t total = 0;

	while (psb) {
		total += psb->len;
		psb = psb->next;
	}

	return total;
}

__export_in_so__ int aosl_psb_reserve (aosl_psb_t *psb, size_t len)
{
	return_err (psb_reserve ((struct ps_buff *)psb, len));
}

__export_in_so__ void *aosl_psb_put (aosl_psb_t *psb, size_t len)
{
	return_ptr_err (psb_put ((struct ps_buff *)psb, len));
}

__export_in_so__ void *aosl_psb_get (aosl_psb_t *psb, size_t len)
{
	return_ptr_err (psb_get ((struct ps_buff *)psb, len));
}

__export_in_so__ void *aosl_psb_peek (const aosl_psb_t *psb, size_t len)
{
	return_ptr_err (psb_peek ((const struct ps_buff *)psb, len));
}

__export_in_so__ void *aosl_psb_push (aosl_psb_t *psb, size_t len)
{
	return_ptr_err (psb_push ((struct ps_buff *)psb, len));
}

__export_in_so__ void *aosl_psb_pull (aosl_psb_t *psb, size_t len)
{
	return_ptr_err (psb_pull ((struct ps_buff *)psb, len));
}

__export_in_so__ int aosl_psb_single (const aosl_psb_t *psb)
{
	return (int)(psb->next == NULL);
}

__export_in_so__ void aosl_psb_reset (aosl_psb_t *psb)
{
	struct ps_buff *psbuff = (struct ps_buff *)psb;
	while (psbuff != NULL) {
		psbuff->data = psbuff->head;
		psbuff->len = 0;
		psbuff = psbuff->next;
	}
}

__export_in_so__ void aosl_free_psb_list (aosl_psb_t *aosl_psb)
{
	struct ps_buff *psb = (struct ps_buff *)aosl_psb;
	while (psb) {
		struct ps_buff *next = psb->next;
		free_psb (psb);
		psb = next;
	}
}
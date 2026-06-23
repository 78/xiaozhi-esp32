/***************************************************************************
 * Module:	AOSL regular file operations implementation file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#include <string.h>

#include <api/aosl_types.h>
#include <api/aosl_mm.h>
#include <api/aosl_log.h>

#include <kernel/err.h>
#include <kernel/fileobj.h>
#include <kernel/thread.h>

static k_rwlock_t fds_lock;
static uint32_t __fobj_life_id = 0;
static struct aosl_rb_root attached_fds;

static int cmp_fd (struct aosl_rb_node *rb_node, struct aosl_rb_node *node, va_list args)
{
	struct file_obj *rb_entry = aosl_rb_entry (rb_node, struct file_obj, rb_node);
	aosl_fd_t fd;

	if (node != NULL) {
		fd = aosl_rb_entry (node, struct file_obj, rb_node)->fd;
	} else {
		fd = va_arg (args, aosl_fd_t);
	}

	if ((uintptr_t)rb_entry->fd > (uintptr_t)fd)
		return 1;

	if ((uintptr_t)rb_entry->fd < (uintptr_t)fd)
		return -1;

	return 0;
}

int install_fd (aosl_fd_t fd, struct file_obj *f)
{
	int err;
	struct aosl_rb_node *node;

	if (aosl_fd_invalid (fd))
		return -AOSL_EBADF;

	k_rwlock_wrlock (&fds_lock);
	node = aosl_find_rb_node (&attached_fds, NULL, fd);
	if (node != NULL) {
		err = -AOSL_EBUSY;
		goto ____out;
	}

	f->life_id = __fobj_life_id++;
	aosl_rb_insert_node (&attached_fds, &f->rb_node);
	err = 0;

____out:
	k_rwlock_wrunlock (&fds_lock);
	return err;
}


int remove_fd (struct file_obj *f)
{
	aosl_fd_t fd = f->fd;
	struct aosl_rb_node *node;

	k_rwlock_wrlock (&fds_lock);
	node = aosl_rb_remove (&attached_fds, NULL, fd);
	k_rwlock_wrunlock (&fds_lock);

	if (node != &f->rb_node)
		return -AOSL_ENONET;

	return 0;
}

struct file_obj *fget (aosl_fd_t fd)
{
	if (!aosl_fd_invalid (fd)) {
		struct aosl_rb_node *node;
		struct file_obj *f = NULL;

		k_rwlock_rdlock (&fds_lock);
		node = aosl_find_rb_node (&attached_fds, NULL, fd);
		if (node != NULL) {
			f = aosl_rb_entry (node, struct file_obj, rb_node);
			__fget (f);
		}
		k_rwlock_rdunlock (&fds_lock);

		return f;
	}

	return NULL;
}

void fput (struct file_obj *f)
{
	if (atomic_dec_and_test (&f->usage)) {
		if (f->dtor != NULL)
			f->dtor (f);

		aosl_free (f);
	}
}

static void attached_fds_check (void)
{
	if (NULL != aosl_rb_first (&attached_fds)) {
		AOSL_LOG_ERR("[dtor] attached_fds no free");
	}
}

void fileobj_init (void)
{
	k_rwlock_init (&fds_lock);
	aosl_rb_root_init (&attached_fds, cmp_fd);
}

void fileobj_fini (void)
{
	attached_fds_check ();
	k_rwlock_destroy (&fds_lock);
}
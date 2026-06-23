/***************************************************************************
 * Module:	file object
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __KERNEL_FILE_H__
#define __KERNEL_FILE_H__


#include <api/aosl_types.h>
#include <api/aosl_defs.h>
#include <api/aosl_rbtree.h>
#include <kernel/atomic.h>
#include <kernel/list.h>

typedef void (*k_obj_dtor_t) (void *obj);

struct file_obj {
	struct aosl_rb_node rb_node;
	aosl_fd_t fd;
	atomic_t usage;
	int mpq_fd;
	uint32_t life_id;
	k_obj_dtor_t dtor;
};


extern void fileobj_init (void);
extern void fileobj_fini (void);

extern int install_fd (aosl_fd_t fd, struct file_obj *f);
extern int remove_fd (struct file_obj *f);

static __inline__ void __fget (struct file_obj *f)
{
	atomic_inc (&f->usage);
}

extern struct file_obj *fget (aosl_fd_t fd);
extern void fput (struct file_obj *f);



#endif /* __KERNEL_FILE_H__ */

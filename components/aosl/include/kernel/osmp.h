/***************************************************************************
 * Module:		OS relative multiplex queue header
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __KERNEL_OSMP_H__
#define __KERNEL_OSMP_H__

#include <api/aosl_types.h>
#include <hal/aosl_hal_iomp.h>


struct mp_queue;
struct iofd;

extern int os_mp_init (struct mp_queue *q);
extern void os_mp_fini (struct mp_queue *q);

extern int os_add_event_fd (struct mp_queue *q, struct iofd *f);
extern int os_del_event_fd (struct mp_queue *q, struct iofd *f);

extern int os_poll_dispatch (struct mp_queue *q, intptr_t timeo);

#endif /* __KERNEL_OSMP_H__ */

/***************************************************************************
 * Module:	Internal used net relative functionals header file
 *
 * Copyright © 2025 Agora
 * This file is part of AOSL, an open source project.
 * Licensed under the Apache License, Version 2.0, with certain conditions.
 * Refer to the "LICENSE" file in the root directory for more information.
 ***************************************************************************/

#ifndef __AOSL_KERNEL_NET_H__
#define __AOSL_KERNEL_NET_H__

#include <api/aosl_socket.h>

const char *k_inet_ntop (int af, const void *src, char *dst, aosl_socklen_t size);
int k_inet_pton (int af, const char *src, void *dst);

#endif /* __AOSL_KERNEL_NET_H__ */

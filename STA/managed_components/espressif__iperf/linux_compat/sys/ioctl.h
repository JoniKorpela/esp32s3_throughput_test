/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*
 * lwIP's lwipopts.h includes <sys/ioctl.h>. Newer glibc headers define
 * SIOCATMARK there, and lwIP's sockets.h defines that macro again.
 * Include the real header, then drop the host macro.
 */
#include_next <sys/ioctl.h>

#ifdef SIOCATMARK
#undef SIOCATMARK
#endif

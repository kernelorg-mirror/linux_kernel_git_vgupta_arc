/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_POSIX_TYPES_H
#define _ASM_ARC_POSIX_TYPES_H

/* XXX: Subtle differences in asm-generic ver - need to chk before switch */
typedef unsigned long __kernel_ino_t;
typedef unsigned short __kernel_mode_t;
typedef unsigned short __kernel_nlink_t;
typedef long __kernel_off_t;
typedef int __kernel_pid_t;
typedef unsigned short __kernel_ipc_pid_t;
typedef unsigned short __kernel_uid_t;
typedef unsigned short __kernel_gid_t;
typedef unsigned int __kernel_size_t;
typedef int __kernel_ssize_t;
typedef int __kernel_ptrdiff_t;
typedef long __kernel_time_t;
typedef long __kernel_suseconds_t;
typedef long __kernel_clock_t;
typedef int __kernel_timer_t;
typedef int __kernel_clockid_t;
typedef int __kernel_daddr_t;
typedef char *__kernel_caddr_t;
typedef unsigned short __kernel_uid16_t;
typedef unsigned short __kernel_gid16_t;
typedef unsigned int __kernel_uid32_t;
typedef unsigned int __kernel_gid32_t;

typedef unsigned short __kernel_old_uid_t;
typedef unsigned short __kernel_old_gid_t;
typedef unsigned short __kernel_old_dev_t;

#ifdef __GNUC__
typedef long long __kernel_loff_t;
#endif

typedef struct {
#if defined(__KERNEL__) || defined(__USE_ALL)
	int val[2];
#else
	int __val[2];
#endif
} __kernel_fsid_t;

#if defined(__KERNEL__) || !defined(__GLIBC__) || (__GLIBC__ < 2)

#undef	__FD_SET
#define __FD_SET(fd, __p) \
		(((fd_set *)(__p))->fds_bits[fd >> 5] |= (1<<(fd & 31)))

#undef	__FD_CLR
#define __FD_CLR(fd, __p) \
		(((fd_set *)(__p))->fds_bits[(fd) >> 5] &= ~(1<<((fd) & 31)))

#undef	__FD_ISSET
#define __FD_ISSET(fd, __p) \
		((((fd_set *)(__p))->fds_bits[fd >> 5] & (1<<(fd & 31))) != 0)

#undef	__FD_ZERO
#define __FD_ZERO(__p) \
		(memset(__p, 0, sizeof(*(fd_set *)(__p))))

#endif

#endif /* _ASM_ARC_POSIX_TYPES_H */

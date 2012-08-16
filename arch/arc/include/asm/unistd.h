/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(_ASM_ARC_UNISTD_H) || defined(__SYSCALL)
#define _ASM_ARC_UNISTD_H

/*
 * Being uClibc based we need some of the deprecated syscalls:
 * -Not emulated by uClibc at all
 *	unlink, mkdir,... (needed by Busybox, LTP etc)
 *	times (needed by LTP pan test harness)
 * -Not emulated efficiently
 *	select: emulated using pselect (but extra code to chk usec > 1sec)
 *
 * some (send/recv) correctly emulated using (recfrom/sendto) and
 * some arch specific ones (fork/vfork)can easily be emulated using clone but
 * thats the price of using common-denominator....
 */
#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_NO_FLAGS
#define __ARCH_WANT_SYSCALL_OFF_T
#define __ARCH_WANT_SYSCALL_DEPRECATED

/* To handle @cmd|0x100 passed by uClibc for semctl(), shmctl() and msgctl() */
#define __ARCH_WANT_IPC_PARSE_VERSION

#include <asm-generic/unistd.h>

#define NR_syscalls	__NR_syscalls

/* ARC specific syscall */
#define __NR_cacheflush        (__NR_arch_specific_syscall + 0)
__SYSCALL(__NR_cacheflush, sys_cacheflush)

/* Generic syscall (fs/filesystems.c - lost in asm-generic/unistd.h */
#define __NR_sysfs		(__NR_arch_specific_syscall + 1)
__SYSCALL(__NR_sysfs, sys_sysfs)

#endif /* _ASM_ARC_UNISTD_H */

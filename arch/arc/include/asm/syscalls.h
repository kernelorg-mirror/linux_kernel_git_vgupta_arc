/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_SYSCALLS_H
#define _ASM_ARC_SYSCALLS_H  1

#ifdef __KERNEL__

#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/types.h>

int sys_execve_wrapper(int, int, int);
int sys_clone_wrapper(int, int, int, int, int);
int sys_fork_wrapper(void);
int sys_vfork_wrapper(void);
int sys_cacheflush(uint32_t start, uint32_t end, uint32_t flags);

#include <asm-generic/syscalls.h>

#endif	/* __KERNEL__ */

#endif

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASMARC_BUG_H
#define _ASMARC_BUG_H

#include <asm/event-log.h>
#include <asm/ptrace.h>

struct task_struct;
void dump_stack(void);
void show_stacktrace(struct task_struct *tsk, struct pt_regs *regs);

#define BUG() do {						\
	dump_stack();					\
	pr_warn("Kernel BUG in %s: %s: %d!\n",\
		__FILE__, __func__,  __LINE__);		\
	sort_snaps(1);						\
} while (0)

#define HAVE_ARCH_BUG

#include <asm-generic/bug.h>

#endif

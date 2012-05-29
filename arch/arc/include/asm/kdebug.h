/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_KDEBUG_H
#define _ASM_ARC_KDEBUG_H

#include <asm/ptrace.h>

enum die_val {
	DIE_UNUSED,
	DIE_TRAP,
	DIE_IERR,
	DIE_OOPS
};

extern void die(const char *str, struct pt_regs *regs, unsigned long address,
		unsigned long cause_reg);

void show_kernel_fault_diag(const char *str, struct pt_regs *regs,
			    unsigned long address, unsigned long cause_reg);
#endif

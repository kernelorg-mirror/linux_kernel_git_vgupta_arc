/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_SMP_H
#define __ASM_ARC_SMP_H

#include <linux/threads.h>

#define raw_smp_processor_id() (current_thread_info()->cpu)

typedef struct {
	void *stack;
	void *c_entry;
	int cpu_id;
} secondary_boot_t;

extern void smp_ipi_init(void);
void __init smp_init_cpus(void)
extern void wakeup_secondary(void);
extern void first_lines_of_secondary(void);

#endif

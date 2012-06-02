/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_SMP_H
#define __ASM_ARC_SMP_H

#ifdef CONFIG_SMP

#include <linux/threads.h>
#include <plat/smp.h>

#define raw_smp_processor_id() (current_thread_info()->cpu)

/*
 * APIs provided by arch SMP code to rest of arch code
 */

typedef struct {
	void *stack;
	void *c_entry;
	int cpu_id;
} secondary_boot_t;

extern void smp_ipi_init(void);
extern void __init smp_init_cpus(void)
extern void wakeup_secondary(void);
extern void first_lines_of_secondary(void);

/*
 * API expected BY platform smp code (FROM arch smp code)
 *
 * smp_ipi_irq_setup:
 *	Takes @cpu and @irq to which the arch-common ISR is hooked up
 */
extern int smp_ipi_irq_setup(int cpu, int irq);

/*
 * APIs expected FROM platform smp code
 *
 * arc_platform_smp_cpuinfo:
 *	returns a string containing info for /proc/cpuinfo
 *
 * arc_platform_smp_init_cpu:
 *	Called from start_kernel_secondary to do any CPU local setup
 *	such as starting a timer, setting up IPI etc
 *
 * arc_platform_ipi_send:
 *	Takes @cpumask to which IPI(s) would be sent.
 *	The actual msg-id/buffer is manager in arch-common code
 *
 * arc_platform_ipi_clear:
 *	Takes @cpu which got IPI at @irq to do any IPI clearing
 */
extern const char *arc_platform_smp_cpuinfo(void);
extern void arc_platform_smp_init_cpu(void);
extern void arc_platform_ipi_send(cpumask_t callmap);
extern void arc_platform_ipi_clear(int cpu, int irq);

#else  /* !CONFIG_SMP */

#define arc_platform_smp_cpuinfo()	((const char *)"")
#define arc_platform_smp_init_cpu()
#define arc_platform_ipi_send(callmap)
#define arc_platform_ipi_clear(cpu, irq)

#endif

#endif

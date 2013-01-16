/*
 * Copyright (C) 2012 Synopsys, Inc. (www.synopsys.com)
 *
 * based on METAG mach/arch.h (which in turn was based on ARM)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_MACH_DESC_H_
#define _ASM_ARC_MACH_DESC_H_

/**
 * struct machine_desc - Describes a board controlled by an ARC CPU/SoC
 * @name:		Board/SoC name
 * @dt_compat:		Array of device tree 'compatible' strings
 *
 * @init_early:		Very early callback (called from setup_arch())
 * @init_irq:		IRQ init callback for setting up IRQ controllers
 * @init_machine:	arch initcall level callback (e.g. parse DT devices)
 * @init_late:		Late init callback
 * @init_smp:		Called for each CPU (e.g. setup IPI)
 *
 * This structure is provided by each board which can be controlled by an ARC.
 * It is chosen by matching the compatible strings in the device tree provided
 * by the bootloader with the strings in @dt_compat, and sets up any aspects of
 * the machine that aren't configured with device tree (yet).
 */
struct machine_desc {
	const char		*name;
	const char		**dt_compat;

	void			(*init_early)(void);
	void			(*init_irq)(void);
	void			(*init_machine)(void);
	void			(*init_late)(void);

#ifdef CONFIG_SMP
	void			(*init_early_smp)(void);
	void			(*init_smp)(unsigned int);
#endif
};

/*
 * Current machine - only accessible during boot.
 */
extern struct machine_desc *machine_desc;

/*
 * Machine type table - also only accessible during boot
 */
extern struct machine_desc __arch_info_begin[], __arch_info_end[];
#define for_each_machine_desc(p)			\
	for (p = __arch_info_begin; p < __arch_info_end; p++)

static inline struct machine_desc *default_machine_desc(void)
{
	/* the default machine is the last one linked in */
	if (__arch_info_end - 1 < __arch_info_begin)
		return NULL;
	return __arch_info_end - 1;
}

/*
 * Set of macros to define architecture features.
 * This is built into a table by the linker.
 */
#define MACHINE_START(_type, _name)			\
static const struct machine_desc __mach_desc_##_type	\
__used							\
__attribute__((__section__(".arch.info.init"))) = {	\
	.name		= _name,

#define MACHINE_END				\
};

extern struct machine_desc *setup_machine_fdt(void *dt);
#endif

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/console.h>
#include <asm/serial.h>
#include <plat/irq.h>
#include <plat/memmap.h>

void __init arc_platform_early_init(void)
{
	pr_info("[plat-arcfpga]: registering early dev resources\n");

#ifdef CONFIG_ARC_SERIAL
	/*
	 * This is to make sure that arc uart would be preferred console
	 * despite one/more of following:
	 *   -command line lacked "console=ttyS0" or
	 *   -CONFIG_VT_CONSOLE was enabled (for no reason whatsoever)
	 */
	add_preferred_console("ttyS", 0, "115200");

#ifdef CONFIG_EARLY_PRINTK
	/* TBD: early_platform_add_devices */
	arc_early_serial_reg();
#endif
#endif
}

int __init fpga_plat_init(void)
{
	pr_info("[plat-arcfpga]: registering device resources\n");

	/* TBD: Use platform_add_devices to add uart resources */

	return 0;
}
arch_initcall(fpga_plat_init);

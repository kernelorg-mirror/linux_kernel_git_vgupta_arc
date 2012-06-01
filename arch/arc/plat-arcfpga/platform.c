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
#include <asm/serial.h>
#include <plat/irq.h>
#include <plat/memmap.h>

void __init arc_platform_early_init(void)
{
	pr_info("[plat-arcfpga]: registering early dev resources\n");

	/* TBD: early_platform_add_devices */
	arc_early_serial_reg();
}

int __init fpga_plat_init(void)
{
	pr_info("[plat-arcfpga]: registering device resources\n");

	/* TBD: Use platform_add_devices to add uart resources */

	return 0;
}
arch_initcall(fpga_plat_init);

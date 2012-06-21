/*
 * ARC FPGA Platform support code
 *
 * Copyright (C) 2012 Synopsys, Inc. (www.synopsys.com)
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

#ifdef CONFIG_ARC_HAS_BVCI_LAT_UNIT

int lat_cycles = CONFIG_BVCI_LAT_CYCLES;

/* BVCI Bus Profiler: Latency Unit */
static void setup_bvci_lat_unit(void)
{
#define MAX_BVCI_UNITS 12

	volatile unsigned int *base = (unsigned int *)BVCI_LAT_UNIT_BASE;
	volatile unsigned int *lat_unit = (unsigned int *)base + 21;
	volatile unsigned int *lat_val = (unsigned int *)base + 22;
	unsigned int unit;
	const unsigned long units_req = CONFIG_BVCI_LAT_UNITS;

	/*
	 * There are multiple Latency Units corresponding to the many
	 * interfaces of the system bus arbiter (both CPU side as well as
	 * the peripheral side).
	 *
	 * Unit  0 - System Arb and Mem Controller - adds latency to all
	 * 	    memory trasactions
	 * Unit  1 - I$ and System Bus
	 * Unit  2 - D$ and System Bus
	 * ..
	 * Unit 12 - IDE Disk controller and System Bus
	 *
	 * The programmers model requires writing to lat_unit reg first
	 * and then the latency value (cycles) to lat_value reg
	 */

	if (CONFIG_BVCI_LAT_UNITS == 0) {
		*lat_unit = 0;
		*lat_val = lat_cycles;
		pr_info("BVCI Latency for all Memory Transactions %d cycles\n",
			lat_cycles);
	}
	else {
		for_each_set_bit(unit, &units_req, MAX_BVCI_UNITS) {
			*lat_unit = unit + 1;  /* above returns 0 based */
			*lat_val = lat_cycles;
			pr_info("BVCI Latency for Unit[%d] = %d cycles\n",
				(unit + 1), lat_cycles);
		}
	}
}
#else
static void setup_bvci_lat_unit(void)
{
}
#endif

void __init arc_platform_early_init(void)
{
	pr_info("[plat-arcfpga]: registering early dev resources\n");

	setup_bvci_lat_unit();

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

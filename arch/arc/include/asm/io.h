/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_IO_H
#define _ASM_ARC_IO_H

#include <asm/byteorder.h>
#include <asm/page.h>

extern void *__ioremap(unsigned long physaddr, unsigned long size,
		       unsigned long flags);

#define ioremap_nocache(phy, sz)	__ioremap(phy, sz, 1)

/* Wrokaroud the braindead drivers which don't know what to do */
#define ioremap(phy, sz)		ioremap_nocache(phy, sz)

extern void iounmap(const void __iomem *addr);

/* Change struct page to physical address */
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)

#define virt_to_bus(a)	__virt_to_bus(a)
#define bus_to_virt(a)	__bus_to_virt(a)

#include <asm-generic/io.h>

static inline __deprecated unsigned long __virt_to_bus(volatile void *address)
{
	return virt_to_phys(address) - PAGE_OFFSET;
}

static inline __deprecated void *__bus_to_virt(unsigned long address)
{
	return phys_to_virt(address) + PAGE_OFFSET;
}

#endif /* _ASM_ARC_IO_H */

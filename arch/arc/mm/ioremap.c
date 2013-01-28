/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/cache.h>

void __iomem *ioremap(unsigned long paddr, unsigned long size)
{
	unsigned long end;

	/* Don't allow wraparound or zero size */
	end = paddr + size - 1;
	if (!size || (end < paddr))
		return NULL;

	/* If the region is h/w uncached, avoid MMU mappings */
	if (paddr >= ARC_UNCACHED_ADDR_SPACE)
		return (void __iomem *)paddr;

	return ioremap_prot(paddr, size, PAGE_KERNEL_NO_CACHE);
}
EXPORT_SYMBOL(ioremap);

/*
 * ioremap with access flags
 * Cache semantics wise it is same as ioremap - "forced" uncached.
 * However unline vanilla ioremap which bypasses ARC MMU for addresses in
 * ARC hardware uncached region, this one still goes thru the MMU as caller
 * might need finer access control (R/W/X)
 */
void __iomem *ioremap_prot(phys_addr_t phys_addr, unsigned long size,
			   unsigned long flags)
{
	void __iomem *addr;
	struct vm_struct *area;
	unsigned long offset, last_addr;
	pgprot_t prot = __pgprot(flags);

	/* Don't allow wraparound, zero size */
	last_addr = phys_addr + size - 1;
	if ((!size) || (last_addr < phys_addr))
		return NULL;

	/* An early platform driver might end up here */
	if (!slab_is_available())
		return NULL;

	/* force uncached */
	prot = pgprot_noncached(prot);

	/* Mappings have to be page-aligned */
	offset = phys_addr & ~PAGE_MASK;
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr + 1) - phys_addr;

	/*
	 * Ok, go for it..
	 */
	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	area->phys_addr = phys_addr;
	addr = (void __iomem *)area->addr;
	if (ioremap_page_range((unsigned long)addr,
			       (unsigned long)addr + size, phys_addr, prot)) {
		vunmap((void __force *)addr);
		return NULL;
	}
	return (void __iomem *)(offset + (char __iomem *)addr);
}
EXPORT_SYMBOL(ioremap_prot);


void iounmap(const void __iomem *addr)
{
	if (addr >= (void __force __iomem *)ARC_UNCACHED_ADDR_SPACE)
		return;

	vfree((void *)(PAGE_MASK & (unsigned long __force)addr));
}
EXPORT_SYMBOL(iounmap);

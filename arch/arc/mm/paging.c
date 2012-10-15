/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#ifndef NONINLINE_MEMSET

void copy_page(void *to, void *from)
{
	unsigned long reg1, reg2, reg3, reg4;

	__asm__ __volatile__(
		"mov lp_count,%6\n"
		"lp 1f\n"
		"ld.ab  %0, [%5, 4]\n\t"
		"ld.ab  %1, [%5, 4]\n\t"
		"ld.ab  %2, [%5, 4]\n\t"
		"ld.ab  %3, [%5, 4]\n\t"
		"st.ab  %0, [%4, 4]\n\t"
		"st.ab  %1, [%4, 4]\n\t"
		"st.ab  %2, [%4, 4]\n\t"
		"st.ab  %3, [%4, 4]\n\t"
		"1:\n"
		: "=&r"(reg1), "=&r"(reg2), "=&r"(reg3), "=&r"(reg4)
		: "r"(to), "r"(from), "ir"(PAGE_SIZE / 4 / 4)
		: "lp_count"
	);
}
EXPORT_SYMBOL(copy_page);

#else

void copy_page(void *to, void *from)
{
	memcpy(to, from, PAGE_SIZE);
}

#endif

void clear_user_page(void *addr, unsigned long vaddr, struct page *page)
{
	clear_page(addr);
}

/* XXX: Revisit this */
void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
		    struct page *to)
{
	copy_page(vto, vfrom);
}

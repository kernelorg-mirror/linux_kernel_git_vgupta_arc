/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#ifdef CONFIG_BLOCK_DEV_RAM
#include <linux/blk.h>
#endif
#include <linux/swap.h>
#include <linux/module.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/mmapcode.h>
#include <asm/sections.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
char empty_zero_page[PAGE_SIZE] __aligned(PAGE_SIZE);
EXPORT_SYMBOL(empty_zero_page);

unsigned long end_mem = CONFIG_ARC_PLAT_SDRAM_SIZE + CONFIG_LINUX_LINK_BASE;

void __init pagetable_init(void)
{
	pgd_init((unsigned long)swapper_pg_dir);
	pgd_init((unsigned long)swapper_pg_dir +
		 sizeof(pgd_t) * USER_PTRS_PER_PGD);
}

void __init paging_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES] = { 0, 0 };

	pagetable_init();

	zones_size[ZONE_NORMAL] =
	    (end_mem - CONFIG_LINUX_LINK_BASE) >> PAGE_SHIFT;

	/*
	 * Must not use free_area_init() as that uses PAGE_OFFSET, which
	 * need not be the case when our kernel linked at non-default addr
	 * i.e. when CONFIG_LINUX_LINK_BASE != PAGE_OFFSET
	 */
#ifdef CONFIG_FLATMEM
	free_area_init_node(0, zones_size, CONFIG_LINUX_LINK_BASE >> PAGE_SHIFT,
			    NULL);
#else
#error "Fix !CONFIG_FLATMEM"
#endif
}

/*
 * mem_init - initializes memory
 *
 * Frees up bootmem
 * Calculates and displays memory available/used
 */
void __init mem_init(void)
{
	int codesize, datasize, initsize, reserved_pages;
	int tmp;

	high_memory = (void *)end_mem;

	max_mapnr = num_physpages;

	totalram_pages = free_all_bootmem();

	reserved_pages = 0;
	for (tmp = 0; tmp < num_physpages; tmp++)
		if (PageReserved(mem_map + tmp))
			reserved_pages++;

	codesize = _etext - _text;
	datasize = _end - _etext;
	initsize = __init_end - __init_begin;

	pr_info("Memory: %luKB available (%dK code,%dK data, %dK init)\n",
		(unsigned long)nr_free_pages() << (PAGE_SHIFT - 10),
		codesize >> 10, datasize >> 10, initsize >> 10);
}

static void __init free_init_pages(const char *what, unsigned long begin,
				   unsigned long end)
{
	unsigned long addr;
	/* need to check that the page we free is not a partial page */
	for (addr = begin; addr + PAGE_SIZE <= end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		free_page(addr);
		totalram_pages++;
	}

	printk(KERN_INFO "Freeing %s: %ldk freed.  [%lux] TO [%lux]\n",
		what, (end - begin) >> 10, begin, end);
}

void free_initmem(void)
{
	free_init_pages("unused kernel memory", (unsigned long)__init_begin,
			(unsigned long)__init_end);

	mmapcode_space_init();
}

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	free_init_pages("initrd memory", start, end);
}
#endif

/*
 * Count the pages we have and Setup bootmem allocator
  */
void __init setup_arch_memory(void)
{
	int bootmap_sz;
	unsigned long first_free_pfn, kernel_end_addr;

	init_mm.start_code = (unsigned long)_text;
	init_mm.end_code = (unsigned long)_etext;
	init_mm.end_data = (unsigned long)_edata;
	init_mm.brk = (unsigned long)_end;

	/*
	 * Make sure that "_end" is page aligned in linker script
	 * so that it points to first free page in system
	 */
	kernel_end_addr = (unsigned long)_end;

	/* First free page beyond kernel image */
	first_free_pfn = PFN_DOWN(kernel_end_addr);

	/* first page of system - kernel .vector starts here */
	min_low_pfn = PFN_DOWN(CONFIG_LINUX_LINK_BASE);

	/*
	 * Last usable page of low mem (no HIGH_MEM yet for ARC port)
	 * -must be BASE + SIZE
	 */
	max_low_pfn = max_pfn = PFN_DOWN(end_mem);

	num_physpages = max_low_pfn - min_low_pfn;

	/* setup bootmem allocator */
	bootmap_sz = init_bootmem_node(NODE_DATA(0),
				       first_free_pfn,/* bitmap start */
				       min_low_pfn,   /* First pg to track */
				       max_low_pfn);  /* Last pg to track */

	/*
	 * Make all mem tracked by bootmem alloc as usable,
	 * except the bootmem bitmap itself
	 */
	free_bootmem(kernel_end_addr, end_mem - kernel_end_addr);
	reserve_bootmem(kernel_end_addr, bootmap_sz, BOOTMEM_DEFAULT);
}

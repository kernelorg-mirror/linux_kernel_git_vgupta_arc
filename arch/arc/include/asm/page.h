/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_PAGE_H
#define __ASM_ARC_PAGE_H

/* PAGE_SHIFT determines the page size */
#if defined(CONFIG_ARC_PAGE_SIZE_16K)
#define PAGE_SHIFT 14
#elif defined(CONFIG_ARC_PAGE_SIZE_4K)
#define PAGE_SHIFT 12
#else
/*
 * Default 8k
 * done this way (instead of under CONFIG_ARC_PAGE_SIZE_8K) because adhoc
 * user code (busybox appletlib.h) expects PAGE_SHIFT to be defined w/o
 * using the correct uClibc header and in their build our autoconf.h is
 * not available
 */
#define PAGE_SHIFT 13
#endif

#ifdef __ASSEMBLY__
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_OFFSET	(0x80000000)
#else
#define PAGE_SIZE	(1UL << PAGE_SHIFT)	/* Default 8K */
#define PAGE_OFFSET	(0x80000000UL)	/* Kernel starts at 2G onwards */
#endif

#define PAGE_MASK	(~(PAGE_SIZE-1))

#ifdef __KERNEL__

#ifndef __ASSEMBLY__

struct mm_struct;
struct vm_area_struct;
struct page;

/* TBD: for now don't worry about VIPT D$ aliasing */
#define clear_page(paddr)  memset((unsigned int *)(paddr), 0, PAGE_SIZE)
extern void copy_page(void *to, void *from);
extern void clear_user_page(void *addr, unsigned long vaddr, struct page *page);
extern void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
			   struct page *to);

#define get_user_page(vaddr)        __get_free_page(GFP_KERNEL)
#define free_user_page(page, addr)  free_page(addr)

#undef STRICT_MM_TYPECHECKS

#ifdef STRICT_MM_TYPECHECKS
/*
 * These are used to make use of C type-checking..
 */
typedef struct {
	unsigned long pte_lo;
} pte_t;
typedef struct {
	unsigned long pgd;
} pgd_t;
typedef struct {
	unsigned long pgprot;
} pgprot_t;
typedef unsigned long pgtable_t;

#define pte_val(x)      ((x).pte_lo)
#define pgd_val(x)      ((x).pgd)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x)        ((pte_t) { (x) })
#define __pgd(x)        ((pgd_t) { (x) })
#define __pgprot(x)     ((pgprot_t) { (x) })

#else /* !STRICT_MM_TYPECHECKS */

typedef unsigned long pte_t;
typedef unsigned long pgd_t;
typedef unsigned long pgprot_t;
typedef unsigned long pgtable_t;

#define pte_val(x)	(x)
#define pgd_val(x)	(x)
#define pgprot_val(x)	(x)
#define __pte(x)	(x)
#define __pgprot(x)	(x)

#endif

#define ARCH_PFN_OFFSET     (CONFIG_LINUX_LINK_BASE >> PAGE_SHIFT)
#include <asm-generic/memory_model.h>   /* page_to_pfn, pfn_to_page */

#define pfn_valid(pfn)      (((pfn) - ARCH_PFN_OFFSET) < max_mapnr)

/*
 * __pa, __va, virt_to_page (ALERT: deprecated, don't use them)
 *
 * These macros have historically been misnamed
 * virt here means link-address/program-address as embedded in object code.
 * So if kernel img is linked at 0x8000_0000 onwards, 0x8010_0000 will be
 * 128th page, and virt_to_page( ) will return the struct page corresp to it.
 * mem_map[ ] is an array of struct page for each page frame in the system
 *
 * Independent of where linux is linked at, link-addr = physical address
 * So the old macro  __pa = vaddr + PAGE_OFFSET - CONFIG_LINUX_LINK_BASE
 * would have been wrong in case kernel is not at 0x8zs
 */
#define __pa(vaddr)  ((unsigned long)vaddr)
#define __va(paddr)  ((void *)((unsigned long)(paddr)))

#define virt_to_page(kaddr)	\
	(mem_map + ((__pa(kaddr) - CONFIG_LINUX_LINK_BASE) >> PAGE_SHIFT))

#define virt_addr_valid(kaddr)  pfn_valid(__pa(kaddr) >> PAGE_SHIFT)

/* Default Permissions for page, used in mmap.c */
#ifdef CONFIG_ARC_STACK_NONEXEC
#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE)
#else
#define VM_DATA_DEFAULT_FLAGS   (VM_READ | VM_WRITE | VM_EXEC | \
				 VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)
#endif

#define WANT_PAGE_VIRTUAL   1

#include <asm-generic/getorder.h>

#endif /* !__ASSEMBLY__ */

/* Kernels Virtual memory area.
 * Unlike other architectures(MIPS, sh, cris ) ARC 700 does not have a
 * "kernel translated" region (like KSEG2 in MIPS). So we use a upper part
 * of the translated bottom 2GB for kernel virtual memory and protect
 * these pages from user accesses by disabling Ru, Eu and Wu.
 */
#define VMALLOC_SIZE	(0x10000000)	/* 256M */
#define VMALLOC_START	(PAGE_OFFSET - VMALLOC_SIZE)
#define VMALLOC_END	(PAGE_OFFSET)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))

/* Most of the architectures seem to be keeping some kind of padding between
 * userspace TASK_SIZE and PAGE_OFFSET. i.e TASK_SIZE != PAGE_OFFSET.
 */
#define USER_KERNEL_GUTTER    0x10000000

/* User address space:
 * On ARC700, CPU allows the entire lower half of 32 bit address space to be
 * translated. Thus potentially 2G (0:0x7FFF_FFFF) could be User vaddr space.
 * However we steal 256M for kernel addr (0x7000_0000:0x7FFF_FFFF) and another
 * 256M (0x6000_0000:0x6FFF_FFFF) is gutter between user/kernel spaces
 * Thus total User vaddr space is (0:0x5FFF_FFFF)
 */
#define TASK_SIZE   (PAGE_OFFSET - VMALLOC_SIZE - USER_KERNEL_GUTTER)

#define STACK_TOP_MAX   TASK_SIZE
#define STACK_TOP       TASK_SIZE

/* This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE      (TASK_SIZE / 3)

/* For mmap randomisation and Page coloring for share code pages */
#define HAVE_ARCH_PICK_MMAP_LAYOUT

#endif /* __KERNEL__ */

#endif /* __ASM_ARC_PAGE_H */

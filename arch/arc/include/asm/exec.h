/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __ASM_ARC_EXEC_H
#define __ASM_ARC_EXEC_H

static inline unsigned long arch_align_stack(unsigned long sp)
{
#ifdef CONFIG_ARC_ADDR_SPACE_RND
	/* ELF loader sets this flag way early.
	 * So no need to check for multiple things like
	 *   !(current->personality & ADDR_NO_RANDOMIZE)
	 *   randomize_va_space
	 */
	if (current->flags & PF_RANDOMIZE) {

		/* Stack grows down for ARC */
		sp -= get_random_int() & ~PAGE_MASK;
	}
#endif

	/* always align stack to 16 bytes */
	sp &= ~0xF;

	return sp;
}


#endif

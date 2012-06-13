/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_CMPXCHG_H
#define __ASM_ARC_CMPXCHG_H

#include <linux/types.h>

#ifdef CONFIG_ARC_HAS_LLSC

static inline unsigned long
__cmpxchg(volatile void *ptr, unsigned long expected, unsigned long new)
{
	unsigned long prev;

	__asm__ __volatile__(
	"1:	llock   %0, [%1]	\n"
	"	brne    %0, %2, 2f	\n"
	"	scond   %3, [%1]	\n"
	"	bnz     1b		\n"
	"2:				\n"
	: "=&r"(prev)
	: "r"(ptr), "ir"(expected),
	  "r"(new) /* can't be "ir". scond can't take limm for "b" */
	: "cc");

	return prev;
}

#else

static inline unsigned long
__cmpxchg(volatile void *ptr, unsigned long expected, unsigned long new)
{
	unsigned long flags;
	int prev;
	volatile unsigned long *p = ptr;

	atomic_ops_lock(flags);
	prev = *p;
	if (prev == expected)
		*p = new;
	atomic_ops_unlock(flags);
	return prev;
}

#endif /* CONFIG_ARC_HAS_LLSC */

/* unlike other APIS, cmpxchg is same as atomix_cmpxchg because
 * because the sematics of cmpxchg itself is to be atomic
 */
#define atomic_cmpxchg(v, o, n) ((int)cmpxchg(&((v)->counter), (o), (n)))

#define cmpxchg(ptr, o, n) \
	((typeof(*(ptr)))__cmpxchg((ptr), (unsigned long)(o), (unsigned long)(n)))

/******************************************************************
 * Atomically Exchange memory with a register
 ******************************************************************/
extern unsigned long __xchg_bad_pointer(void);

/* On ARC700, "ex" is inherently atomic so don't need IRQ disabling */

static inline unsigned long __xchg(unsigned long val, volatile void *ptr,
				   int size)
{
	switch (size) {
	case 4:
		__asm__ __volatile__(

		"	ex  %0, [%1]	\n"
		: "+r"(val)
		: "r"(ptr)
		: "memory");

		return val;
	}
	return __xchg_bad_pointer();
}

#define xchg(ptr, with) ((typeof(*(ptr)))__xchg((unsigned long)(with), (ptr), \
						 sizeof(*(ptr))))

#define atomic_xchg(v, new) (xchg(&((v)->counter), new))

#endif

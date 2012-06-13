/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/uaccess.h>

/****************************************************************
 * Legacy arch string routines:
 * to be removed, once the new tight asm code proves alright
 * */

#ifdef __HAVE_ARCH_SLOW_STRINGS

#ifdef __HAVE_ARCH_MEMSET

#undef memset

void *memset(void *dest, int c, size_t count)
{
	unsigned char *d_char = dest;
	unsigned char ch = (unsigned char)c;

	/* IMP: 32 is arbit, given the cost of branches etc. */
	if (count > 32) {
		unsigned int chchchch = 0;

		if (c) {
			unsigned short chch = ch | (ch << 8);
			chchchch = chch | (chch << 16);
		}

		__asm__ __volatile__(
			"   bbit0   %0, 0, 1f \n"
			"   stb.ab  %2, [%0,1]\n"
			"   sub %1, %1, 1     \n"
			"1: \n"
			"   bbit0   %0, 1, 2f \n"
			"   stw.ab  %2, [%0,2]\n"
			"   sub %1, %1, 2     \n"
			"2: \n"
			"   asr.f   lp_count, %1, 2\n"
			"   lpnz    3f\n"
			"   st.ab   %2, [%0,4]\n"
			"   sub %1, %1, 4     \n"
			"3:\n"
			"   bbit0   %1, 1, 4f \n"
			"   stw.ab  %2, [%0,2]\n"
			"   sub %1, %1, 2     \n"
			"4: \n"
			"   bbit0   %1, 0, 5f \n"
			"   stb.ab  %2, [%0,1]\n"
			"   sub %1, %1, 1     \n"
			"5: \n"
			: "+r"(d_char), "+r"(count)
			: "ir"(chchchch)
			: "lp_count", "lp_start", "lp_end");
	} else {
		int remainder = count;
		while (remainder--)
			*d_char++ = ch;
	}

	return dest;
}
EXPORT_SYMBOL(memset);

#endif /* # __HAVE_ARCH_MEMSET */

#ifdef __HAVE_ARCH_MEMCPY

#undef memcpy

void *memcpy(void *to, const void *from, size_t count)
{
	__generic_copy_from_user(to, from, count);
	return to;
}
EXPORT_SYMBOL(memcpy);

#endif /* __HAVE_ARCH_MEMCPY */
#endif /* __HAVE_ARCH_SLOW_STRING */

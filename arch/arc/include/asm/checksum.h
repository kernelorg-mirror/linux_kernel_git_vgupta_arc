/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Joern Rennecke  <joern.rennecke@embecosm.com>: Jan 2012
 *  -Insn Scheduling improvements to csum core routines.
 *      = csum_fold( ) largely derived from ARM version.
 *      = ip_fast_cum( ) to have module scheduling
 *  -gcc 4.4.x broke networking. Alias analysis needed to be primed.
 *   worked around by adding memory clobber to ip_fast_csum( )
 *
 * vineetg: May 2010
 *  -Rewrote ip_fast_cscum( ) and csum_fold( ) with fast inline asm
 */

#ifndef _ASM_ARC_CHECKSUM_H
#define _ASM_ARC_CHECKSUM_H

#include <linux/in6.h>
/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
unsigned int csum_partial(const void *buff, int len, unsigned int sum);

/*
 * the same as csum_partial, but copies from src while it
 * checksums, and handles user-space pointer exceptions correctly, when needed.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

extern __wsum csum_partial_copy(const char *src, char *dst, int len, int sum);

/*
 * the same as csum_partial_copy, but copies from user space.
 *
 * here even more important to align src and dst on a 32-bit (or even
 * better 64-bit) boundary
 */

extern __wsum csum_partial_copy_from_user(const char *src, char *dst,
						int len, int sum,
						int *csum_err);

#define csum_partial_copy_nocheck(src, dst, len, sum)	\
		csum_partial_copy((src), (dst), (len), (sum))

/*
 *	Fold a partial checksum
 *
 *  The 2 swords comprising the 32bit sum are added, any carry to 16th bit
 *  added back and final sword result inverted.
 */
static inline __sum16 csum_fold(__wsum s)
{
	unsigned r = s << 16 | s >> 16;	/* ror */
	s = ~s;
	s -= r;
	return s >> 16;
}

/*
 *	This is a version of ip_compute_csum() optimized for IP headers,
 *	which always checksum on 4 octet boundaries.
 */
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	const void *ptr = iph;
	unsigned int tmp, tmp2, sum;

	__asm__(
	"	ld.ab  %0, [%3, 4]		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"	sub    %1, %4, 2		\n"
	"	lsr.f  lp_count, %1, 1		\n"
	"	bcc    0f			\n"
	"	add.f  %0, %0, %2		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"0:	lp     1f			\n"
	"	ld.ab  %1, [%3, 4]		\n"
	"	adc.f  %0, %0, %2		\n"
	"	ld.ab  %2, [%3, 4]		\n"
	"	adc.f  %0, %0, %1		\n"
	"1:	adc.f  %0, %0, %2		\n"
	"	add.cs %0,%0,1			\n"
	: "=&r"(sum), "=r"(tmp), "=&r"(tmp2),
		"+&r"
		(ptr)
:		"r"(ihl)
:		"cc", "lp_count", "memory");

	return csum_fold(sum);
}

static inline __wsum csum_tcpudp_nofold(unsigned long saddr,
	unsigned long daddr, unsigned short len, unsigned short proto,
	unsigned int sum)
{
	__asm__ __volatile__(
	"	add.f %0, %0, %1	\n"
	"	adc.f %0, %0, %2	\n"
	"	adc.f %0, %0, %3	\n"
	"	adc.f %0, %0, %4	\n"
	"	adc   %0, %0, 0	\n"
	: "=r"(sum)
	: "r"(saddr), "r"(daddr), "r"(ntohs(len) << 16),
	  "r"(proto << 8), "0"(sum)
	: "cc");

	return sum;
}

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
static inline unsigned short int csum_tcpudp_magic(unsigned long saddr,
	unsigned long daddr, unsigned short len, unsigned short proto,
	unsigned int sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

unsigned short ip_compute_csum(const unsigned char *buff, int len);

#endif /* _ASM_ARC_CHECKSUM_H */

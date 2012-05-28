/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: June 2010
 *    -__clear_user( ) called multiple times during elf load was byte loop
 *    converted to do as much word clear as possible.
 *
 * vineetg: Dec 2009
 *    -Hand crafted constant propagation for "constant" copy sizes
 *    -stock kernel shrunk by 33K at -O3
 *
 * vineetg: Sept 2009
 *    -Added option to (UN)inline copy_(to|from)_user to reduce code sz
 *    -kernel shrunk by 200K even at -O3 (gcc 4.2.1)
 *    -Enabled when doing -Os
 *
 * Amit Bhor, Sameer Dhavale: Codito Technologies 2004
 */

#ifndef _ASM_ARC_UACCESS_H
#define _ASM_ARC_UACCESS_H

#include <linux/sched.h>
#include <asm/errno.h>
#include <linux/string.h>	/* for generic string functions */


#define __kernel_ok		(segment_eq(get_fs(),KERNEL_DS))
#define __user_ok(addr, sz)	(((sz) <= TASK_SIZE) && \
				 (((addr)+(sz)) <= get_fs()))
#define __access_ok(addr, sz)	(unlikely(__kernel_ok) || \
				 likely(__user_ok((addr), (sz))))

#if 0  /* XXX: Remove this once we are relatively stable */
#define get_user(x,p)       __get_user_check((x),(p),sizeof(*(p)))
#define __get_user(x,p)     __get_user_nocheck((x),(p),sizeof(*(p)))


#define __get_user_check(x,ptr,size)                            \
({                                                              \
    long __gu_err = -EFAULT, __gu_val = 0;                      \
    const __typeof__(*(ptr)) *__gu_addr = (ptr);                \
    if (access_ok(VERIFY_READ,__gu_addr,size)) {                \
        __gu_err = 0;                                           \
        __get_user_size(__gu_val,__gu_addr,(size),__gu_err);    \
    }                                                           \
    (x) = (__typeof__(*(ptr)))__gu_val;                         \
    __gu_err;                                                   \
})

#define __get_user_nocheck(x,ptr,size)                  \
({                                                      \
    long __gu_err = 0, __gu_val;                        \
    __get_user_size(__gu_val,(ptr),(size),__gu_err);    \
    (x) = (__typeof__(*(ptr)))__gu_val;                 \
    __gu_err;                                           \
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval)              \
do {                                                    \
    switch (size) {                                     \
    case 1: __get_user_asm(x,ptr,retval,"ldb"); break;  \
    case 2: __get_user_asm(x,ptr,retval,"ldw"); break;  \
    case 4: __get_user_asm(x,ptr,retval,"ld");  break;  \
    case 8: __get_user_asm_64(x,ptr,retval);    break;  \
    default: (x) = __get_user_bad();                    \
    }                                                   \
} while (0)

// FIXME :: check if the "nop" is required
#define __get_user_asm(x,addr,err,op)       \
    __asm__ __volatile__(                   \
    "1: "op"    %1,[%2]\n"                  \
    "   mov %0, 0x0\n"                      \
    "2: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "3: mov %0, %3\n"                       \
    "   j   2b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,3b\n"                      \
    "   .previous\n"                        \
    : "=r" (err), "=&r" (x)                 \
    : "r" (addr), "i" (-EFAULT), "0" (err))

#define __get_user_asm_64(x,addr,err)       \
    __asm__ __volatile__(                   \
    "1: ld  %1,[%2]\n"                      \
    "2: ld  %R1,[%2, 4]\n"                  \
    "   mov %0, 0x0\n"                      \
    "3: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "4: mov %0, %3\n"                       \
    "   mov %1, 0x0\n"                      \
    "   j   3b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,4b\n"                      \
    "   .word 2b,4b\n"                      \
    "   .previous\n"                        \
    : "=r" (err), "=&r" (x)                 \
    : "r" (addr), "i" (-EFAULT), "0" (err))
#endif

#if 0
#define put_user(x,p)       __put_user_check((__typeof(*(p)))(x),(p),sizeof(*(p)))
#define __put_user(x,p)     __put_user_nocheck((__typeof(*(p)))(x),(p),sizeof(*(p)))
#define __put_user_check(x,ptr,size)                    \
({                                                      \
    long __pu_err = -EFAULT;                            \
    __typeof__(*(ptr)) *__pu_addr = (ptr);              \
    if (access_ok(VERIFY_WRITE,__pu_addr,size)) {       \
        __pu_err = 0;                                   \
        __put_user_size((x),__pu_addr,(size),__pu_err); \
    }                                                   \
    __pu_err;                                           \
})

#define __put_user_nocheck(x,ptr,size)              \
({                                                  \
    long __pu_err = 0;                              \
    __typeof__(*(ptr)) *__pu_addr = (ptr);          \
    __put_user_size((x),__pu_addr,(size),__pu_err); \
    __pu_err;                                       \
})


extern long __put_user_bad(void);

#define __put_user_size(x,ptr,size,retval)              \
do {                                                    \
    switch (size) {                                     \
    case 1: __put_user_asm(x,ptr,retval,"stb"); break;  \
    case 2: __put_user_asm(x,ptr,retval,"stw"); break;  \
    case 4: __put_user_asm(x,ptr,retval,"st");  break;  \
    case 8: __put_user_asm_64(x,ptr,retval);    break;  \
    default: __put_user_bad();                          \
    }                                                   \
} while (0)

#define __put_user_asm(x,addr,err,op)       \
    __asm__ __volatile__(                   \
    "1: "op"    %1,[%2]\n"                  \
    "   mov %0, 0x0\n"                      \
    "2: ;nop\n"                              \
    "   .section .fixup, \"ax\"\n"          \
    "   .align 4\n"                         \
    "3: mov %0, %3\n"                       \
    "   j   2b\n"                           \
    "   .previous\n"                        \
    "   .section __ex_table, \"a\"\n"       \
    "   .align 4\n"                         \
    "   .word 1b,3b\n"                      \
    "   .previous\n"                        \
    : "=r" (err)                            \
    : "r" (x), "r" (addr),"i" (-EFAULT), "0" (err))

#define __put_user_asm_64(x,addr,err)   \
    __asm__ __volatile__(               \
    "1: st  %1,[%2]\n"                  \
    "2: st  %R1,[%2, 4]\n"              \
    "   mov %0, 0x0\n"                  \
    "3: ;nop\n"                          \
    "   .section .fixup, \"ax\"\n"      \
    "   .align 4\n"                     \
    "4: mov %0, %3\n"                   \
    "   mov %1, 0x0\n"                  \
    "   j   3b\n"                       \
    "   .previous\n"                    \
    "   .section __ex_table, \"a\"\n"   \
    "   .align 4\n"                     \
    "   .word 1b,4b\n"                  \
    "   .word 2b,4b\n"                  \
    "   .previous\n"                    \
    : "=r" (err)                        \
    : "r" (x), "r" (addr),"i" (-EFAULT), "0" (err))
#endif

extern unsigned long slowpath_copy_from_user(void *to, const void __user *from,
					     unsigned long n);

static inline unsigned long
__arc_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	long res = 0;
	char val;
	unsigned long tmp1, tmp2, tmp3, tmp4;
	unsigned long orig_n = n;

	if (n == 0)
		return 0;

	/* unaligned */
	if (((unsigned long)to & 0x3) || ((unsigned long)from & 0x3))
		return slowpath_copy_from_user(to, from, n);

	/*
	 * Hand-crafted constant propagation to reduce code sz of the
	 * laddered copy 16x,8,4,2,1
	 */
	if (__builtin_constant_p(orig_n)) {
		res = orig_n;

		if (orig_n / 16) {
			orig_n = orig_n % 16;

			__asm__ __volatile__(
			"	lsr   lp_count, %7,4		\n"
			"	lp    3f			\n"
			"1:	ld.ab   %3, [%2, 4]		\n"
			"11:	ld.ab   %4, [%2, 4]		\n"
			"12:	ld.ab   %5, [%2, 4]		\n"
			"13:	ld.ab   %6, [%2, 4]		\n"
			"	st.ab   %3, [%1, 4]		\n"
			"	st.ab   %4, [%1, 4]		\n"
			"	st.ab   %5, [%1, 4]		\n"
			"	st.ab   %6, [%1, 4]		\n"
			"	sub     %0,%0,16		\n"
			"3:	;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   3b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   1b, 4b			\n"
			"	.word   11b,4b			\n"
			"	.word   12b,4b			\n"
			"	.word   13b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from),
			  "=r"(tmp1), "=r"(tmp2), "=r"(tmp3), "=r"(tmp4)
			: "ir"(n)
			: "lp_count", "memory");
		}
		if (orig_n / 8) {
			orig_n = orig_n % 8;

			__asm__ __volatile__(
			"14:	ld.ab   %3, [%2,4]		\n"
			"15:	ld.ab   %4, [%2,4]		\n"
			"	st.ab   %3, [%1,4]		\n"
			"	st.ab   %4, [%1,4]		\n"
			"	sub     %0,%0,8			\n"
			"31:	;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   31b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   14b,4b			\n"
			"	.word   15b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from),
			  "=r"(tmp1), "=r"(tmp2)
			:
			: "memory");
		}
		if (orig_n / 4) {
			orig_n = orig_n % 4;

			__asm__ __volatile__(
			"16:	ld.ab   %3, [%2,4]		\n"
			"	st.ab   %3, [%1,4]		\n"
			"	sub     %0,%0,4			\n"
			"32:	;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   32b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   16b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
		if (orig_n / 2) {
			orig_n = orig_n % 2;

			__asm__ __volatile__(
			"17:	ldw.ab   %3, [%2,2]		\n"
			"	stw.ab   %3, [%1,2]		\n"
			"	sub      %0,%0,2		\n"
			"33:	;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   33b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   17b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
		if (orig_n & 1) {
			__asm__ __volatile__(
			"18:	ldb.ab   %3, [%2,2]		\n"
			"	stb.ab   %3, [%1,2]		\n"
			"	sub      %0,%0,1		\n"
			"34:	; nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   34b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   18b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
	} else {  /* n is NOT constant, so laddered copy of 16x,8,4,2,1  */

		__asm__ __volatile__(
		"	mov %0,%3			\n"
		"	lsr.f   lp_count, %3,4		\n"  /* 16x bytes */
		"	lpnz    3f			\n"
		"1:	ld.ab   %5, [%2, 4]		\n"
		"11:	ld.ab   %6, [%2, 4]		\n"
		"12:	ld.ab   %7, [%2, 4]		\n"
		"13:	ld.ab   %8, [%2, 4]		\n"
		"	st.ab   %5, [%1, 4]		\n"
		"	st.ab   %6, [%1, 4]		\n"
		"	st.ab   %7, [%1, 4]		\n"
		"	st.ab   %8, [%1, 4]		\n"
		"	sub     %0,%0,16		\n"
		"3:	and.f   %3,%3,0xf		\n"  /* stragglers */
		"	bz      34f			\n"
		"	bbit0   %3,3,31f		\n"  /* 8 bytes left */
		"14:	ld.ab   %5, [%2,4]		\n"
		"15:	ld.ab   %6, [%2,4]		\n"
		"	st.ab   %5, [%1,4]		\n"
		"	st.ab   %6, [%1,4]		\n"
		"	sub.f   %0,%0,8			\n"
		"31:	bbit0   %3,2,32f		\n"  /* 4 bytes left */
		"16:	ld.ab   %5, [%2,4]		\n"
		"	st.ab   %5, [%1,4]		\n"
		"	sub.f   %0,%0,4			\n"
		"32:	bbit0   %3,1,33f		\n"  /* 2 bytes left */
		"17:	ldw.ab  %5, [%2,2]		\n"
		"	stw.ab  %5, [%1,2]		\n"
		"	sub.f   %0,%0,2			\n"
		"33:	bbit0   %3,0,34f		\n"
		"18:	ldb.ab  %5, [%2,1]		\n"  /* 1 byte left */
		"	stb.ab  %5, [%1,1]		\n"
		"	sub.f   %0,%0,1			\n"
		"34:	;nop				\n"
		"	.section .fixup, \"ax\"		\n"
		"	.align 4			\n"
		"4:	j   34b				\n"
		"	.previous			\n"
		"	.section __ex_table, \"a\"	\n"
		"	.align 4			\n"
		"	.word   1b, 4b			\n"
		"	.word   11b,4b			\n"
		"	.word   12b,4b			\n"
		"	.word   13b,4b			\n"
		"	.word   14b,4b			\n"
		"	.word   15b,4b			\n"
		"	.word   16b,4b			\n"
		"	.word   17b,4b			\n"
		"	.word   18b,4b			\n"
		"	.previous			\n"
		: "=r" (res), "=r"(to), "=r"(from), "=r"(n), "=r"(val),
		  "=r"(tmp1), "=r"(tmp2), "=r"(tmp3), "=r"(tmp4)
		: "3"(n), "1"(to), "2"(from)
		: "lp_count", "memory");
	}

	return res;
}

extern unsigned long slowpath_copy_to_user(void __user *to, const void *from,
					   unsigned long n);

static inline unsigned long
__arc_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	long res = 0;
	char val;
	unsigned long tmp1, tmp2, tmp3, tmp4;
	unsigned long orig_n = n;

	if (n == 0)
		return 0;

	/* unaligned */
	if (((unsigned long)to & 0x3) || ((unsigned long)from & 0x3))
		return slowpath_copy_to_user(to, from, n);

	if (__builtin_constant_p(orig_n)) {
		res = orig_n;

		if (orig_n / 16) {
			orig_n = orig_n % 16;

			__asm__ __volatile__(
			"	lsr lp_count, %7,4		\n"
			"	lp  3f				\n"
			"	ld.ab %3, [%2, 4]		\n"
			"	ld.ab %4, [%2, 4]		\n"
			"	ld.ab %5, [%2, 4]		\n"
			"	ld.ab %6, [%2, 4]		\n"
			"1:	st.ab %3, [%1, 4]		\n"
			"11:	st.ab %4, [%1, 4]		\n"
			"12:	st.ab %5, [%1, 4]		\n"
			"13:	st.ab %6, [%1, 4]		\n"
			"	sub   %0, %0, 16		\n"
			"3:;nop					\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   3b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   1b, 4b			\n"
			"	.word   11b,4b			\n"
			"	.word   12b,4b			\n"
			"	.word   13b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from),
			  "=r"(tmp1), "=r"(tmp2), "=r"(tmp3), "=r"(tmp4)
			: "ir"(n)
			: "lp_count", "memory");
		}
		if (orig_n / 8) {
			orig_n = orig_n % 8;

			__asm__ __volatile__(
			"	ld.ab   %3, [%2,4]		\n"
			"	ld.ab   %4, [%2,4]		\n"
			"14:	st.ab   %3, [%1,4]		\n"
			"15:	st.ab   %4, [%1,4]		\n"
			"	sub     %0, %0, 8		\n"
			"31:;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   31b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   14b,4b			\n"
			"	.word   15b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from),
			  "=r"(tmp1), "=r"(tmp2)
			:
			: "memory");
		}
		if (orig_n / 4) {
			orig_n = orig_n % 4;

			__asm__ __volatile__(
			"	ld.ab   %3, [%2,4]		\n"
			"16:	st.ab   %3, [%1,4]		\n"
			"	sub     %0, %0, 4		\n"
			"32:;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   32b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   16b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
		if (orig_n / 2) {
			orig_n = orig_n % 2;

			__asm__ __volatile__(
			"	ldw.ab    %3, [%2,2]		\n"
			"17:	stw.ab    %3, [%1,2]		\n"
			"	sub       %0, %0, 2		\n"
			"33:;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   33b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   17b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
		if (orig_n & 1) {
			__asm__ __volatile__(
			"	ldb.ab  %3, [%2,1]		\n"
			"18:	stb.ab  %3, [%1,1]		\n"
			"	sub     %0, %0, 1		\n"
			"34:	;nop				\n"
			"	.section .fixup, \"ax\"		\n"
			"	.align 4			\n"
			"4:	j   34b				\n"
			"	.previous			\n"
			"	.section __ex_table, \"a\"	\n"
			"	.align 4			\n"
			"	.word   18b,4b			\n"
			"	.previous			\n"
			: "+r" (res), "+r"(to), "+r"(from), "=r"(tmp1)
			:
			: "memory");
		}
	} else {  /* n is NOT constant, so laddered copy of 16x,8,4,2,1  */

		__asm__ __volatile__(
		"	mov   %0,%3			\n"
		"	lsr.f lp_count, %3,4		\n"  /* 16x bytes */
		"	lpnz  3f			\n"
		"	ld.ab %5, [%2, 4]		\n"
		"	ld.ab %6, [%2, 4]		\n"
		"	ld.ab %7, [%2, 4]		\n"
		"	ld.ab %8, [%2, 4]		\n"
		"1:	st.ab %5, [%1, 4]		\n"
		"11:	st.ab %6, [%1, 4]		\n"
		"12:	st.ab %7, [%1, 4]		\n"
		"13:	st.ab %8, [%1, 4]		\n"
		"	sub   %0, %0, 16		\n"
		"3:	and.f %3,%3,0xf			\n" /* stragglers */
		"	bz 34f				\n"
		"	bbit0   %3,3,31f		\n" /* 8 bytes left */
		"	ld.ab   %5, [%2,4]		\n"
		"	ld.ab   %6, [%2,4]		\n"
		"14:	st.ab   %5, [%1,4]		\n"
		"15:	st.ab   %6, [%1,4]		\n"
		"	sub.f   %0, %0, 8		\n"
		"31:	bbit0   %3,2,32f		\n"  /* 4 bytes left */
		"	ld.ab   %5, [%2,4]		\n"
		"16:	st.ab   %5, [%1,4]		\n"
		"	sub.f   %0, %0, 4		\n"
		"32:	bbit0 %3,1,33f			\n"  /* 2 bytes left */
		"	ldw.ab    %5, [%2,2]		\n"
		"17:	stw.ab    %5, [%1,2]		\n"
		"	sub.f %0, %0, 2			\n"
		"33:	bbit0 %3,0,34f			\n"
		"	ldb.ab    %5, [%2,1]		\n"  /* 1 byte left */
		"18:	stb.ab  %5, [%1,1]		\n"
		"	sub.f %0, %0, 1			\n"
		"34:	;nop				\n"
		"	.section .fixup, \"ax\"		\n"
		"	.align 4			\n"
		"4:	j   34b				\n"
		"	.previous			\n"
		"	.section __ex_table, \"a\"	\n"
		"	.align 4			\n"
		"	.word   1b, 4b			\n"
		"	.word   11b,4b			\n"
		"	.word   12b,4b			\n"
		"	.word   13b,4b			\n"
		"	.word   14b,4b			\n"
		"	.word   15b,4b			\n"
		"	.word   16b,4b			\n"
		"	.word   17b,4b			\n"
		"	.word   18b,4b			\n"
		"	.previous			\n"
		: "=r" (res), "=r"(to), "=r"(from), "=r"(n), "=r"(val),
		  "=r"(tmp1), "=r"(tmp2), "=r"(tmp3), "=r"(tmp4)
		: "3"(n), "1"(to), "2"(from)
		: "lp_count", "memory");
	}

	return res;
}

static inline unsigned long __arc_clear_user(void __user *to, unsigned long n)
{
	long res = n;
	unsigned char *d_char = to;

	__asm__ __volatile__(
	"	bbit0   %0, 0, 1f		\n"
	"75:	stb.ab  %2, [%0,1]		\n"
	"	sub %1, %1, 1			\n"
	"1:	bbit0   %0, 1, 2f		\n"
	"76:	stw.ab  %2, [%0,2]		\n"
	"	sub %1, %1, 2			\n"
	"2:	asr.f   lp_count, %1, 2		\n"
	"	lpnz    3f			\n"
	"77:	st.ab   %2, [%0,4]		\n"
	"	sub %1, %1, 4			\n"
	"3:	bbit0   %1, 1, 4f		\n"
	"78:	stw.ab  %2, [%0,2]		\n"
	"	sub %1, %1, 2			\n"
	"4:	bbit0   %1, 0, 5f		\n"
	"79:	stb.ab  %2, [%0,1]		\n"
	"	sub %1, %1, 1			\n"
	"5:					\n"
	"	.section .fixup, \"ax\"		\n"
	"	.align 4			\n"
	"3:	j   5b				\n"
	"	.previous			\n"
	"	.section __ex_table, \"a\"	\n"
	"	.align 4			\n"
	"	.word   75b, 3b			\n"
	"	.word   76b, 3b			\n"
	"	.word   77b, 3b			\n"
	"	.word   78b, 3b			\n"
	"	.word   79b, 3b			\n"
	"	.previous			\n"
	: "+r"(d_char), "+r"(res)
	: "i"(0)
	: "lp_count", "lp_start", "lp_end", "memory");

	return res;
}

static inline long
__arc_strncpy_from_user(char *dst, const char __user *src, long count)
{
	long res = count;
	char val;
	unsigned int hw_count;

	if (count == 0)
		return 0;

	__asm__ __volatile__(
	"	lp 2f		\n"
	"1:	ldb.ab  %3, [%2, 1]		\n"
	"	breq.d  %3, 0, 2f		\n"
	"	stb.ab  %3, [%1, 1]		\n"
	"2:	sub %0, %6, %4			\n"
	"3:	;nop				\n"
	"	.section .fixup, \"ax\"		\n"
	"	.align 4			\n"
	"4:	mov %0, %5			\n"
	"	j   3b				\n"
	"	.previous			\n"
	"	.section __ex_table, \"a\"	\n"
	"	.align 4			\n"
	"	.word   1b, 4b			\n"
	"	.previous			\n"
	: "=r"(res), "+r"(dst), "+r"(src), "=&r"(val), "=l"(hw_count)
	: "g"(-EFAULT), "ir"(count), "4"(count)	/* this "4" seeds lp_count */
	: "memory");

	return res;
}

static inline long __arc_strnlen_user(const char __user *s, long n)
{
	long res, tmp1, cnt;
	char val;

	__asm__ __volatile__(
	"	mov %2, %1			\n"
	"1:	ldb.ab  %3, [%0, 1]		\n"
	"	breq.d  %3, 0, 2f		\n"
	"	sub.f   %2, %2, 1		\n"
	"	bnz 1b				\n"
	"	sub %2, %2, 1			\n"
	"2:	sub %0, %1, %2			\n"
	"3:	;nop				\n"
	"	.section .fixup, \"ax\"		\n"
	"	.align 4			\n"
	"4:	mov %0, 0			\n"
	"	j   3b				\n"
	"	.previous			\n"
	"	.section __ex_table, \"a\"	\n"
	"	.align 4			\n"
	"	.word 1b, 4b			\n"
	"	.previous			\n"
	: "=r"(res), "=r"(tmp1), "=r"(cnt), "=r"(val)
	: "0"(s), "1"(n)
	: "memory");

	return res;
}

#define __copy_from_user(t, f, n)	__arc_copy_from_user(t, f, n)
#define __copy_to_user(t, f, n)		__arc_copy_to_user(t, f, n)
#define __clear_user(d, n)		__arc_clear_user(d, n)
#define __strncpy_from_user(d, s, n)	__arc_strncpy_from_user(d, s, n)
#define __strnlen_user(s, n)		__arc_strnlen_user(s, n)

#include <asm-generic/uaccess.h>

extern int fixup_exception(struct pt_regs *regs);

#endif

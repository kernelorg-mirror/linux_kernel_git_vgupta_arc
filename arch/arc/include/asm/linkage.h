/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_LINKAGE_H
#define __ASM_LINKAGE_H

#ifdef __ASSEMBLY__

#define ASM_NL		 `	/* use '`' to mark new line in macro */

#define END(name) 			\
	.LFE##name:		ASM_NL	\
	.size name, .-name


#define DW2_CIE()				ASM_NL \
	.section .debug_frame,"",@progbits	ASM_NL \
.Lframe0:					ASM_NL \
	.4byte	@.LECIE0-@.LSCIE0		ASM_NL \
.LSCIE0:					ASM_NL \
	.4byte	0xffffffff			ASM_NL \
	.byte	0x1				ASM_NL \
	.string	""				ASM_NL \
	.uleb128 0x1				ASM_NL \
	.sleb128 -4				ASM_NL \
	.byte	0x1f				ASM_NL \
	.byte	0xc				ASM_NL \
	.uleb128 0x1c				ASM_NL \
	.uleb128 0				ASM_NL \
	.align 4				ASM_NL \
.LECIE0:

#define DW2_FDE(sym)				ASM_NL \
	.4byte	@.LEFDE0##sym-@.LASFDE0##sym	ASM_NL \
.LASFDE0##sym:					ASM_NL \
	.4byte	@.Lframe0			ASM_NL \
	.4byte	@##sym				ASM_NL \
	.4byte	@.LFE##sym - @sym		ASM_NL \
	.align 4				ASM_NL \
.LEFDE0##sym:

/* annotation for data/code we want in DCCM/ICCM - if enabled in .config */
.macro ARCFP_DATA nm
#ifdef CONFIG_ARC_HAS_DCCM
	.section .data.arcfp
#else
	.section .data
#endif
	.global \nm
.endm

.macro ARCFP_CODE
#ifdef CONFIG_ARC_HAS_ICCM
	.section .text.arcfp, "ax",@progbits
#else
	.section .text, "ax",@progbits
#endif
.endm

#else	/* !__ASSEMBLY__ */

#ifdef CONFIG_ARC_HAS_ICCM
#define __arcfp_code __attribute__((__section__(".text.arcfp")))
#else
#define __arcfp_code __attribute__((__section__(".text")))
#endif

#ifdef CONFIG_ARC_HAS_DCCM
#define __arcfp_data __attribute__((__section__(".data.arcfp")))
#else
#define __arcfp_data __attribute__((__section__(".data")))
#endif

#endif /* __ASSEMBLY__ */

#endif

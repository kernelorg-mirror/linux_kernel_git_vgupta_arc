/*
 * ARC CPU startup Code
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Dec 2007
 *  -Check if we are running on Simulator or on real hardware
 *      to skip certain things during boot on simulator
 */

#include <asm/asm-offsets.h>
#include <asm/entry.h>
#include <linux/linkage.h>
#include <asm/arcregs.h>

	.cpu A7
	.data
	.globl atag_head
	.align 4
atag_head:
	.long 0

	.section .init.text, "ax",@progbits
	.type stext, @function
	.globl stext
stext:
	;adding support for atag parsing
	;If kernel parameters are passed by u-boot, then
	; r0 = 0
	; r1 = magic number (board identity)
	; r2 = address of ATAG parameter list

	cmp r0, 0x0
	jne no_params
	lr  r3, [identity]
	cmp r1, r3
	jne no_params
	st  r2, [atag_head]


no_params:
#ifdef CONFIG_SMP
	lr  r0,[identity]

	; processor ID [ 3 2 <1> 0 ]
	; For ARC700 it is 0. For ARC800 it is core-ID (0, 1, ...)
	lsr r1, r0, 8
	and.f r1, r1, 0x000000ff

	; Whether it be single core in ARC700 or Boot CPU of ARC800,
	; both have processor-ID = 0.
	; In either case, jump to master_proceed.
	; Code same for SMP and Non-SMP

	bz master_proceed

.Lmyself:
	; Secondary Halt self. Later master will set PC and clear halt bit
	flag 1
	; When debugging, Metaware debugger steps thru both, hence this loop
	b .Lmyself


master_proceed:
#endif

#ifdef CONFIG_ARC_UBOOT_CMDLINE

; u-boot passes the command line variable at the beginning of BSS
; copy the first few bytes out of the BSS into the command_line variable.

	mov r5, [@command_line]
	mov r6, __bss_start

__copy_commandline:
	ld.ab   r0, [r6,4]
	cmp     r0, 0
	beq     __clear_bss
	st.ab    r0, [r5,4]
	j       __copy_commandline
#endif

__clear_bss:

; Clear BSS before updating any globals
; If u-boot is configured, then only clean the beginning.

	mov r5, __bss_start
#ifdef CONFIG_ARC_UBOOT_CMDLINE
	mov r6, __bss_start + 256
#else
	mov r6, __bss_stop
#endif

__clear_bss_loop:

	st.ab   0, [r5,4]
	brlt    r5, r6, __clear_bss_loop

	; chip-id [ <3 2> 1 0 ]
	; useful for determining SIM vs Real H/w (SIM = 0xffff)
	lr  r0, [identity]
	lsr r3, r0, 16
	sub r3, 0xffff, r3
	st  r3, [@running_on_hw] ; 0 for sim, non-zero for real hw

#ifdef CONFIG_ARC_CURR_IN_REG
	; setup init_task as "current"
	mov r25, [@init_task]
	st r25, [@_current_task]
#endif

	; setup stack (fp, sp)
	mov fp, 0
	mov sp,init_thread_union + THREAD_SIZE

	/* Jump to start of kernel initialization routine */
	jal start_kernel

#ifdef CONFIG_SMP
;----------------------------------------------------------------
;     First lines of code run by secondary before jumping to 'C'
;----------------------------------------------------------------
	.section .init.text, "ax",@progbits
	.type first_lines_of_secondary, @function
	.globl first_lines_of_secondary

first_lines_of_secondary:

	; setup idle task for this cpu as "current"
	ld  r0, [@secondary_idle_tsk]
#ifdef CONFIG_ARC_CURR_IN_REG
	mov  r25, r0
#endif

	; setup stack
	mov fp, 0

	; set it's stack base to tsk->thread_info bottom
	ld  r0, [r0, TASK_THREAD_INFO]	 ; tsk->thread_info
	add2 sp, r0, (THREAD_SIZE - 4)/4 ; one word GUTTER at bottom

	j start_kernel_secondary

#endif

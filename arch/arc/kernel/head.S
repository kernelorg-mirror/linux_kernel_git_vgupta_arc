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

	; CPU ID [ 3 2 <1> 0 ]
	; For ARC700 it is 0.
	; For ARC700-smp-extn: core-ID (0,1,...)  Master ID is always 0
	GET_CPU_ID  r1

	; Boot(Master) proceeds
	; Non-Master waits to be booted (mechanims are platform dependent)
	cmp r1, 0
	jnz arc_platform_smp_wait_to_boot
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

	mov r9, @init_task
	SET_CURR_TASK_ON_CPU  r9, r0	; r9 = tsk, r0 = scratch

	; setup stack (fp, sp)
	mov fp, 0

	; tsk->thread_info is really a PAGE, whose bottom hoists stack
	GET_TSK_STACK_BASE r9, sp	; r9 = tsk, sp = stack base(output)

	j start_kernel	; "C" entry point

#ifdef CONFIG_SMP
;----------------------------------------------------------------
;     First lines of code run by secondary before jumping to 'C'
;----------------------------------------------------------------
	.section .init.text, "ax",@progbits
	.type first_lines_of_secondary, @function
	.globl first_lines_of_secondary

first_lines_of_secondary:

	; setup per-cpu idle task as "current" on this CPU
	ld  r0, [@secondary_idle_tsk]
	SET_CURR_TASK_ON_CPU  r0, r1

	; setup stack (fp, sp)
	mov fp, 0

	; set it's stack base to tsk->thread_info bottom
	GET_TSK_STACK_BASE r0, sp

	j start_kernel_secondary

#endif

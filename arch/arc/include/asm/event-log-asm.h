/*
 *  Low level Event Capture API callable from Assembly Code
 *  vineetg: Feb 2008
 *
 *  TBD: SMP Safe
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_EVENT_LOG_ASM_H
#define __ASM_ARC_EVENT_LOG_ASM_H

#include <asm/event-log.h>

#ifdef __ASSEMBLY__

#ifndef CONFIG_ARC_DBG_EVENT_TIMELINE

.macro TAKE_SNAP_EXCP_TLB r0, r1
.endm

.macro TAKE_SNAP_TLB_REFILL r0, r1
.endm

.macro TAKE_SNAP_SYSCALL r0, r1
.endm

.macro TAKE_SNAP_C_FROM_ASM type
.endm

#else /* CONFIG_ARC_DBG_EVENT_TIMELINE */

#include <asm/asm-offsets.h>

/*
 * Log buffer and iterator are uncached so when we retrieve them form mdb,
 * nothing is left in cache
 */
#define LD_DI	ld.di
#define ST_DI	st.di

#ifdef CONFIG_ISA_ARCV2

.macro IRQ_SAVE r0, r1
	clri \r0
.endm

.macro IRQ_RESTORE r0
	seti \r0
.endm

.macro  SMP_BARRIER
	dmb 3
.endm

.macro GET_CPU_TS	r0
#ifdef CONFIG_SMP
	mov	\r0, 0x42	/* MCIP GFRC counter */
	sr	\r0, [0x600]
	lr	\r0, [0x602]
	;ld	\r0, [@jiffies]
#else
	lr	\r0, [0x100]	; TIMER1
#endif
.endm

#else	/* ISA_ARCOMPACT */

.macro IRQ_SAVE r0, r1
	lr	\r0, [status32]
	bic	\r1, \r0, (STATUS_E1_MASK | STATUS_E2_MASK)
	flag	\r1
.endm

.macro IRQ_RESTORE r0
	flag	\r0
.endm

.macro GET_CPU_TS	r0
	lr	\r0, [0x100]	; TIMER1
.endm
#endif

.macro  SNAP_LOCK r0, r1
	IRQ_SAVE	\r0, \r1
	PUSH		\r0	; save flags on stack

#if defined(CONFIG_SMP)
1:
	mov		\r0, 1
	ex		\r0, [@timeline_lock]
	breq		\r0, 1, 1b

	SMP_BARRIER
#endif
.endm

.macro	SNAP_UNLOCK r0
#if defined(CONFIG_SMP)
	SMP_BARRIER

	mov		\r0, 0
	st		\r0, [@timeline_lock]
#endif

	POP		\r0
	IRQ_RESTORE	\r0
.endm

.macro SNAP_PROLOGUE r0, r1, event_id
	SNAP_LOCK	\r0, \r1

	mov	\r0, @timeline_log
	mov	\r1, @timeline_ctr

	LD_DI	\r1, [\r1]
	mpyu	\r1, \r1, EVLOG_RECORD_SZ
	add	\r1, \r0, \r1

	/*############ Common data ########## */

	GET_CPU_TS	\r0
	ST_DI	\r0, [\r1, EVLOG_FIELD_TIME]

#ifdef CONFIG_SMP
	bic	\r0, sp, THREAD_SIZE-1	; sp & ~(THREAD_SIZE - 1)
	ld	\r0, [\r0, THREAD_INFO_TSK]	; thread_info->tsk
#else
	ld	\r0, [_current_task]
#endif
	ld	\r0, [\r0, TASK_PID]
	ST_DI	\r0, [\r1, EVLOG_FIELD_TASK]

	/* Type of event (Intr/Excp/Trap etc) */
	mov	\r0, \event_id
	ST_DI	\r0, [\r1, EVLOG_FIELD_EVENT_ID]

	ST_DI	sp, [\r1, EVLOG_FIELD_SP]

	lr	\r0, [eret]
	ST_DI	\r0, [\r1, EVLOG_FIELD_PC]

	lr	\r0, [efa]    ; EFA
	ST_DI	\r0, [\r1, EVLOG_FIELD_EFA]

	lr	\r0, [0x403]	; ECR
	ST_DI	\r0, [\r1, EVLOG_FIELD_CAUSE]

	lr	\r0, [erstatus]
	ST_DI	\r0, [\r1, EVLOG_FIELD_STATUS]

	;mov	\r0, 0    ; AUX_SP
	lr \r0, [0x468]    ; MMU_PID
	ST_DI	\r0, [\r1, EVLOG_FIELD_EXTRA]

#ifdef CONFIG_SMP
	GET_CPU_ID	\r0
	ST_DI		\r0, [\r1, EVLOG_FIELD_CPU]
#endif
.endm

.macro SNAP_EPILOGUE r0, r1

	/* increment timeline_ctr  with mode on max */
	LD_DI	\r0, [timeline_ctr]
	add	\r0, \r0, 1
	and	\r0, \r0, MAX_SNAPS_MASK
	ST_DI	\r0, [timeline_ctr]

	SNAP_UNLOCK	\r0
.endm

.macro TAKE_SNAP_EXCP_TLB r0, r1
	SNAP_PROLOGUE \r0, \r1, SNAP_TLB
	SNAP_EPILOGUE \r0, \r1
.endm

.macro TAKE_SNAP_TLB_REFILL r0, r1
	SNAP_PROLOGUE \r0, \r1, SNAP_TLB_FAST

	; OK to clobber r3 in Fast Path handler
	lr	r3, [ARC_REG_TLBPD1]
	ST_DI	r3, [\r1, EVLOG_FIELD_EXTRA]

	SNAP_EPILOGUE \r0, \r1
.endm

.macro TAKE_SNAP_SYSCALL r0, r1
	SNAP_PROLOGUE \r0, \r1, SNAP_TRAP_IN

	ST_DI	r8, [\r1, EVLOG_FIELD_CAUSE]	; syscall num

	SNAP_EPILOGUE \r0, \r1
.endm

.macro TAKE_SNAP_C_FROM_ASM event
	mov r0, \event
	mov r1, sp
	bl take_snap_regs
.endm

#endif	/* CONFIG_ARC_DBG_EVENT_TIMELINE */

#endif	/* __ASSEMBLY__ */

#endif

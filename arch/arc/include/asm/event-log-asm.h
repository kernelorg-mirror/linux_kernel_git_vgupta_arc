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

.macro TAKE_SNAP_EXCP_TLB r0, r1, miss_type
.endm

.macro TAKE_SNAP_SYSCALL r0, r1
.endm

.macro TAKE_SNAP_C_FROM_ASM type
.endm

#else /* CONFIG_ARC_DBG_EVENT_TIMELINE */

#include <asm/asm-offsets.h>

#ifdef CONFIG_ISA_ARCV2

.macro IRQ_SAVE r0, r1
	clri \r0
.endm

.macro IRQ_RESTORE r0
	seti \r0
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

#endif

.macro  SNAP_LOCK r0, r1
	IRQ_SAVE	\r0, \r1
	PUSH		\r0	; save flags on stack
.endm

.macro	SNAP_UNLOCK r0
	POP		\r0
	IRQ_RESTORE	\r0
.endm

.macro SNAP_PROLOGUE r0, r1, event_id

	PUSH	\r0
	PUSH	\r1

	SNAP_LOCK	\r0, \r1

	ld	\r1, [timeline_ctr]
	mpyu	\r1, \r1, EVLOG_RECORD_SZ
	add	\r1, timeline_log, \r1

	/*############ Common data ########## */

	/* TIMER1 count in timeline_log[timeline_ctr].time */
	lr	\r0, [0x100]
	st	\r0, [\r1, EVLOG_FIELD_TIME]

	/* current task ptr in timeline_log[timeline_ctr].task */
	ld	\r0, [_current_task]
	ld	\r0, [\r0, TASK_PID]
	st	\r0, [\r1, EVLOG_FIELD_TASK]

	/* Type of event (Intr/Excp/Trap etc) */
	mov	\r0, \event_id
	st	\r0, [\r1, EVLOG_FIELD_EVENT_ID]

	st	sp, [\r1, EVLOG_FIELD_SP]

	lr	\r0, [eret]
	st	\r0, [\r1, EVLOG_FIELD_PC]

	lr	\r0, [efa]    ; EFA
	st	\r0, [\r1, EVLOG_FIELD_EFA]

	lr	\r0, [0x403]	; ECR
	st	\r0, [\r1, EVLOG_FIELD_CAUSE]

	lr	\r0, [erstatus]
	st	\r0, [\r1, EVLOG_FIELD_STATUS]

	lr	\r0, [0xd]    ; AUX_SP
	st	\r0, [\r1, EVLOG_FIELD_EXTRA]
.endm


.macro SNAP_EPILOGUE r0, r1

	/* increment timeline_ctr  with mode on max */
	ld	\r0, [timeline_ctr]
	add	\r0, \r0, 1
	and	\r0, \r0, MAX_SNAPS_MASK
	st	\r0, [timeline_ctr]

	SNAP_UNLOCK	\r0

	/* Restore back orig scratch reg */
	POP	\r1
	POP	\r0
.endm

.macro TAKE_SNAP_EXCP_TLB r0, r1, miss_type
	SNAP_PROLOGUE \r0, \r1, \miss_type
	SNAP_EPILOGUE \r0, \r1
.endm

.macro TAKE_SNAP_SYSCALL r0, r1
	SNAP_PROLOGUE \r0, \r1, SNAP_TRAP_IN

	st	r8, [\r1, EVLOG_FIELD_CAUSE]	; syscall num

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

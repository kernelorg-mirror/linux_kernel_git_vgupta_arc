/*
 * Low Level Interrupts/Traps/Exceptions(non-TLB) Handling for ARC
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TBD:
 *  -remove the low level wrappers for execve/rt_sigreturn/sigaltstack
 *   needed before task_pt_regs was invented. This saves a branch per call
 *   and more importantly allows clean invocation of post-syscall ptrace hook
 * vineetg: May 2011
 *  -Userspace unaligned access emulation
 *
 * vineetg: Feb 2011 (ptrace low level code fixes)
 *  -traced syscall return code (r0) was not saved into pt_regs for restoring
 *   into user reg-file when traded task rets to user space.
 *  -syscalls needing arch-wrappers (mainly for passing sp as pt_regs)
 *   were not invoking post-syscall trace hook (jumping directly into
 *   ret_from_system_call)
 *
 * vineetg: Nov 2010:
 *  -Vector table jumps (@8 bytes) converted into branches (@4 bytes)
 *  -To maintain the slot size of 8 bytes/vector, added nop, which is
 *   not executed at runtime.
 *
 * vineetg: Nov 2009 (Everything needed for TIF_RESTORE_SIGMASK)
 *  -do_signal()invoked upon TIF_RESTORE_SIGMASK as well
 *  -Wrappers for sys_{,rt_}sigsuspend() nolonger needed as they don't
 *   need ptregs anymore
 *
 * Vineetg: Oct 2009
 *  -In a rare scenario, Process gets a Priv-V exception and gets scheduled
 *   out. Since we don't do FAKE RTIE for Priv-V, CPU excpetion state remains
 *   active (AE bit enabled).  This causes a double fault for a subseq valid
 *   exception. Thus FAKE RTIE needed in low level Priv-Violation handler.
 *   Instr Error could also cause similar scenario, so same there as well.
 *
 * Vineetg: July 2009
 *  -Removed CONFIG_ARC_USER_FAULTS_DEBUG (dumping regs in case of fatal
 *    signal in user faults return path) as it is already provided by
 *    /proc/sys/kernel/print-fatal-signals
 *
 * Vineetg: April 2009:
 *  -Check for Valid sys call number in EV_Trap was bogus
 *   sys calls > NR_syscalls was _NOT_ declared bad
 *   sys call  = NR_syscalls was _WRONGLY_ declared bad
 *
 * Vineetg: March 2009 : (Critical look at this file one more time)
 *  -Replaced and.f, nop, bz sequences with bbit0 type instruction
 *  -After returning from preempt_schedule_irq( ) we used to check for
 *   rescheuling again. That is already been done in the generic code
 *  -No need to maintain cpu_stat[].local_irq_count in ISR entry/exit
 *  -Removed the extra hops return_from_interrupt_level[1|2]
 *     while returning from Interrupts
 *  -waitpid no longer needed in syscall Table as uClibc implements it
 *   using wait4 syscall.
 *  -no need to save CALLEE Regs before execve
 *
 * Vineetg: March 2009 (Supporting 2 levels of Interrupts)
 *
 * Vineetg: Feb 2009
 *  -do_softirq( ) was being called twice on ISR return path
 *     First from irq_exit( ) in generic code
 *     Second in ret_from_interrupt_level[1 | 2]  => REMOVED THIS
 *
 * Vineetg: Sept 3rd 2008:
 *  -Added a INSPECT_ON_IRQ macro to be called from Low Level ISR to inspect
 *   a variable getting clobbered (during ISR or between 2 ISRs)
 *  -Don't need FAKE RTIE (re-enabling exceptions before entering Excpn hdlr)
 *   for Instruction Error and Priviledge Violation Exceptions
 *
 * Vineetg: Aug 28th 2008: Bug #94984
 *  -Zero Overhead Loop Context shd be cleared when entering IRQ/EXcp/Trap
 *   Normally CPU does this automatically, however when doing FAKE rtie,
 *   we need to explicitly do this. The problem in macros
 *   FAKE_RET_FROM_EXCPN and FAKE_RET_FROM_EXCPN_LOCK_IRQ was that this bit
 *   was being "CLEARED" rather then "SET". Since it is Loop INHIBIT Bit,
 *   setting it and not clearing it clears ZOL context
 *
 * Vineetg: June 10th 2008
 *   -We can now display all 26 Regs on SegV
 *    For that return from a critical User fault is handled differently from
 *    general return from exceptions (ret_from_user_faults)
 *   -Machine Check doesnt run recursively
 *
 * Vineetg: May 16th, 2008
 *  - r25 now contains the Current Task when in kernel
 *  - SysCall wrappers for exec/signal* needn't push/pop Callee saved Regs
 *  - SysCall wrappers for fork siblings neednot POP callee Regs
 *  - Kernel Stack Switching Code now reused as a macro
 *
 * Vineetg: Dec 22, 2007
 *    Minor Surgery of Low Level ISR to make it SMP safe
 *    - MMU_SCRATCH0 Reg used for freeing up r9 in Level 1 ISR
 *    - _current_task is made an array of NR_CPUS
 *    - Access of _current_task wrapped inside a macro so that if hardware
 *       team agrees for a dedicated reg, no other code is touched
 *
 * Amit Bhor, Rahul Trivedi, Kanika Nema, Sameer Dhavale : Codito Tech 2004
 */

/*------------------------------------------------------------------
 *    Function                            ABI
 *------------------------------------------------------------------
 *
 *  Arguments                           r0 - r7
 *  Caller Saved Registers              r0 - r12
 *  Callee Saved Registers              r13- r25
 *  Global Pointer (gp)                 r26
 *  Frame Pointer (fp)                  r27
 *  Stack Pointer (sp)                  r28
 *  Interrupt link register (ilink1)    r29
 *  Interrupt link register (ilink2)    r30
 *  Branch link register (blink)        r31
 *------------------------------------------------------------------
 */

	.cpu A7

;############################ Vector Table #################################

.macro VECTOR  lbl
#if 1   /* Just in case, build breaks */
	j   \lbl
#else
	b   \lbl
	nop
#endif
.endm

	.section .vector, "ax",@progbits
	.align 4

/* Each entry in the vector table must occupy 2 words. Since it is a jump
 * across sections (.vector to .text) we are gauranteed that 'j somewhere'
 * will use the 'j limm' form of the intrsuction as long as somewhere is in
 * a section other than .vector.
 */

; ********* Critical System Events **********************
VECTOR   res_service             ; 0x0, Restart Vector  (0x0)
VECTOR   mem_service             ; 0x8, Mem exception   (0x1)
VECTOR   instr_service           ; 0x10, Instrn Error   (0x2)

; ******************** Device ISRs **********************
#ifdef CONFIG_ARC_IRQ3_LV2
VECTOR   handle_interrupt_level2
#else
VECTOR   handle_interrupt_level1
#endif

VECTOR   handle_interrupt_level1

#ifdef CONFIG_ARC_IRQ5_LV2
VECTOR   handle_interrupt_level2
#else
VECTOR   handle_interrupt_level1
#endif

#ifdef CONFIG_ARC_IRQ6_LV2
VECTOR   handle_interrupt_level2
#else
VECTOR   handle_interrupt_level1
#endif

.rept   25
VECTOR   handle_interrupt_level1 ; Other devices
.endr

/* FOR ARC600: timer = 0x3, uart = 0x8, emac = 0x10 */

; ******************** Exceptions **********************
VECTOR   EV_MachineCheck         ; 0x100, Fatal Machine check   (0x20)
VECTOR   EV_TLBMissI             ; 0x108, Intruction TLB miss   (0x21)
VECTOR   EV_TLBMissD             ; 0x110, Data TLB miss         (0x22)
VECTOR   EV_TLBProtV             ; 0x118, Protection Violation  (0x23)
				 ;         or Misaligned Access
VECTOR   EV_PrivilegeV           ; 0x120, Privilege Violation   (0x24)
VECTOR   EV_Trap                 ; 0x128, Trap exception        (0x25)
VECTOR   EV_Extension            ; 0x130, Extn Intruction Excp  (0x26)

.rept   24
VECTOR   reserved                ; Reserved Exceptions
.endr

#include <linux/linkage.h>   /* ARC_{EXTRY,EXIT} */
#include <asm/entry.h>       /* SAVE_ALL_{INT1,INT2,TRAP...} */
#include <asm/errno.h>
#include <asm/arcregs.h>

#include <asm/event-log-asm.h>

;##################### Scratch Mem for IRQ stack switching #############

ARCFP_DATA int1_saved_reg
	.align 32
	.type   int1_saved_reg, @object
	.size   int1_saved_reg, 4
int1_saved_reg:
	.zero 4

/* Each Interrupt level needs it's own scratch */
#ifdef CONFIG_ARC_COMPACT_IRQ_LEVELS

ARCFP_DATA int2_saved_reg
	.type   int2_saved_reg, @object
	.size   int2_saved_reg, 4
int2_saved_reg:
	.zero 4

#endif

; ---------------------------------------------
	.section .text, "ax",@progbits

res_service:		; processor restart
	flag    0x1     ; not implemented
	nop
	nop

reserved:		; processor restart
	rtie            ; jump to processor initializations

;##################### Interrupt Handling ##############################

#ifdef CONFIG_ARC_COMPACT_IRQ_LEVELS
; ---------------------------------------------
;  Level 2 ISR: Can interrupt a Level 1 ISR
; ---------------------------------------------
ARC_ENTRY handle_interrupt_level2

	; TODO-vineetg for SMP this wont work
	; free up r9 as scratchpad
	st  r9, [@int2_saved_reg]

	;Which mode (user/kernel) was the system in when intr occured
	lr  r9, [status32_l2]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_INT2

	;------------------------------------------------------
	; if L2 IRQ interrupted a L1 ISR, disable preemption
	;------------------------------------------------------

	ld r9, [sp, PT_status32]        ; get statu32_l2 (saved in pt_regs)
	bbit0 r9, STATUS_A1_BIT, 1f     ; L1 not active when L2 IRQ, so normal

	; A1 is set in status32_l2
	; bump thread_info->preempt_count (Disable preemption)
	GET_CURR_THR_INFO_FROM_SP   r10
	ld      r9, [r10, THREAD_INFO_PREEMPT_COUNT]
	add     r9, r9, 1
	st      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

1:
	TAKE_SNAP_C_FROM_ASM  SNAP_INTR_IN2

	;------------------------------------------------------
	; setup params for Linux common ISR and invoke it
	;------------------------------------------------------
	lr  r0, [icause2]
	and r0, r0, 0x1f

	bl.d  @arch_do_IRQ
	mov r1, sp

	mov r8,0x2
	sr r8, [AUX_IRQ_LV12]       ; clear bit in Sticky Status Reg

	b   ret_from_exception

ARC_EXIT handle_interrupt_level2

#endif

; ---------------------------------------------
;  Level 1 ISR
; ---------------------------------------------
ARC_ENTRY handle_interrupt_level1

	/* free up r9 as scratchpad */
#ifdef CONFIG_SMP
	sr  r9, [ARC_REG_SCRATCH_DATA0]
#else
	st   r9, [@int1_saved_reg]
#endif

	;Which mode (user/kernel) was the system in when intr occured
	lr  r9, [status32_l1]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_INT1

	; snapshot routine takes care of disabling nested intr
	TAKE_SNAP_C_FROM_ASM  SNAP_INTR_IN

	lr  r0, [icause1]
	and r0, r0, 0x1f

	bl.d  @arch_do_IRQ
	mov r1, sp

	mov r8,0x1
	sr r8, [AUX_IRQ_LV12]       ; clear bit in Sticky Status Reg

	b   ret_from_exception
ARC_EXIT handle_interrupt_level1

;################### Non TLB Exception Handling #############################

; ---------------------------------------------
; Instruction Error Exception Handler
; ---------------------------------------------

ARC_ENTRY instr_service

	st  r9, [@ex_saved_reg1]

	TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	lr  r0, [ecr]
	lr  r1, [efa]

	mov r2, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_insterror_or_kprobe
	b   ret_from_exception
ARC_EXIT instr_service

; ---------------------------------------------
; Memory Error Exception Handler
; ---------------------------------------------

ARC_ENTRY mem_service

	st  r9, [@ex_saved_reg1]

	TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	lr  r0, [ecr]
	lr  r1, [efa]
	mov r2, sp
	bl  do_memory_error
	b   ret_from_exception
ARC_EXIT mem_service

; ---------------------------------------------
; Machine Check Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_MachineCheck

	st  r9, [@ex_saved_reg1]
	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	lr  r0, [ecr]
	lr  r1, [efa]
	mov r2, sp

	brne    r0, 0x200100, 1f
	bl      do_tlb_overlap_fault
	b       ret_from_exception

1:
	; DEAD END: can't do much, display Regs and HALT
	SAVE_CALLEE_SAVED_USER

	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp, [r10, THREAD_CALLEE_REG]

	j  do_machine_check_fault

ARC_EXIT EV_MachineCheck

; ---------------------------------------------
; Protection Violation Exception Handler
; ---------------------------------------------

ARC_ENTRY EV_TLBProtV

	; free up r9 as scratchpad
	st  r9, [@ex_saved_reg1]

	TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

	;Which mode (user/kernel) was the system in when Exception occured
	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	;---------(3) Save some more regs-----------------
	;  vineetg: Mar 6th: Random Seg Fault issue #1
	;  ecr and efa were not saved in case an Intr sneaks in
	;  after fake rtie
	;
	lr  r3, [ecr]
	lr  r4, [efa]

	; --------(4) Return from CPU Exception Mode ---------
	;  Fake a rtie, but rtie to next label
	;  That way, subsequently, do_page_fault ( ) executes in pure kernel
	;  mode with further Exceptions enabled

	FAKE_RET_FROM_EXCPN r9

	;------ (5) Type of Protection Violation? ----------
	;
	; ProtV Hardware Exception is triggered for Access Faults of 2 types
	;   -Access Violaton (WRITE to READ ONLY Page)
	;   -Unaligned Access (READ/WRITE on odd boundary)
	;
	; Handling for these 2 are completely difft in Linux
	;   -Access Violation may be legitimate as Linux implements COW using it.
	;   -Unaligned Access is always wrong and causes process to be terminated.
	;
	cmp r3, 0x230400    ; Misaligned data access ?
	beq 4f

	;========= (6a) Access Violation Processing ========
	cmp r3, 0x230100
	mov r1, 0x0              ; if LD exception ? write = 0
	mov.ne r1, 0x1           ; else write = 1

	mov r2, r4              ; faulting address
	mov r0, sp              ; pt_regs
	bl  do_page_fault
	b   ret_from_exception

	;========== (6b) Non aligned access ============
4:
	mov r0, r3              ; cause code
	mov r1, r4              ; faulting address
	mov r2, sp              ; pt_regs

#ifdef  CONFIG_ARC_MISALIGN_ACCESS
	SAVE_CALLEE_SAVED_USER
	mov r3, sp              ; callee_regs
#endif

	bl  do_misaligned_access

#ifdef  CONFIG_ARC_MISALIGN_ACCESS
	DISCARD_CALLEE_SAVED_USER
#endif

	b   ret_from_exception

ARC_EXIT EV_TLBProtV

; ---------------------------------------------
; Privilege Violation Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_PrivilegeV
	st  r9, [@ex_saved_reg1]

	TAKE_SNAP_ASM r8, r9, SNAP_EXCP_IN

	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	lr  r0, [ecr]
	lr  r1, [efa]
	mov r2, sp

	FAKE_RET_FROM_EXCPN r9

	bl  do_privilege_fault
	b   ret_from_exception
ARC_EXIT EV_PrivilegeV

; ---------------------------------------------
; Extension Instruction Exception Handler
; ---------------------------------------------
ARC_ENTRY EV_Extension
	st  r9, [@ex_saved_reg1]
	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_SYS

	lr  r0, [ecr]
	lr  r1, [efa]
	mov r2, sp
	bl  do_extension_fault
	b   ret_from_exception
ARC_EXIT EV_Extension

;######################### System Call Tracing #########################

tracesys:
	; save EFA in case tracer wants the PC of traced task
	; using ERET won't work since next-PC has already committed
	lr  r12, [efa]
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r11
	st  r12, [r11, THREAD_FAULT_ADDR]

	; PRE Sys Call Ptrace hook
	bl  @syscall_trace

	; Do the Sys Call as we normally would.
	; Validate the Sys Call number
	ld  r8, [sp, PT_r8]
	cmp     r8,  NR_syscalls
	mov.hi  r0, -ENOSYS
	bhi     tracesys_exit

	; Restore the sys-call args. Mere invocation of the hook abv could have
	; clobbered them (since they are in scratch regs). The tracer could also
	; have deliberately changed the args:  r0-r7 - Sys Calls Args
	; Note that r8 - Sys Call Num has already been restored above
	ld  r0, [sp, PT_r0]
	ld  r1, [sp, PT_r1]
	ld  r2, [sp, PT_r2]
	ld  r3, [sp, PT_r3]
	ld  r4, [sp, PT_r4]
	ld  r5, [sp, PT_r5]
	ld  r6, [sp, PT_r6]
	ld  r7, [sp, PT_r7]
	ld.as   r9, [sys_call_table, r8]
	jl      [r9]        ; Entry into Sys Call Handler

tracesys_exit:
	st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

	;POST Sys Call Ptrace Hook
	bl  @syscall_trace
	b   ret_from_exception ; NOT ret_from_system_call at is saves r0 which
	; we'd done before calling post hook above

;################### Break Point TRAP ##########################

	; ======= (5b) Trap is due to Break-Point =========

trap_with_param:

	;make sure orig_r8 is a positive value
	st  NR_syscalls + 2, [sp, PT_orig_r8]

	mov r0, r12
	lr  r1, [efa]
	mov r2, sp

	; Now that we have read EFA, its safe to do "fake" rtie
	;   and get out of CPU exception mode
	FAKE_RET_FROM_EXCPN r11

	; Save callee regs in case gdb wants to have a look
	; SP will grow up by size of CALLEE Reg-File
	; NOTE: clobbers r12
	SAVE_CALLEE_SAVED_USER

	; save location of saved Callee Regs @ thread_struct->pc
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp, [r10, THREAD_CALLEE_REG]

	; Call the trap handler
	bl  do_non_swi_trap

	; unwind stack to discard Callee saved Regs
	DISCARD_CALLEE_SAVED_USER

	b   ret_from_exception

;##################### Trap Handling ##############################
;
; EV_Trap caused by TRAP_S and TRAP0 instructions.
;------------------------------------------------------------------
;   (1) System Calls
;       :parameters in r0-r7.
;       :r8 has the system call number
;   (2) Break Points
;------------------------------------------------------------------

ARC_ENTRY EV_Trap

	; free up r9 as scratchpad
	st  r9, [@ex_saved_reg1]

	TAKE_SNAP_ASM r10, r9,  SNAP_TRAP_IN

	;Which mode (user/kernel) was the system in when intr occured
	lr  r9, [erstatus]

	SWITCH_TO_KERNEL_STK
	SAVE_ALL_TRAP

	;------- (4) What caused the Trap --------------
	lr     r12, [ecr]
	and.f  0, r12, ECR_PARAM_MASK
	bnz    trap_with_param

	; ======= (5a) Trap is due to System Call ========

	; Before doing anything, return from CPU Exception Mode
	FAKE_RET_FROM_EXCPN r11

	; Is this task being TRACED and Is Tracer interested in System Call tracing
	GET_CURR_THR_INFO_FROM_SP   r9
	ld  r10, [r9, THREAD_INFO_FLAGS]
	and.f   0, r10, _TIF_SYSCALL_TRACE

	; If yes, call the system call with the pre and post tracing hooks
	bnz tracesys  ; this never comes back

	;============ This is normal System Call case ==========
	; Sys-call num shd not exceed the total system calls avail
	cmp     r8,  NR_syscalls
	mov.hi  r0, -ENOSYS
	bhi     ret_from_system_call

	; Offset into the syscall_table and call handler
	ld.as   r9,[sys_call_table, r8]
	jl      [r9]        ; Entry into Sys Call Handler

	; fall through to ret_from_system_call

ARC_EXIT EV_Trap
ARC_ENTRY ret_from_system_call

	st  r0, [sp, PT_r0]     ; sys call return value in pt_regs

	; fall through yet again
	; NOTE: ret_from_exception must immediately follow ret_from_system_call

;################### Return from Intr/Excp ##########################

ARC_ENTRY ret_from_exception

	ld  r8, [sp, PT_status32]   ; returning to User/Kernel Mode

#ifdef CONFIG_PREEMPT

	bbit0  r8, STATUS_U_BIT, resume_kernel_mode

#else

	bbit0  r8, STATUS_U_BIT, restore_regs

#endif

	; else returning to user mode
resume_user_mode:

	; TODO: Fast PAth chk: Can we rush back to user mode?


	; --------- do we need to resched the current user task ----
	; check Thread_info->flags for need sched flag
	GET_CURR_THR_INFO_FROM_SP   r10
	ld  r9, [r10, THREAD_INFO_FLAGS]
	bbit0  r9, TIF_NEED_RESCHED, skip_user_resched

	; User Task Preemption
	bl  schedule
	b   ret_from_exception

skip_user_resched:
	; --------- check for signals/restore-sigmask ------------
	and.f  0, r9, (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK)
	bz  restore_regs

	; setup args for "C" routine, do_signal ( )

	mov r0, sp      ;  pass ptr to pt_regs

	; save CALLEE Regs.
	; (i) For core dump: Incase this signal causes one
	; (ii) If this is due to SIGTRAP/SIGSTOP, task is being traced thus
	;      tracer might call PEEKUSR for a callee reg
	;
	; NOTE: SP will grow up by size of CALLEE Reg-File
	; NOTE: macro clobbers r12
	SAVE_CALLEE_SAVED_USER

	; save location of saved Callee Regs @ thread_struct->pc
	GET_CURR_TASK_FIELD_PTR   TASK_THREAD, r10
	st  sp,[r10, THREAD_CALLEE_REG]

	bl  @do_signal

	; unwind stack to discard Callee saved Regs
	DISCARD_CALLEE_SAVED_USER

	;--------- All things done, go back to Userland ------

	b restore_regs


#ifdef CONFIG_PREEMPT

resume_kernel_mode:

	; check preempt_count
	GET_CURR_THR_INFO_FROM_SP   r10
	ld  r8, [r10, THREAD_INFO_PREEMPT_COUNT]

	; Preemption disabled, go back to whatever we were doing before
	brne  r8, 0, restore_regs

	; check if this task's NEED_RESCHED flag set
	ld  r9, [r10, THREAD_INFO_FLAGS]
	bbit0  r9, TIF_NEED_RESCHED, restore_regs

	; All set to invoke preemption.
	; NEED to make sure that further IRQs don't happen.
	lr  r9, [status32]
	and r9, r9, ~(STATUS_E1_MASK | STATUS_E2_MASK)
	flag r9

	; Invoke PREEMPTION
	bl      preempt_schedule_irq

#endif

	; fall through

	; Restore the saved sys context (common exit-path for EXCPN/IRQ/Trap)
	; IRQ shd definitely not happen between now and rtie

restore_regs :

	; Disable Interrupts first
	lr  r10, [status32]
	mov r9, r10 ;make a copy. Didn't they teach u to "save" things ?
	and r9, r9, ~(STATUS_E1_MASK | STATUS_E2_MASK)
	flag r9

#ifdef CONFIG_ARC_CURR_IN_REG
	; Restore User R25
	; Earlier this used to be only for returning to user mode
	; However with 2 levels of IRQ this can also happen even if
	; in kernel mode
	ld r9, [sp, PT_sp]
	brhs r9, VMALLOC_START, 8f
	RESTORE_USER_R25
8:
#endif

	; Restore REG File. In case multiple Events outstanding,
	; use the same priorty as rtie: EXCPN, L2 IRQ, L1 IRQ, None

	; if Returning from Exception
	bbit0  r10, STATUS_AE_BIT, not_exception
	RESTORE_ALL_SYS
	rtie

	; Not Exception so maybe Interrupts (Level 1 or 2)

not_exception:

#ifdef CONFIG_ARC_COMPACT_IRQ_LEVELS

	bbit0  r10, STATUS_A2_BIT, not_level2_interrupt

	;------------------------------------------------------------------
	; if L2 IRQ interrupted a L1 ISR,  we'd disbaled preemption earlier
	; so that sched doesnt move to new task, causing L1 to be delayed
	; undeterministically. Now that we've achieved that, lets reset
	; things to what they were, before returning from L2 context
	;----------------------------------------------------------------

	ld r9, [sp, PT_orig_r8]        ; get orig_r8 to make sure it is
	brne r9, -2, 149f              ; infact a L2 ISR ret path

	ld r9, [sp, PT_status32]        ; get statu32_l2 (saved in pt_regs)
	bbit0 r9, STATUS_A1_BIT, 149f     ; L1 not active when L2 IRQ, so normal

	; A1 is set in status32_l2
	; decrement thread_info->preempt_count (re-enable preemption)
	GET_CURR_THR_INFO_FROM_SP   r10
	ld      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

	; paranoid check, given A1 was active when A2 happened, preempt count
	; must not be 0 beccause we would have incremented it.
	; If this does happen we simply HALT as it means a BUG !!!
	cmp     r9, 0
	bnz     2f
	flag 1

2:
	sub     r9, r9, 1
	st      r9, [r10, THREAD_INFO_PREEMPT_COUNT]

149:
	TAKE_SNAP_C_FROM_ASM SNAP_INTR_OUT2
	;return from level 2
	RESTORE_ALL_INT2
debug_marker_l2:
	rtie

not_level2_interrupt:

#endif

	bbit0  r10, STATUS_A1_BIT, not_level1_interrupt

	;return from level 1
	TAKE_SNAP_C_FROM_ASM SNAP_INTR_OUT

	RESTORE_ALL_INT1
debug_marker_l1:
	rtie

not_level1_interrupt:

	;this case is for syscalls or Exceptions (with fake rtie)

#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
	ld r8, [sp, PT_orig_r8]
	cmp r8, NR_syscalls+1
	jeq  149f
	TAKE_SNAP_ASM r9, r10, SNAP_TRAP_OUT
	j 150f

149:
	TAKE_SNAP_ASM r9, r10, SNAP_EXCP_OUT
150:

#endif

	RESTORE_ALL_SYS
debug_marker_syscall:
	rtie

ARC_EXIT ret_from_exception

ARC_ENTRY ret_from_fork
	; when the forked child comes here from the __switch_to function
	; r0 has the last task pointer.
	; put last task in scheduler queue
	bl  @schedule_tail
	b @ret_from_exception
ARC_EXIT ret_from_fork

;################### Special Sys Call Wrappers ##########################

ARC_ENTRY sys_execve_wrapper
	; copy pointer to pt_regs as a parameter
	mov  r3, sp
	bl  @sys_execve

	GET_CURR_THR_INFO_FROM_SP   r9
	ld  r10, [r9, THREAD_INFO_FLAGS]
	and.f   0, r10, _TIF_SYSCALL_TRACE

	bz ret_from_system_call
	b  tracesys_exit
ARC_EXIT sys_execve_wrapper

ARC_ENTRY sys_fork_wrapper
	; copy pointer to pt_regs as a parameter
	mov  r0, sp
	SAVE_CALLEE_SAVED_USER
	bl  @sys_fork
	DISCARD_CALLEE_SAVED_USER

	GET_CURR_THR_INFO_FROM_SP   r9
	ld  r10, [r9, THREAD_INFO_FLAGS]
	and.f   0, r10, _TIF_SYSCALL_TRACE

	bz ret_from_system_call
	b  tracesys_exit
ARC_EXIT sys_fork_wrapper

ARC_ENTRY sys_vfork_wrapper
	; copy pointer to pt_regs as a parameter
	mov  r0, sp
	SAVE_CALLEE_SAVED_USER
	bl  @sys_vfork
	DISCARD_CALLEE_SAVED_USER

	GET_CURR_THR_INFO_FROM_SP   r9
	ld  r10, [r9, THREAD_INFO_FLAGS]
	and.f   0, r10, _TIF_SYSCALL_TRACE

	bz ret_from_system_call
	b  tracesys_exit
ARC_EXIT sys_vfork_wrapper

ARC_ENTRY sys_clone_wrapper
	; clone sys-call takes 2 mandatary args: @flags and @child-stack
	; and it can take 3 var-args, depending on flags
	; To keep sys_clone( ) signature constant, we assume all 5 args
	; and set the helper @prtegs in next free reg
	; this is cheap since our args are in regs, not on stack
	mov r5, sp  ; pt_regs
	SAVE_CALLEE_SAVED_USER
	bl  @sys_clone
	DISCARD_CALLEE_SAVED_USER

	GET_CURR_THR_INFO_FROM_SP   r9
	ld  r10, [r9, THREAD_INFO_FLAGS]
	and.f   0, r10, _TIF_SYSCALL_TRACE

	bz ret_from_system_call
	b  tracesys_exit
ARC_EXIT sys_clone_wrapper

;################### Sys Call Table ##########################

; The syscall table starts here...
.data
ARC_ENTRY sys_call_table
__syscall_start:
/*000*/ .long   @sys_ni_syscall
	.long   @sys_exit
	.long   @sys_fork_wrapper
	.long   @sys_read
	.long   @sys_write
/*005*/ .long   @sys_open
	.long   @sys_close
	.long   @sys_ni_syscall /* was sys_waitpid */
	.long   @sys_creat
	.long   @sys_link
/*010*/ .long   @sys_unlink
	.long   @sys_execve_wrapper
	.long   @sys_chdir
	.long   @sys_time /* used by libc4 */
	.long   @sys_mknod
/*015*/ .long   @sys_chmod
	.long   @sys_lchown16
	.long   @sys_ni_syscall /* was sys_break */
	.long   @sys_ni_syscall /* was sys_stat */
	.long   @sys_lseek
/*020*/ .long   @sys_getpid
	.long   @sys_mount
	.long   @sys_oldumount /* used by libc4 */
	.long   @sys_setuid16
	.long   @sys_getuid16
/*025*/ .long   @sys_stime
	.long   @sys_ptrace
	.long   @sys_alarm /* used by libc4 */
	.long   @sys_ni_syscall /* was sys_fstat */
	.long   @sys_pause
/*030*/ .long   @sys_utime /* used by libc4 */
	.long   @sys_ni_syscall /* was sys_stty */
	.long   @sys_ni_syscall /* was sys_getty */
	.long   @sys_access
	.long   @sys_nice
/*035*/ .long   @sys_ni_syscall /* was sys_ftime */
	.long   @sys_sync
	.long   @sys_kill
	.long   @sys_rename
	.long   @sys_mkdir
/*040*/ .long   @sys_rmdir
	.long   @sys_dup
	.long   @sys_pipe
	.long   @sys_times
	.long   @sys_ni_syscall /* was sys_prof */
/*045*/ .long   @sys_brk
	.long   @sys_setgid16
	.long   @sys_getgid16
	.long   @sys_ni_syscall /* was sys_signal */
	.long   @sys_geteuid16
/*050*/ .long   @sys_getegid16
	.long   @sys_acct
	.long   @sys_umount
	.long   @sys_ni_syscall /* was sys_lock */
	.long   @sys_ioctl
/*055*/ .long   @sys_fcntl
	.long   @sys_ni_syscall /* was sys_mpx */
	.long   @sys_setpgid
	.long   @sys_ni_syscall /* was sys_ulimit */
	.long   @sys_ni_syscall /* was sys_olduname */
/*060*/ .long   @sys_umask
	.long   @sys_chroot
	.long   @sys_ustat
	.long   @sys_dup2
	.long   @sys_getppid
/*065*/ .long   @sys_getpgrp
	.long   @sys_setsid
	.long   @sys_ni_syscall	/* was sys_sigaction */
	.long   @sys_ni_syscall /* was sys_sgetmask */
	.long   @sys_ni_syscall /* was sys_ssetmask */
/*070*/ .long   @sys_setreuid16
	.long   @sys_setregid16
	.long   @sys_ni_syscall	/* was sys_sigsuspend */
	.long   @sys_sigpending
	.long   @sys_sethostname
/*075*/ .long   @sys_setrlimit
	.long   @sys_old_getrlimit /* used by libc4 */
	.long   @sys_getrusage
	.long   @sys_gettimeofday
	.long   @sys_settimeofday
/*080*/ .long   @sys_getgroups16
	.long   @sys_setgroups16
	.long   @sys_ni_syscall	/* was old_select */
	.long   @sys_symlink
	.long   @sys_ni_syscall /* was sys_lstat */
/*085*/ .long   @sys_readlink
	.long   @sys_uselib
	.long   @sys_swapon
	.long   @sys_reboot
	.long   @sys_old_readdir /* used by libc4 */
/*090*/ .long   @@sys_ni_syscall  /* was old_mmap */
	.long   @sys_arc_munmap
	.long   @sys_truncate
	.long   @sys_ftruncate
	.long   @sys_fchmod
/*095*/ .long   @sys_fchown16
	.long   @sys_getpriority
	.long   @sys_setpriority
	.long   @sys_ni_syscall /* was sys_profil */
	.long   @sys_statfs
/*100*/ .long   @sys_fstatfs
	.long   @sys_ni_syscall
	.long   @sys_socketcall
	.long   @sys_syslog
	.long   @sys_setitimer
/*105*/ .long   @sys_getitimer
	.long   @sys_newstat
	.long   @sys_newlstat
	.long   @sys_newfstat
	.long   @sys_ni_syscall /* was sys_uname */
/*110*/ .long   @sys_ni_syscall /* was sys_iopl */
	.long   @sys_vhangup
	.long   @sys_ni_syscall
	.long   @sys_ni_syscall /* vm86old for i386 */
	.long   @sys_wait4
/*115*/ .long   @sys_swapoff
	.long   @sys_sysinfo
	.long   @sys_ipc
	.long   @sys_fsync
	.long   @sys_sigreturn
/*120*/ .long   @sys_clone_wrapper
	.long   @sys_setdomainname
	.long   @sys_newuname
	.long   @sys_cacheflush
	.long   @sys_adjtimex
/*125*/ .long   @sys_mprotect
	.long   @sys_sigprocmask
	.long   @sys_ni_syscall
	.long   @sys_init_module
	.long   @sys_delete_module
/*130*/ .long   @sys_ni_syscall
	.long   @sys_quotactl
	.long   @sys_getpgid
	.long   @sys_fchdir
	.long   @sys_bdflush
/*135*/ .long   @sys_sysfs
	.long   @sys_personality
	.long   @sys_ni_syscall
	.long   @sys_setfsuid16
	.long   @sys_setfsgid16
/*140*/ .long   @sys_llseek
	.long   @sys_getdents
	.long   @sys_select
	.long   @sys_flock
	.long   @sys_msync
/*145*/ .long   @sys_readv
	.long   @sys_writev
	.long   @sys_getsid
	.long   @sys_fdatasync
	.long   @sys_sysctl
/*150*/ .long   @sys_mlock
	.long   @sys_munlock
	.long   @sys_mlockall
	.long   @sys_munlockall
	.long   @sys_sched_setparam
/*155*/ .long   @sys_sched_getparam
	.long   @sys_sched_setscheduler
	.long   @sys_sched_getscheduler
	.long   @sys_sched_yield
	.long   @sys_sched_get_priority_max
/*160*/ .long   @sys_sched_get_priority_min
	.long   @sys_sched_rr_get_interval
	.long   @sys_nanosleep
	.long   @sys_mremap
	.long   @sys_setresuid16
/*165*/ .long   @sys_getresuid16
	.long   @sys_ni_syscall
	.long   @sys_ni_syscall
	.long   @sys_poll
	.long   @sys_ni_syscall
/*170*/ .long   @sys_setresgid16
	.long   @sys_getresgid16
	.long   @sys_prctl
	.long   @sys_rt_sigreturn
	.long   @sys_rt_sigaction
/*175*/ .long   @sys_rt_sigprocmask
	.long   @sys_rt_sigpending
	.long   @sys_rt_sigtimedwait
	.long   @sys_rt_sigqueueinfo
	.long   @sys_rt_sigsuspend
/*180*/ .long   @sys_pread64
	.long   @sys_pwrite64
	.long   @sys_chown16
	.long   @sys_getcwd
	.long   @sys_capget
/*185*/ .long   @sys_capset
	.long   @sys_sigaltstack
	.long   @sys_sendfile
	.long   @sys_ni_syscall
	.long   @sys_ni_syscall
/*190*/ .long   @sys_vfork_wrapper
	.long   @sys_getrlimit
	.long   @sys_mmap2
	.long   @sys_truncate64
	.long   @sys_ftruncate64
/*195*/ .long   @sys_stat64
	.long   @sys_lstat64
	.long   @sys_fstat64
	.long   @sys_lchown
	.long   @sys_getuid
/*200*/ .long   @sys_getgid
	.long   @sys_geteuid
	.long   @sys_getegid
	.long   @sys_setreuid
	.long   @sys_setregid
/*205*/ .long   @sys_getgroups
	.long   @sys_setgroups
	.long   @sys_fchown
	.long   @sys_setresuid
	.long   @sys_getresuid
/*210*/ .long   @sys_setresgid
	.long   @sys_getresgid
	.long   @sys_chown
	.long   @sys_setuid
	.long   @sys_setgid
/*215*/ .long   @sys_setfsuid
	.long   @sys_setfsgid
	.long   @sys_pivot_root
	.long   @sys_mincore
	.long   @sys_madvise
/*220*/ .long   @sys_getdents64
	.long   @sys_fcntl64
	.long   @sys_ni_syscall
	.long   @sys_ni_syscall
	.long   @sys_gettid
/*225*/ .long   @sys_lookup_dcookie
	.long   @sys_statfs64
	.long   @sys_ni_syscall /* was sys_waitpid */
	.long   @sys_mq_open
	.long   @sys_mq_unlink
/*230*/ .long   @sys_mq_timedreceive
	.long   @sys_mq_notify
	.long   @sys_mq_getsetattr
	.long   @sys_mq_timedsend
	.long   @sys_timer_create
/*235*/ .long   @sys_timer_settime
	.long   @sys_timer_gettime
	.long   @sys_timer_getoverrun
	.long   @sys_timer_delete
	.long   @sys_clock_settime
/*240*/ .long   @sys_clock_gettime
	.long   @sys_clock_getres
	.long   @sys_clock_nanosleep
	.long   @sys_sched_setaffinity
	.long   @sys_sched_getaffinity
/*245*/ .long   @sys_waitid
	.long   @sys_restart_syscall
	.long   @sys_pread64
	.long   @sys_pwrite64
	.long   @sys_mincore
/*250*/ .long   @sys_madvise
	.long   @sys_readahead
	.long   @sys_setxattr
	.long   @sys_lsetxattr
	.long   @sys_fsetxattr
/*255*/ .long   @sys_getxattr
	.long   @sys_lgetxattr
	.long   @sys_fgetxattr
	.long   @sys_listxattr
	.long   @sys_llistxattr
/*260*/ .long   @sys_flistxattr
	.long   @sys_removexattr
	.long   @sys_lremovexattr
	.long   @sys_fremovexattr
	.long   @sys_tkill
/*265*/ .long   @sys_sendfile64
	.long   @sys_futex
	.long   @sys_io_setup
	.long   @sys_io_destroy
	.long   @sys_io_getevents
/*270*/ .long   @sys_io_submit
	.long   @sys_io_cancel
	.long   @sys_ni_syscall /* was sys_fadvise64 */
	.long   @sys_exit_group
	.long   @sys_epoll_create
/*275*/ .long   @sys_epoll_ctl
	.long   @sys_epoll_wait
	.long   @sys_remap_file_pages
	.long   @sys_set_tid_address
	.long   @sys_fstatfs64
/*280*/ .long   @sys_tgkill
	.long   @sys_utimes
	.long   @sys_fadvise64_64
	.long   @sys_mbind
	.long   @sys_get_mempolicy
/*285*/ .long   @sys_set_mempolicy
	.long   @sys_kexec_load
	.long   @sys_add_key
	.long   @sys_request_key
	.long   @sys_keyctl
/*290*/ .long   @sys_ioprio_set
	.long   @sys_ioprio_get
	.long   @sys_inotify_init
	.long   @sys_inotify_add_watch
	.long   @sys_inotify_rm_watch
/*295*/ .long   @sys_migrate_pages
	.long   @sys_openat
	.long   @sys_mkdirat
	.long   @sys_mknodat
	.long   @sys_fchownat
/*300*/ .long   @sys_futimesat
	.long   @sys_fstatat64
	.long   @sys_unlinkat
	.long   @sys_renameat
	.long   @sys_linkat
/*305*/ .long   @sys_symlinkat
	.long   @sys_readlinkat
	.long   @sys_fchmodat
	.long   @sys_faccessat
	.long   @sys_pselect6
/*310*/ .long   @sys_ppoll
	.long   @sys_unshare
	.long   @sys_set_robust_list
	.long   @sys_get_robust_list
	.long   @sys_splice
/*315*/ .long   @sys_sync_file_range
	.long   @sys_tee
	.long   @sys_vmsplice
	.long   @sys_move_pages
	.long   @sys_getcpu
/*320*/ .long   @sys_epoll_pwait
	.long   @sys_utimensat
	.long   @sys_signalfd
	.long   @sys_timerfd_create
	.long   @sys_eventfd
/*325*/ .long   @sys_fallocate
	.long   @sys_timerfd_settime
	.long   @sys_timerfd_gettime
	.long   @sys_signalfd4
	.long   @sys_eventfd2
/*330*/ .long   @sys_epoll_create1
	.long   @sys_dup3
	.long   @sys_pipe2
	.long   @sys_inotify_init1
	.long   @sys_preadv
/*335*/ .long   @sys_pwritev
	.long   @sys_socket
	.long   @sys_bind
	.long   @sys_connect
	.long   @sys_listen
/*340*/ .long   @sys_accept
	.long   @sys_getsockname
	.long   @sys_getpeername
	.long   @sys_socketpair
	.long   @sys_send
/*345*/ .long   @sys_sendto
	.long   @sys_recv
	.long   @sys_recvfrom
	.long   @sys_shutdown
	.long   @sys_setsockopt
/*350*/ .long   @sys_getsockopt
	.long   @sys_sendmsg
	.long   @sys_recvmsg
	.long   @sys_arc_settls
	.long   @sys_arc_gettls
/*355*/ .long   @sys_perf_event_open
	.long   @sys_rt_tgsigqueueinfo
	.long   @sys_recvmmsg

__syscall_end:

	.rept   NR_syscalls - (__syscall_end - __syscall_start) / 4
	.long   @sys_ni_syscall
	.endr
ARC_EXIT sys_call_table

#ifdef CONFIG_ARC_DW2_UNWIND
; Workaround for bug 94179 (STAR ):
; Despite -fasynchronous-unwind-tables, linker is not making dwarf2 unwinder
; section (.debug_frame) as loadable. So we force it here.
; This also fixes STAR 9000487933 where the prev-workaround (objcopy --setflag)
; would not work after a clean build due to kernel build system dependencies.
.section .debug_frame, "wa",@progbits
#endif

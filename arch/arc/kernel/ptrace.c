/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  vineetg: Jan 2010
 *      -Enabling dynamic printk for fun
 *      -str to id conversion for ptrace requests and reg names
 *      -Consistently use offsets wrt "struct user" for all ptrace req
 *      -callee_reg accessed using accessor to implement alternate ways of
 *       fetcing it (say using dwarf unwinder)
 *      -Using generic_ptrace_(peek|poke)data
 *
 * Soam Vasani, Kanika Nema: Codito Technologies 2004
 */

#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/mm.h>
#include <linux/elf.h>
#include <linux/regset.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/tracehook.h>
#include <asm/setup.h>

char *get_str_from_id(int id, const struct id_to_str *tbl)
{
	int i;
	for (i = 0; tbl[i].str != NULL; i++) {
		if (id == tbl[i].id)
			return tbl[i].str;
	}

	return "???";
}

#define reg_nm(reg)	get_str_from_id(reg, ptrace_reg_nm)
#define req_nm(req)	get_str_from_id(req, ptrace_req_nm)

static const struct id_to_str ptrace_req_nm[] = {
	 {PTRACE_SYSCALL, "syscall"},
	 {PTRACE_PEEKTEXT, "peek-text"},
	 {PTRACE_PEEKDATA, "peek-data"},
	 {PTRACE_PEEKUSR, "peek-user"},
	 {PTRACE_POKETEXT, "poke-text"},
	 {PTRACE_POKEDATA, "poke-data"},
	 {PTRACE_POKEUSR, "poke-user"},
	 {PTRACE_CONT, "continue...."},
	 {PTRACE_KILL, "kill $#%#$$"},
	 {PTRACE_ATTACH, "attach"},
	 {PTRACE_DETACH, "detach"},
	 {PTRACE_SETOPTIONS, "set-option"},
	 {PTRACE_SETSIGINFO, "set-siginfo"},
	 {PTRACE_GETSIGINFO, "get-siginfo"},
	 {PTRACE_GETEVENTMSG, "get-event-msg"},
	 {0, NULL}
};

static const struct id_to_str ptrace_reg_nm[] = {
	 {0, "res1"},
	 {4, "BTA"},
	 {8, "LPS"},
	 {12, "LPE"},
	 {16, "LPC"},
	 {20, "STATUS3"},
	 {24, "PC"},
	 {28, "BLINK"},
	 {32, "FP"},
	 {36, "GP"},
	 {40, "r12"},
	 {44, "r11"},
	 {48, "r10"},
	 {52, "r9"},
	 {56, "r8"},
	 {60, "r7"},
	 {64, "r6"},
	 {68, "r5"},
	 {72, "r4"},
	 {76, "r3"},
	 {80, "r2"},
	 {84, "r1"},
	 {88, "r0"},
	 {92, "ORIG r0 (sys-call only)"},
	 {96, "ORIG r8 (event type)"},
	 {100, "SP"},
	 {104, "res2"},
	 {108, "r25"},
	 {112, "r24"},
	 {116, "r23"},
	 {120, "r22"},
	 {124, "r21"},
	 {128, "r20"},
	 {132, "r19"},
	 {136, "r18"},
	 {140, "r17"},
	 {144, "r16"},
	 {148, "r15"},
	 {152, "r14"},
	 {156, "r13"},
	 {160, "(EFA) Break Pt addr"},
	 {164, "STOP PC"},
	 {0, NULL}
};

static struct callee_regs *task_callee_regs(struct task_struct *tsk)
{
	struct callee_regs *tmp = (struct callee_regs *)tsk->thread.callee_reg;
	return tmp;
}

asmlinkage int syscall_trace_entry(struct pt_regs *regs)
{
	if (tracehook_report_syscall_entry(regs))
		return ULONG_MAX;

	return regs->r8;
}

asmlinkage void syscall_trace_exit(struct pt_regs *regs)
{
	tracehook_report_syscall_exit(regs, 0);
}

static int genregs_get(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       void *kbuf, void __user *ubuf)
{
	const struct pt_regs *ptregs = task_pt_regs(target);
	const struct callee_regs *cregs = task_callee_regs(target);
	int ret = 0;
	unsigned int stop_pc_val;

#define REG_O_CHUNK(START, END, PTR)	\
	if (!ret)	\
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, PTR, \
			offsetof(struct user_regs_struct, START), \
			offsetof(struct user_regs_struct, END));

#define REG_O_ONE(LOC, PTR)	\
	if (!ret)		\
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf, PTR, \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

	REG_O_CHUNK(scratch, callee, ptregs);
	REG_O_CHUNK(callee, efa, cregs);
	REG_O_CHUNK(efa, stop_pc, &target->thread.fault_address);

	if (!ret) {
		if (in_brkpt_trap(ptregs)) {
			stop_pc_val = target->thread.fault_address;
			pr_debug("\t\tstop_pc (brk-pt)\n");
		} else {
			stop_pc_val = ptregs->ret;
			pr_debug("\t\tstop_pc (others)\n");
		}

		REG_O_ONE(stop_pc, &stop_pc_val);
	}

	return ret;
}

static int genregs_set(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       const void *kbuf, const void __user *ubuf)
{
	const struct pt_regs *ptregs = task_pt_regs(target);
	const struct callee_regs *cregs = task_callee_regs(target);
	int ret = 0;

#define REG_IN_CHUNK(FIRST, NEXT, PTR)	\
	if (!ret)			\
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, \
			(void *)(PTR), \
			offsetof(struct user_regs_struct, FIRST), \
			offsetof(struct user_regs_struct, NEXT));

#define REG_IN_ONE(LOC, PTR)		\
	if (!ret)			\
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf, \
			(void *)(PTR), \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

#define REG_IGNORE_ONE(LOC)		\
	if (!ret)			\
		ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf, \
			offsetof(struct user_regs_struct, LOC), \
			offsetof(struct user_regs_struct, LOC) + 4);

	/* TBD: disallow updates to STATUS32, orig_r8 etc*/
	REG_IN_CHUNK(scratch, callee, ptregs);	/* pt_regs[bta..orig_r8] */
	REG_IN_CHUNK(callee, efa, cregs);	/* callee_regs[r25..r13] */
	REG_IGNORE_ONE(efa);			/* efa update invalid */
	REG_IN_ONE(stop_pc, &ptregs->ret);	/* stop_pc: PC update */

	return ret;
}

enum arc_getset {
	REGSET_GENERAL,
};

static const struct user_regset arc_regsets[] = {
	[REGSET_GENERAL] = {
	       .core_note_type = NT_PRSTATUS,
	       .n = ELF_NGREG,
	       .size = sizeof(unsigned long),
	       .align = sizeof(unsigned long),
	       .get = genregs_get,
	       .set = genregs_set,
	}
};

static const struct user_regset_view user_arc_view = {
	.name		= UTS_MACHINE,
	.e_machine	= EM_ARCOMPACT,
	.regsets	= arc_regsets,
	.n		= ARRAY_SIZE(arc_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_arc_view;
}

void ptrace_disable(struct task_struct *child)
{
}

long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret = -EIO;
	unsigned int count, pos;
	unsigned int __user *u_addr;
	void *kbuf;

	pr_debug("REQ=%ld (%s), ADDR =0x%lx, DATA=0x%lx)\n",
		    request, req_nm(request), addr, data);

	switch (request) {

	case PTRACE_PEEKUSR:
		pos = addr;	/* offset in struct user_regs_struct */
		count = 4;	/* 1 register only */
		u_addr = (unsigned int __user *)data;
		kbuf = NULL;
		ret = genregs_get(child, NULL, pos, count, kbuf, u_addr);
		break;

	case PTRACE_POKEUSR:
		pos = addr;	/* offset in struct user_regs_struct */
		count = 4;	/* 1 register only */

		/* Ideally @data would have abeen a user space buffer, from
		 * where, we do a copy_from_user.
		 * However this request only involves one word, which courtesy
		 * our ABI can be passed in a reg.
		 * regset interface however expects some buffer to copyin from
		 */
		kbuf = &data;
		u_addr = NULL;

		ret = genregs_set(child, NULL, pos, count, kbuf, u_addr);
		break;

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}

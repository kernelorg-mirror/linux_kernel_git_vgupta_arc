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
	 {PTRACE_GETREGS, "getreg"},
	 {PTRACE_SETREGS, "setreg"},
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

static struct callee_regs *task_callee_regs(struct task_struct *tsk,
					    struct callee_regs *calleereg)
{
	struct callee_regs *tmp = (struct callee_regs *)tsk->thread.callee_reg;
	return tmp;
}

void ptrace_disable(struct task_struct *child)
{
	/* DUMMY: dummy function to resolve undefined references */
}

unsigned long getreg(const unsigned int off, struct task_struct *child)
{
	unsigned int *reg_file;
	struct pt_regs *ptregs = task_pt_regs(child);
	unsigned int data = 0;

	if (off < offsetof(struct user_regs_struct, callee)) {
		reg_file = (unsigned int *)ptregs;
		data = reg_file[off / 4];
	} else if (off < offsetof(struct user_regs_struct, efa)) {
		struct callee_regs calleeregs;
		reg_file = (unsigned int *)task_callee_regs(child, &calleeregs);
		data =
		    reg_file[(off -
			      offsetof(struct user_regs_struct, callee)) / 4];
	} else if (off == offsetof(struct user_regs_struct, efa)) {
		data = child->thread.fault_address;
	} else if (off == offsetof(struct user_regs_struct, stop_pc)) {
		if (in_brkpt_trap(ptregs)) {
			data = child->thread.fault_address;
			pr_debug("\t\tstop_pc (brk-pt)\n");
		} else {
			data = ptregs->ret;
			pr_debug("\t\tstop_pc (others)\n");
		}
	}

	pr_debug("\t\t%s:0x%2x (%s) = %x\n", __func__, off, reg_nm(off),
		data);

	return data;
}

void setreg(unsigned int off, unsigned int data, struct task_struct *child)
{
	unsigned int *reg_file;
	struct pt_regs *ptregs = task_pt_regs(child);

	pr_debug("\t\t%s:0x%2x (%s) = 0x%x\n", __func__, off, reg_nm(off),
		data);

	if (off == offsetof(struct user_regs_struct, scratch.res) ||
	    off == offsetof(struct user_regs_struct, callee.res) ||
	    off == offsetof(struct user_regs_struct, efa)) {
		pr_warn("Bogus ptrace setreg request\n");
		return;
	}

	if (off == offsetof(struct user_regs_struct, stop_pc)) {
		pr_debug("\t\tstop_pc (others)\n");
		ptregs->ret = data;
	} else if (off < offsetof(struct user_regs_struct, callee)) {
		reg_file = (unsigned int *)ptregs;
		reg_file[off / 4] = data;
	} else if (off < offsetof(struct user_regs_struct, efa)) {
		struct callee_regs calleeregs;
		reg_file = (unsigned int *)task_callee_regs(child, &calleeregs);
		off -= offsetof(struct user_regs_struct, callee);
		reg_file[off / 4] = data;
	} else {
		pr_warn("Bogus ptrace setreg request\n");
	}
}

/*
 * As a User API, @data is payload of API and it's interpretation depends
 * on the @request. However most of the getxxx requests, it has the user
 * address where data needs to be copied to.
 */
long arch_ptrace(struct task_struct *child, long request,
		 unsigned long addr, unsigned long data)
{
	int ret;
	int i;
	unsigned long tmp;
	unsigned int __user *u_addr = (unsigned int __user *)data;

	if (!(request == PTRACE_PEEKTEXT || request == PTRACE_PEEKDATA ||
	      request == PTRACE_PEEKUSR)) {
		pr_debug("REQ=%ld (%s), ADDR =0x%lx, DATA=0x%lx)\n",
		    request, req_nm(request), addr, data);
	}

	switch (request) {

	case PTRACE_GETREGS:
		for (i = 0; i < sizeof(struct user_regs_struct) / 4; i++) {
			/* getreg wants a byte offset */
			tmp = getreg(i << 2, child);
			ret = put_user(tmp, u_addr + i);
			if (ret < 0)
				goto out;
		}
		break;

	case PTRACE_SETREGS:
		for (i = 0; i < sizeof(struct user_regs_struct) / 4; i++) {
			ret = get_user(tmp, (u_addr + i));
			if (ret < 0)
				goto out;
			setreg(i << 2, tmp, child);
		}

		ret = 0;
		break;

	/*
	 * U-AREA Read (Registers, signal etc)
	 * From offset @addr in @child's struct user into location @data
	 */
	case PTRACE_PEEKUSR:
		/* user regs */
		if (addr > (unsigned)offsetof(struct user, regs) &&
		    addr < (unsigned)offsetof(struct user, regs) +
			    sizeof(struct user_regs_struct)) {
			pr_debug("\tPeek-usr\n");
			tmp = getreg(addr, child);
			ret = put_user(tmp, u_addr);

		} else if (addr == (unsigned)offsetof(struct user, signal)) {
			/* set by syscall_trace */
			tmp = current->exit_code;
			ret = put_user(tmp, u_addr);

		} else if (addr > 0 && addr < sizeof(struct user)) {
			return -EIO; /* nothing else is interesting yet */
		} else {
			return -EIO; /* out of bounds */
		}

		break;

	case PTRACE_POKEUSR:
		ret = 0;
		if (addr == (int)offsetof(struct user_regs_struct, efa))
			return -EIO;

		if (addr > (unsigned)offsetof(struct user, regs) &&
		    addr < (unsigned)offsetof(struct user, regs) +
			    sizeof(struct user_regs_struct))
			setreg(addr, data, child);
		else
			return -EIO;

		break;

	/*
	 * Single step not supported.  ARC's single step bit won't
	 * work for us.  It is in the DEBUG register, which the RTI
	 * instruction does not restore.
	 */
	case PTRACE_SINGLESTEP:

	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}

out:
	return ret;

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

static int arc_regs_get(struct task_struct *target,
			const struct user_regset *regset,
			unsigned int pos, unsigned int count,
			void *kbuf, void __user *ubuf)
{
	pr_debug("arc_regs_get %p %d %d\n", target, pos, count);

	if (kbuf) {
		unsigned long *k = kbuf;
		while (count > 0) {
			*k++ = getreg(pos, target);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		unsigned long __user *u = ubuf;
		while (count > 0) {
			if (__put_user(getreg(pos, target), u++))
				return -EFAULT;
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}

	return 0;
}

static int arc_regs_set(struct task_struct *target,
			const struct user_regset *regset,
			unsigned int pos, unsigned int count,
			const void *kbuf, const void __user *ubuf)
{
	int ret = 0;

	pr_debug("arc_regs_set %p %d %d\n", target, pos, count);

	if (kbuf) {
		const unsigned long *k = kbuf;
		while (count > 0) {
			setreg(pos, *k++, target);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		const unsigned long __user *u = ubuf;
		unsigned long word;
		while (count > 0) {
			ret = __get_user(word, u++);
			if (ret)
				return ret;
			setreg(pos, word, target);
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}

	return 0;
}

static const struct user_regset arc_regsets[] = {
	[0] = {
	       .core_note_type = NT_PRSTATUS,
	       .n = ELF_NGREG,
	       .size = sizeof(long),
	       .align = sizeof(long),
	       .get = arc_regs_get,
	       .set = arc_regs_set}
};

static const struct user_regset_view user_arc_view = {
	.name = UTS_MACHINE,
	.e_machine = EM_ARCOMPACT,
	.regsets = arc_regsets,
	.n = ARRAY_SIZE(arc_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_arc_view;
}

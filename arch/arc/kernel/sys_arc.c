/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Vineetg: July 2009
 *    -kernel_execve inline asm optimised to prevent un-needed reg shuffling
 *     Now have 6 instructions in  generated code as opposed to 16.
 */

#include <linux/errno.h>
#include <linux/msg.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>

struct sel_arg_struct {
	unsigned long n;
	fd_set *inp, *outp, *exp;
	struct timeval *tvp;
};

asmlinkage int old_select(struct sel_arg_struct *arg)
{
	struct sel_arg_struct a;

	if (copy_from_user(&a, arg, sizeof(a)))
		return -EFAULT;
	/* sys_select() does the appropriate kernel locking */
	return sys_select(a.n, a.inp, a.outp, a.exp, a.tvp);
}

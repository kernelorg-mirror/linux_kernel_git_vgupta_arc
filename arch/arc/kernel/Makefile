#
# Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

obj-$(CONFIG_MODULES)   += arcksyms.o module.o

# Pass UTS_MACHINE for user_regset definition
CFLAGS_ptrace.o		+= -DUTS_MACHINE='"$(UTS_MACHINE)"'

obj-y		+= arcksyms.o ptrace.o sys_arc.o setup.o irq.o time.o
obj-y		+= troubleshoot.o stacktrace.o event-log.o
obj-$(CONFIG_ARCH_ARC_HLINK)		+= arc_hostlink.o
ifdef CONFIG_SMP
obj-y		+= atomic_lock.o bitops_lock.o
endif

ifdef CONFIG_ARC_STACK_UNWIND
obj-y		+= unwind.o
endif

ifdef CONFIG_KPROBES
obj-y       += kprobes.o
endif

CFLAGS_fpu.o   += -mdpfp
obj-$(CONFIG_ARCH_ARC_FPU) += fpu.o

obj-$(CONFIG_ARC_MISAILGNED_ACCESS) += unaligned.o

extra-y := init_task.o vmlinux.lds

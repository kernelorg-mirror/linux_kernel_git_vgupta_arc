#
# Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

# Pass UTS_MACHINE for user_regset definition
CFLAGS_ptrace.o		+= -DUTS_MACHINE='"$(UTS_MACHINE)"'

obj-y	+= arcksyms.o ptrace.o sys_arc.o setup.o irq.o time.o reset.o
obj-y	+= troubleshoot.o stacktrace.o

obj-$(CONFIG_ARC_DBG_EVENT_TIMELINE)	+= event-log.o
obj-$(CONFIG_MODULES)			+= arcksyms.o module.o
obj-$(CONFIG_ARCH_ARC_HLINK)		+= arc_hostlink.o
obj-$(CONFIG_SMP) 			+= atomic_lock.o bitops_lock.o
obj-$(CONFIG_GENERIC_CLOCKEVENTS) 	+= time.o
obj-$(CONFIG_ARC_STACK_UNWIND)		+= unwind.o
obj-$(CONFIG_KPROBES)      		+= kprobes.o
obj-$(CONFIG_ARC_MISAILGNED_ACCESS) 	+= unaligned.o

CFLAGS_fpu.o   += -mdpfp
obj-$(CONFIG_ARCH_ARC_FPU) 		+= fpu.o

extra-y := init_task.o vmlinux.lds

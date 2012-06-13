/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_SWITCH_TO_H
#define _ASM_ARC_SWITCH_TO_H

#ifndef __ASSEMBLY__

struct task_struct;		/* to prevent cyclic dependencies */

/* switch_to macro based on the ARM implementaion */
extern struct task_struct *__switch_to(struct task_struct *prev,
				       struct task_struct *next);

#ifdef CONFIG_ARC_FPU_SAVE_RESTORE

#define HW_BUG_101581

#ifdef HW_BUG_101581

extern void fpu_save_restore(struct task_struct *prev,
			     struct task_struct *next);

#define ARC_FPU_PREV(p, n)	fpu_save_restore(p, n)
#define ARC_FPU_NEXT(t)

#else /* !HW_BUG_101581 */

extern inline void fpu_save(struct task_struct *tsk);
extern inline void fpu_restore(struct task_struct *tsk);

#define ARC_FPU_PREV(p, n)	fpu_save(p)
#define ARC_FPU_NEXT(t)		fpu_restore(t)

#endif /* !HW_BUG_101581 */

#else /* !CONFIG_ARC_FPU_SAVE_RESTORE */

#define ARC_FPU_PREV(p, n)
#define ARC_FPU_NEXT(n)

#endif /* !CONFIG_ARC_FPU_SAVE_RESTORE */

#define switch_to(prev, next, last)	\
do {					\
	ARC_FPU_PREV(prev, next);	\
	last = __switch_to(prev, next);\
	ARC_FPU_NEXT(next);		\
	mb();				\
} while (0);

/* Hook into Schedular to be invoked prior to Context Switch
 *  -If ARC H/W profiling enabled it does some stuff
 *  -If event logging enabled it takes a event snapshot
 *
 *  Having a funtion would have been cleaner but to get the correct caller
 *  (from __builtin_return_address) it needs to be inline
 */

/* Things to do for event logging prior to Context switch */
#ifdef CONFIG_ARC_DBG_EVENT_TIMELINE
#define PREP_ARCH_SWITCH_ACT1(next)					\
do {									\
	if (next->mm)							\
		take_snap(SNAP_PRE_CTXSW_2_U,				\
			 (unsigned int) __builtin_return_address(0),	\
			 current_thread_info()->preempt_count);		\
	else								\
		take_snap(SNAP_PRE_CTXSW_2_K,				\
			 (unsigned int) __builtin_return_address(0),	\
			 current_thread_info()->preempt_count);		\
}									\
while (0)
#else
#define PREP_ARCH_SWITCH_ACT1(next)
#endif

/* This def is the one used by schedular */
#define prepare_arch_switch(next)		\
do {						\
	PREP_ARCH_SWITCH_ACT1(next);		\
} while (0)


#endif

#endif

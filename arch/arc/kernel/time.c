/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * vineetg: Jan 1011
 *  -sched_clock( ) no longer jiffies based. Uses the same clocksource
 *   as gtod
 *
 * Rajeshwarr/Vineetg: Mar 2008
 *  -Implemented CONFIG_GENERIC_TIME (rather deleted arch specific code)
 *   for arch independent gettimeofday()
 *  -Implemented CONFIG_GENERIC_CLOCKEVENTS as base for hrtimers
 *
 * Vineetg: Mar 2008: Forked off from time.c which now is time-jiff.c
 */

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/timex.h>
#include <linux/profile.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <asm/irq.h>
#include <asm/arcregs.h>

/* ARC700 has two 32bit independent prog Timers: TIMER0 and TIMER1
 * Each can programmed to go from @count to @limit and optionally
 * interrupt when that happens
 *
 * We've designated TIMER0 for events (clockevents)
 * while TIMER1 for free running (clocksource)
 */
#define ARC_TIMER_MAX	0xFFFFFFFF

/******************************************************************
 * Hardware Interface routines to program the ARC TIMERs
 ******************************************************************/

/*
 * Arm the timer to interrupt after @limit cycles
 */
static void arc_timer0_setup_event(unsigned int limit)
{
	/* setup start and end markers */
	write_aux_reg(ARC_REG_TIMER0_LIMIT, limit);
	write_aux_reg(ARC_REG_TIMER0_CNT, 0);	/* start from 0 */

	/* IE: Interrupt on count = limit,
	 * NH: Count cycles only when CPU running (NOT Halted)
	 */
	write_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE | TIMER_CTRL_NH);
}

/*
 * Acknowledge the interrupt & enable/disable the interrupt
 */
static void arc_timer0_ack_event(unsigned int irq_reenable)
{
	/* 1. Ack the interrupt by writing to CTRL reg.
	 *    Any write will cause intr to be ack, however it has to be one of
	 *    writable bits (NH: Count when not halted)
	 * 2. If required by caller, re-arm timer to Interrupt at the end of
	 *    next cycle.
	 *
	 * Small optimisation:
	 * Normal code would have been
	 *  if (irq_reenable) CTRL_REG = (IE | NH); else CTRL_REG = NH;
	 * However since IE is BIT0 we can fold the branch
	 */
	write_aux_reg(ARC_REG_TIMER0_CTRL, irq_reenable | TIMER_CTRL_NH);
}

/*
 * Arm the timer to keep counting monotonically
 */
static void arc_timer1_setup_free_flow(unsigned int limit)
{
	/* although for free flowing case, limit would alway be max 32 bits
	 * still we've kept the interface open, just in case ...
	 */
	write_aux_reg(ARC_REG_TIMER1_LIMIT, limit);
	write_aux_reg(ARC_REG_TIMER1_CNT, 0);
	write_aux_reg(ARC_REG_TIMER1_CTRL, TIMER_CTRL_NH);
}

/********** Clock Source Device *********/

static cycle_t cycle_read_t1(struct clocksource *cs)
{
	return (cycle_t) read_aux_reg(ARC_REG_TIMER1_CNT);
}

static struct clocksource clocksource_t1 = {
	.name = "ARC Timer1",
	.rating = 300,
	.read = cycle_read_t1,
	.mask = CLOCKSOURCE_MASK(32),
	.flags = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init arc_clocksource_init(void)
{
	arc_timer1_setup_free_flow(ARC_TIMER_MAX);

	/*
	 * CLK upto 4.29 GHz can be safely represented in 32 bits because
	 * Max 32 bit number is 4,294,967,295
	 */
	clocksource_register_hz(&clocksource_t1, CONFIG_ARC_PLAT_CLK);
}

/********** Clock Event Device *********/

static int arc_next_event(unsigned long delta, struct clock_event_device *dev)
{
	arc_timer0_setup_event(delta);
	return 0;
}

static void arc_set_mode(enum clock_event_mode mode,
			 struct clock_event_device *dev)
{
	pr_info("clockevent mode switch to [%d]\n", mode);
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		arc_timer0_setup_event(CONFIG_ARC_PLAT_CLK / HZ);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	default:
		break;
	}

	return;
}

struct clock_event_device arc_clockevent_device = {
	.name		= "ARC Timer0",
	.features	= CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC,
	.mode		= CLOCK_EVT_MODE_UNUSED,
	.rating		= 300,
	.irq		= TIMER0_INT,
	.set_next_event = arc_next_event,
	.set_mode 	= arc_set_mode,
};

static irqreturn_t timer_irq_handler(int irq, void *dev_id);

static struct irqaction arc_timer_irq = {
	.name = "ARC Timer0",
	.flags = IRQF_TIMER | IRQF_DISABLED,
	.handler = timer_irq_handler,
	.dev_id = &arc_clockevent_device,
};

irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	arc_timer0_ack_event(evt->mode == CLOCK_EVT_MODE_PERIODIC);
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static void __cpuinit arc_clockevent_init(void)
{
	struct clock_event_device *evt = &arc_clockevent_device;

	clockevents_calc_mult_shift(evt, CONFIG_ARC_PLAT_CLK, 5);

	evt->max_delta_ns = clockevent_delta2ns(ARC_TIMER_MAX, evt);
	evt->cpumask = cpumask_of(0);

	clockevents_register_device(evt);

	setup_irq(TIMER0_INT, &arc_timer_irq);
}

void __init time_init(void)
{
	arc_clocksource_init();
	arc_clockevent_init();
}

static int arc_finished_booting;

/*
 * Scheduler clock - returns current time in nanosec units.
 * It's return value must NOT wrap around.
 *
 * Although the return value is nanosec units based, what's more important
 * is whats the "source" of this value. The orig jiffies based computation
 * was only as granular as jiffies itself (10ms on ARC).
 * We need something that is more granular, so use the same mechanism as
 * gettimeofday(), which uses ARC Timer T1 wrapped as a clocksource.
 * Unfortunately the first call to sched_clock( ) is way before that subsys
 * is initialiased, thus use the jiffies based value in the interim.
 */
unsigned long long sched_clock(void)
{
	if (!arc_finished_booting) {
		return (unsigned long long)(jiffies - INITIAL_JIFFIES)
		    * (NSEC_PER_SEC / HZ);
	} else {
		struct timespec ts;
		getrawmonotonic(&ts);
		return (unsigned long long)timespec_to_ns(&ts);
	}
}

static int __init arc_clocksource_done_booting(void)
{
	arc_finished_booting = 1;
	return 0;
}

fs_initcall(arc_clocksource_done_booting);

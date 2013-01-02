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

/* ARC700 has two 32bit independent prog Timers: TIMER0 and TIMER1
 * Each can programmed to go from @count to @limit and optionally
 * interrupt when that happens.
 * A write to Control Register clears the Interrupt
 *
 * We've designated TIMER0 for events (clockevents)
 * while TIMER1 for free running (clocksource)
 *
 * Newer ARC700 cores have 64bit clk fetching RTSC insn, preferred over TIMER1
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

#define ARC_TIMER_MAX	0xFFFFFFFF

/********** Clock Source Device *********/

/*
 * set 32bit TIMER1 to keep counting monotonically and wraparound
 */
void __cpuinit arc_counter_setup(void)
{
	write_aux_reg(ARC_REG_TIMER1_LIMIT, ARC_TIMER_MAX);
	write_aux_reg(ARC_REG_TIMER1_CNT, 0);
	write_aux_reg(ARC_REG_TIMER1_CTRL, TIMER_CTRL_NH);
}

static cycle_t arc_counter_read(struct clocksource *cs)
{
	return (cycle_t) read_aux_reg(ARC_REG_TIMER1_CNT);
}

static struct clocksource arc_counter = {
	.name   = "ARC Timer1",
	.rating = 300,
	.read   = arc_counter_read,
	.mask   = CLOCKSOURCE_MASK(32),
	.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

/********** Clock Event Device *********/

/*
 * Arm the timer to interrupt after @limit cycles
 * The distinction for oneshot/periodic is done in arc_event_timer_ack() below
 */
static void arc_timer_event_setup(unsigned int limit)
{
	write_aux_reg(ARC_REG_TIMER0_LIMIT, limit);
	write_aux_reg(ARC_REG_TIMER0_CNT, 0);	/* start from 0 */

	write_aux_reg(ARC_REG_TIMER0_CTRL, TIMER_CTRL_IE | TIMER_CTRL_NH);
}

/*
 * Acknowledge the interrupt (oneshot) and optionally re-arm it (periodic)
 * -Any write to CTRL Reg will ack the intr (NH bit: Count when not halted)
 * -Rearming is done by setting the IE bit
 *
 * Small optimisation: Normal code would have been
 *   if (irq_reenable)
 *     CTRL_REG = (IE | NH);
 *   else
 *     CTRL_REG = NH;
 * However since IE is BIT0 we can fold the branch
 */
static void arc_timer_event_ack(unsigned int irq_reenable)
{
	write_aux_reg(ARC_REG_TIMER0_CTRL, irq_reenable | TIMER_CTRL_NH);
}

static int arc_clkevent_set_next_event(unsigned long delta,
				       struct clock_event_device *dev)
{
	arc_timer_event_setup(delta);
	return 0;
}

static void arc_clkevent_set_mode(enum clock_event_mode mode,
				  struct clock_event_device *dev)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		arc_timer_event_setup(CONFIG_ARC_PLAT_CLK / HZ);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	default:
		break;
	}

	return;
}

static DEFINE_PER_CPU(struct clock_event_device, arc_clockevent_device) = {
	.name		= "ARC Timer0",
	.features	= CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC,
	.mode		= CLOCK_EVT_MODE_UNUSED,
	.rating		= 300,
	.irq		= TIMER0_IRQ,	/* hardwired, no need for resources */
	.set_next_event = arc_clkevent_set_next_event,
	.set_mode	= arc_clkevent_set_mode,
};

irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
	struct clock_event_device *clk = &__get_cpu_var(arc_clockevent_device);

	arc_timer_event_ack(clk->mode == CLOCK_EVT_MODE_PERIODIC);
	clk->event_handler(clk);
	return IRQ_HANDLED;
}

void __cpuinit arc_local_timer_setup(unsigned int cpu)
{
	struct clock_event_device *clk = &per_cpu(arc_clockevent_device, cpu);

	clockevents_calc_mult_shift(clk, CONFIG_ARC_PLAT_CLK, 5);

	clk->max_delta_ns = clockevent_delta2ns(ARC_TIMER_MAX, clk);
	clk->cpumask = cpumask_of(cpu);

	clockevents_register_device(clk);

	/*
	 * Done only on Boot CPU as it would fail on others.
	 * (Treated a re-registration for a !IRQF_SHARED irq)
	 */
	if (cpu == 0) {
		int rc;
		rc = request_percpu_irq(TIMER0_IRQ, timer_irq_handler,
					"Timer0 (clk-dev)", clk);

		if (rc)
			panic("TIMER0 IRQ reg failed on BOOT cpu\n");
	}

	/* This is done on each CPU */
	enable_percpu_irq(TIMER0_IRQ, 0);
}

/*
 * Called from start_kernel() - boot CPU only
 *
 * -Sets up h/w timers as applicable on boot cpu
 * -Also sets up any global state needed for timer subsystem:
 *    - for "counting" timer, registers a clocksource, usable across CPUs
 *      (assuming that underlying counter h/w is synchronized across cores)
 *    - for "event" timer, sets up TIMER0 IRQ (as that is platform agnostic)
 */
void __init time_init(void)
{
	/* sets up the timekeeping free-flowing counter */
	arc_counter_setup();

	/*
	 * register boot-local counter as clocksource
	 * CLK upto 4.29 GHz can be safely represented in 32 bits because
	 * Max 32 bit number is 4,294,967,295
	 */
	clocksource_register_hz(&arc_counter, CONFIG_ARC_PLAT_CLK);

	/* sets up the periodic event timer */
	arc_local_timer_setup(smp_processor_id());
}

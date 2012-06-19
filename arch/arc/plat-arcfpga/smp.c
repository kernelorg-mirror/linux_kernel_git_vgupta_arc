/*
 * ARC700 Simulation-only Extensions for SMP
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Vineet Gupta    - 2012 : split off arch common and plat specific SMP
 *  Rajeshwar Ranga - 2007 : Interrupt Distribution Unit API's
 */

#include <linux/smp.h>
#include <asm/irq.h>
#include <plat/smp.h>

static char smp_cpuinfo_buf[128];

/*-------------------------------------------------------------------
 * Platform specific callbacks expected by arch SMP code
 *-------------------------------------------------------------------
 */

const char *arc_platform_smp_cpuinfo(void)
{
#define IS_AVAIL1(var, str)    ((var) ? str : "")

	struct bcr_mp mp;

	READ_BCR(ARC_REG_MP_BCR, mp);

	sprintf(smp_cpuinfo_buf, "Extn [700-SMP]: v%d, arch(%d) %s %s %s\n",
		mp.ver, mp.mp_arch, IS_AVAIL1(mp.scu, "SCU"),
		IS_AVAIL1(mp.idu, "IDU"), IS_AVAIL1(mp.sdu, "SDU"));

	return smp_cpuinfo_buf;
}

/*
 * Master kick starting another CPU
 */
void arc_platform_smp_wakeup_cpu(int cpu, unsigned long pc)
{
	/* setup the start PC */
	write_aux_reg(ARC_AUX_XTL_REG_PARAM, pc);

	/* Trigger WRITE_PC cmd for this cpu */
	write_aux_reg(ARC_AUX_XTL_REG_CMD,
			(ARC_XTL_CMD_WRITE_PC | (cpu << 8)));

	/* Take the cpu out of Halt */
	write_aux_reg(ARC_AUX_XTL_REG_CMD,
			(ARC_XTL_CMD_CLEAR_HALT | (cpu << 8)));

}

/*
 * Any SMP specific init CPU does when it comes up
 * Called from start_kernel_secondary()
 */
void arc_platform_smp_init_cpu(void)
{
	/* Owner of the Idu Interrupt determines who is SELF */
	int cpu = smp_processor_id();

	/* Check if CPU is configured for more than 16 interrupts */
	if (NR_IRQS <= 16 || get_hw_config_num_irq() <= 16)
		BUG();

	/* Setup the interrupt in IDU */
	idu_disable();

	idu_irq_set_tgtcpu(cpu,	/* IDU IRQ assoc with CPU */
			   (0x1 << cpu)	/* target cpus mask, here single cpu */
	    );

	idu_irq_set_mode(cpu,	/* IDU IRQ assoc with CPU */
			 IDU_IRQ_MOD_TCPU_ALLRECP, IDU_IRQ_MODE_PULSE_TRIG);

	idu_enable();

	/* Attach the arch-common IPI ISR to our IDU IRQ */
	smp_ipi_irq_setup(cpu, IDU_INTERRUPT_0 + cpu);
}

void arc_platform_ipi_send(const struct cpumask *callmap)
{
	unsigned int cpu;

	for_each_cpu(cpu, callmap)
		idu_irq_assert(cpu);
}

void arc_platform_ipi_clear(int cpu, int irq)
{
	idu_irq_clear(IDU_INTERRUPT_0 + cpu);
}

/*-------------------------------------------------------------------
 * IDU helpers
 *-------------------------------------------------------------------
 */

void idu_irq_set_mode(uint8_t irq, uint8_t dest_mode, uint8_t trig_mode)
{
	uint32_t par = IDU_IRQ_MODE_PARAM(dest_mode, trig_mode);

	IDU_SET_PARAM(par);
	IDU_SET_COMMAND(irq, IDU_IRQ_WMODE);
}

void idu_irq_set_tgtcpu(uint8_t irq, uint32_t mask)
{
	IDU_SET_PARAM(mask);
	IDU_SET_COMMAND(irq, IDU_IRQ_WBITMASK);
}

bool idu_irq_get_ack(uint8_t irq)
{
	uint32_t val;

	IDU_SET_COMMAND(irq, IDU_IRQ_ACK);
	val = IDU_GET_PARAM();

	return val & (1 << irq);
}

bool idu_irq_get_pend(uint8_t irq)
{
	uint32_t val;

	IDU_SET_COMMAND(irq, IDU_IRQ_PEND);
	val = IDU_GET_PARAM();

	return val & (1 << irq);
}

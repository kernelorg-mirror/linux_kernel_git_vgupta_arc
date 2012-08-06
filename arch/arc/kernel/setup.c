/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Rajeshwarr: June 2009
 *    -Tidy up the Bootmem Allocator setup
 *    -We were skipping OVER "1" page in bootmem setup
 *
 *  Vineetg: June 2009
 *    -Tag parsing done ONLY if bootloader passes it, otherwise defaults
 *     for those params are already setup at compile time.
 *
 *  Vineetg: April 2009
 *    -NFS and IDE (hda2) root filesystem works now
 *
 *  Vineetg: Jan 30th 2008
 *    -setup_processor() is now CPU init API called by all CPUs
 *        It includes TLB init, Vect Tbl Iit, Clearing ALL IRQs etc
 *    -cpuinfo_arc700 is now an array of NR_CPUS; contains MP info as well
 *    -/proc/cpuinfo iterator fixed so that show_cpuinfo() gets
 *        correct CPU-id to display CPU specific info
 */

#include <linux/serial.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/socket.h>
#include <linux/root_dev.h>
#include <linux/console.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <asm/sections.h>
#include <asm/arcregs.h>
#include <asm/tlb.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/unwind.h>
#include <asm/serial.h>
#include <asm/smp.h>
#include <plat/memmap.h>

extern void __init arc_verify_sig_sz(void);

struct cpuinfo_arc cpuinfo_arc700[NR_CPUS];
#define FIX_PTR(x)  __asm__ __volatile__(";" : "+r"(x))

/* Important System variables
 * We start with compile time DEFAULTS and over-ride ONLY if bootloader
 * passes atag list
 */

unsigned long clk_speed = CONFIG_ARC_PLAT_CLK;
struct sockaddr mac_addr = { 0, {0x64, 0x66, 0x46, 0x88, 0x63, 0x33} };

int running_on_hw = 1;	/* vs. on ISS */
char __initdata command_line[COMMAND_LINE_SIZE];
struct task_struct *_current_task[NR_CPUS];	/* currently active task */

/*
 * CPU info code now more organised. Instead of stupid if else,
 * added tables which can be run thru
 */
struct cpuinfo_data {
	int id;
	const char *str;
	int up_range;
};

void __init read_arc_build_cfg_regs(void)
{
	struct bcr_perip uncached_space;
	struct cpuinfo_arc *cpu = &cpuinfo_arc700[smp_processor_id()];
	FIX_PTR(cpu);

	READ_BCR(AUX_IDENTITY, cpu->core);

	cpu->timers = read_aux_reg(ARC_REG_TIMERS_BCR);

	cpu->vec_base = read_aux_reg(AUX_INTR_VEC_BASE);
	if (cpu->vec_base == 0)
		cpu->vec_base = (unsigned int)_int_vec_base_lds;

	READ_BCR(ARC_REG_D_UNCACH_BCR, uncached_space);
	cpu->uncached_base = uncached_space.start << 24;

	cpu->extn.mul = read_aux_reg(ARC_REG_MUL_BCR);
	cpu->extn.swap = read_aux_reg(ARC_REG_SWAP_BCR);
	cpu->extn.norm = read_aux_reg(ARC_REG_NORM_BCR);
	cpu->extn.minmax = read_aux_reg(ARC_REG_MIXMAX_BCR);
	cpu->extn.barrel = read_aux_reg(ARC_REG_BARREL_BCR);
	READ_BCR(ARC_REG_MAC_BCR, cpu->extn_mac_mul);

	cpu->extn.ext_arith = read_aux_reg(ARC_REG_EXTARITH_BCR);
	cpu->extn.crc = read_aux_reg(ARC_REG_CRC_BCR);

	/* Note that we read the CCM BCRs independent of kernel config
	 * This is to catch the cases where user doesn't know that
	 * CCMs are present in hardware build
	 */
	{
		struct bcr_iccm iccm;
		struct bcr_dccm dccm;
		struct bcr_dccm_base dccm_base;
		unsigned int bcr_32bit_val;

		bcr_32bit_val = read_aux_reg(ARC_REG_ICCM_BCR);
		if (bcr_32bit_val) {
			iccm = *((struct bcr_iccm *)&bcr_32bit_val);
			cpu->iccm.base_addr = iccm.base << 16;
			cpu->iccm.sz = 0x2000 << (iccm.sz - 1);
		}

		bcr_32bit_val = read_aux_reg(ARC_REG_DCCM_BCR);
		if (bcr_32bit_val) {
			dccm = *((struct bcr_dccm *)&bcr_32bit_val);
			cpu->dccm.sz = 0x800 << (dccm.sz);

			READ_BCR(ARC_REG_DCCMBASE_BCR, dccm_base);
			cpu->dccm.base_addr = dccm_base.addr << 8;
		}
	}

	READ_BCR(ARC_REG_XY_MEM_BCR, cpu->extn_xymem);

	read_decode_mmu_bcr();
	read_decode_cache_bcr();

	READ_BCR(ARC_REG_FP_BCR, cpu->fp);
	READ_BCR(ARC_REG_DPFP_BCR, cpu->dpfp);
}

static const struct cpuinfo_data arc_cpu_tbl[] = {
	{0x10, "ARCTangent A5", 0x1F},
	{0x20, "ARC 600", 0x2F},
	{0x30, "ARC 700", 0x33},
	{0x34, "ARC 700 R4.10", 0x34},
	{0x0, NULL}
};

char *arc_cpu_mumbojumbo(int cpu_id, char *buf, int len)
{
	int i, n = 0;
	struct cpuinfo_arc *cpu = &cpuinfo_arc700[cpu_id];
	int be = 0;
#ifdef CONFIG_CPU_BIG_ENDIAN
	be = 1;
#endif
	FIX_PTR(cpu);

	n += scnprintf(buf + n, len - n,
		       "\nARC IDENTITY\t: Family [%#02x]"
		       " Cpu-id [%#02x] Chip-id [%#4x]\n",
		       cpu->core.family, cpu->core.cpu_id,
		       cpu->core.chip_id);

	for (i = 0; arc_cpu_tbl[i].id != 0; i++) {
		if ((cpu->core.family >= arc_cpu_tbl[i].id) &&
		    (cpu->core.family <= arc_cpu_tbl[i].up_range)) {
			n += scnprintf(buf + n, len - n,
				       "processor\t: %s %s\n",
				       arc_cpu_tbl[i].str,
				       be ? "[Big Endian]" : "");
			break;
		}
	}

	if (arc_cpu_tbl[i].id == 0)
		n += scnprintf(buf + n, len - n, "UNKNOWN ARC Processor\n");

	n += scnprintf(buf + n, len - n, "CPU speed\t: %u.%02u Mhz\n",
		       (unsigned int)(CONFIG_ARC_PLAT_CLK / 1000000),
		       (unsigned int)(CONFIG_ARC_PLAT_CLK / 10000) % 100);

	n += scnprintf(buf + n, len - n, "Timers\t\t: %s %s\n",
		       (cpu->timers & 0x200) ? "TIMER1" : "",
		       (cpu->timers & 0x100) ? "TIMER0" : "");

	n += scnprintf(buf + n, len - n, "Vect Tbl Base\t: %#x\n",
		       cpu->vec_base);

	n += scnprintf(buf + n, len - n, "UNCACHED Base\t: %#x\n",
		       cpu->uncached_base);

	return buf;
}

static const struct id_to_str mul_type_nm[] = {
	{ 0x0, "N/A"},
	{ 0x1, "32x32 (spl Result Reg)" },
	{ 0x2, "32x32 (ANY Result Reg)" }
};

static const struct id_to_str mac_mul_nm[] = {
	{0x0, "N/A"},
	{0x1, "N/A"},
	{0x2, "Dual 16 x 16"},
	{0x3, "N/A"},
	{0x4, "32x16"},
	{0x5, "N/A"},
	{0x6, "Dual 16x16 and 32x16"}
};

char *arc_extn_mumbojumbo(int cpu_id, char *buf, int len)
{
	int n = 0;
	struct cpuinfo_arc *cpu = &cpuinfo_arc700[cpu_id];

	FIX_PTR(cpu);
#define IS_AVAIL1(var, str)	((var) ? str : "")
#define IS_AVAIL2(var, str)	((var == 0x2) ? str : "")
#define IS_AVAIL3(var)   ((var) ? "" : "N/A")

	n += scnprintf(buf + n, len - n,
		       "Extn [700-Base]\t: %s %s %s %s %s %s\n",
		       IS_AVAIL2(cpu->extn.norm, "norm,"),
		       IS_AVAIL2(cpu->extn.barrel, "barrel-shift,"),
		       IS_AVAIL1(cpu->extn.swap, "swap,"),
		       IS_AVAIL2(cpu->extn.minmax, "minmax,"),
		       IS_AVAIL1(cpu->extn.crc, "crc,"),
		       IS_AVAIL2(cpu->extn.ext_arith, "ext-arith"));

	n += scnprintf(buf + n, len - n, "Extn [700-MPY]\t: %s",
		       mul_type_nm[cpu->extn.mul].str);

	n += scnprintf(buf + n, len - n, "   MAC MPY: %s\n",
		       mac_mul_nm[cpu->extn_mac_mul.type].str);

	if (cpu->core.family == 0x34) {
		const char *inuse = "(in-use)";
		const char *notinuse = "(not used)";

		n += scnprintf(buf + n, len - n, "Extn [700-4.10]\t: "
			       "LLOCK/SCOND %s, SWAPE %s, RTSC %s\n",
			       __CONFIG_ARC_HAS_LLSC_VAL ? inuse : notinuse,
			       __CONFIG_ARC_HAS_SWAPE_VAL ? inuse : notinuse,
			       __CONFIG_ARC_HAS_RTSC_VAL ? inuse : notinuse);
	}

	n += scnprintf(buf + n, len - n, "DCCM: %s", IS_AVAIL3(cpu->dccm.sz));
	if (cpu->dccm.sz)
		n += scnprintf(buf + n, len - n, "@ %x, %d KB ",
			       cpu->dccm.base_addr, TO_KB(cpu->dccm.sz));

	n += scnprintf(buf + n, len - n, "  ICCM: %s", IS_AVAIL3(cpu->iccm.sz));
	if (cpu->iccm.sz)
		n += scnprintf(buf + n, len - n, "@ %x, %d KB",
			       cpu->iccm.base_addr, TO_KB(cpu->iccm.sz));

	n += scnprintf(buf + n, len - n, "\nExtn [Floating Point]: %s",
		       !(cpu->fp.ver || cpu->dpfp.ver) ? "N/A" : "");

	if (cpu->fp.ver)
		n += scnprintf(buf + n, len - n, "SP [v%d] %s",
			       cpu->fp.ver, cpu->fp.fast ? "(fast)" : "");

	if (cpu->dpfp.ver)
		n += scnprintf(buf + n, len - n, "DP [v%d] %s",
			       cpu->dpfp.ver, cpu->dpfp.fast ? "(fast)" : "");

	n += scnprintf(buf + n, len - n, "\n");

	return buf;
}

void __init arc_chk_ccms(void)
{
#if defined(CONFIG_ARC_HAS_DCCM) || defined(CONFIG_ARC_HAS_ICCM)
	struct cpuinfo_arc *cpu = &cpuinfo_arc700[smp_processor_id()];

#ifdef CONFIG_ARC_HAS_DCCM
	/*
	 * DCCM can be arbit placed in hardware.
	 * Make sure it's placement/sz matches what Linux is built with
	 */
	if ((unsigned int)__arc_dccm_base != cpu->dccm.base_addr)
		panic("Linux built with incorrect DCCM Base address\n");

	if (DCCM_COMPILE_SZ != cpu->dccm.sz)
		panic("Linux built with incorrect DCCM Size\n");
#endif

#ifdef CONFIG_ARC_HAS_ICCM
	if (ICCM_COMPILE_SZ != cpu->iccm.sz)
		panic("Linux built with incorrect ICCM Size\n");
#endif
#endif
}


/*
 * Ensure that FP hardware and kernel config match
 * -If hardware contains DPFP, kernel needs to save/restore FPU state
 *  across context switches
 * -If hardware lacks DPFP, but kernel configured to save FPU state then
 *  kernel trying to access non-existant DPFP regs will crash
 *
 * We only check for Dbl precision Floating Point, because only DPFP
 * hardware has dedicated regs which need to be saved/restored on ctx-sw
 * (Single Precision uses core regs), thus kernel is kind of oblivious to it
 */
void __init arc_chk_fpu(void)
{
	struct cpuinfo_arc *cpu = &cpuinfo_arc700[smp_processor_id()];

	if (cpu->dpfp.ver) {
#ifndef CONFIG_ARC_FPU_SAVE_RESTORE
		pr_warn("DPFP support broken in this kernel...\n");
#endif
	} else {
#ifdef CONFIG_ARC_FPU_SAVE_RESTORE
		panic("H/w lacks DPFP support, apps won't work\n");
#endif
	}
}

/*
 * Initialize and setup the processor core
 * This is called by all the CPUs thus should not do special case stuff
 *    such as only for boot CPU etc
 */

void __init setup_processor(void)
{
	char str[256];
	int cpu_id = smp_processor_id();

	read_arc_build_cfg_regs();

	arc_init_IRQ();

	printk(arc_cpu_mumbojumbo(cpu_id, str, sizeof(str)));

	/* Enable MMU */
	arc_mmu_init();

	arc_cache_init();

	arc_chk_ccms();

	printk(arc_extn_mumbojumbo(cpu_id, str, sizeof(str)));

#ifdef CONFIG_SMP
	printk(arc_platform_smp_cpuinfo());
#endif

	arc_chk_fpu();
}

static int __init parse_tag_core(struct tag *tag)
{
	printk_init("ATAG_CORE: successful parsing\n");
	return 0;
}

__tagtable(ATAG_CORE, parse_tag_core);

static int __init parse_tag_mem32(struct tag *tag)
{
	printk_init("ATAG_MEM: size = 0x%x\n", tag->u.mem.size);

	end_mem = CONFIG_ARC_PLAT_SDRAM_SIZE + CONFIG_LINUX_LINK_BASE;

	return 0;
}

__tagtable(ATAG_MEM, parse_tag_mem32);

static int __init parse_tag_clk_speed(struct tag *tag)
{
	printk_init("ATAG_CLK_SPEED: clock-speed = %d\n",
		    tag->u.clk_speed.clk_speed_hz);
	clk_speed = tag->u.clk_speed.clk_speed_hz;
	return 0;
}

__tagtable(ATAG_CLK_SPEED, parse_tag_clk_speed);

/* not yet useful... */
static int __init parse_tag_ramdisk(struct tag *tag)
{
	printk_init("ATAG_RAMDISK: Flags for ramdisk = 0x%x\n",
		    tag->u.ramdisk.flags);
	printk_init("ATAG_RAMDISK: ramdisk size = 0x%x\n", tag->u.ramdisk.size);
	printk_init("ATAG_RAMDISK: ramdisk start address = 0x%x\n",
		    tag->u.ramdisk.start);
	return 0;

}

__tagtable(ATAG_RAMDISK, parse_tag_ramdisk);

/* not yet useful.... */
static int __init parse_tag_initrd2(struct tag *tag)
{
	printk_init("ATAG_INITRD2: initrd start = 0x%x\n", tag->u.initrd.start);
	printk_init("ATAG_INITRD2: initrd size = 0x%x\n", tag->u.initrd.size);
	return 0;

}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

static int __init parse_tag_cache(struct tag *tag)
{
	printk_init("ATAG_CACHE: ICACHE status = 0x%x\n", tag->u.cache.icache);
	printk_init("ATAG_CACHE: DCACHE status = 0x%x\n", tag->u.cache.dcache);
	return 0;

}

__tagtable(ATAG_CACHE, parse_tag_cache);

#ifdef CONFIG_ARC_SERIAL
static int __init parse_tag_serial(struct tag *tag)
{
	int serial_baudrate;
	printk_init("ATAG_SERIAL: serial_nr = %d\n", tag->u.serial.serial_nr);
	/* when we have multiple uart's serial_nr should also be processed */
	printk_init("ATAG_SERIAL: serial baudrate = %d\n",
		    tag->u.serial.baudrate);
	serial_baudrate = tag->u.serial.baudrate;
	return 0;
}

__tagtable(ATAG_SERIAL, parse_tag_serial);

#endif

static int __init parse_tag_vmac(struct tag *tag)
{
	int i;
	printk_init("ATAG_VMAC: vmac address = %d:%d:%d:%d:%d:%d\n",
		    tag->u.vmac.addr[0], tag->u.vmac.addr[1],
		    tag->u.vmac.addr[2], tag->u.vmac.addr[3],
		    tag->u.vmac.addr[4], tag->u.vmac.addr[5]);

	mac_addr.sa_family = 0;

	for (i = 0; i < 6; i++)
		mac_addr.sa_data[i] = tag->u.vmac.addr[i];

	return 0;

}

__tagtable(ATAG_VMAC, parse_tag_vmac);

static int __init parse_tag_cmdline(struct tag *tag)
{
	printk_init("ATAG_CMDLINE: command line = %s\n",
		    tag->u.cmdline.cmdline);
	strcpy(command_line, tag->u.cmdline.cmdline);
	return 0;

}

__tagtable(ATAG_CMDLINE, parse_tag_cmdline);

static int __init parse_tag(struct tag *tag)
{
	struct tagtable *t;

	for (t = &__tagtable_begin; t < &__tagtable_end; t++)
		if (tag->hdr.tag == t->tag) {
			t->parse(tag);
			break;
		}

	return t < &__tagtable_end;
}

static void __init parse_tags(struct tag *tags)
{
	while (tags->hdr.tag != ATAG_NONE) {

		if (!parse_tag(tags))
			printk_init("Ignoring unknown tag type\n");

		tags = tag_next(tags);
	}

}

/* Sample of how atags must be prepared by the bootloader */
static struct init_tags {
	struct tag_header hdr1;
	struct tag_core core;
	struct tag_header hdr5;
	struct tag_clk_speed clk_speed;
	struct tag_header hdr6;
	struct tag_cache cache;
#ifdef CONFIG_ARC_SERIAL
	struct tag_header hdr7;
	struct tag_serial serial;
#endif
#ifdef CONFIG_ARCTANGENT_EMAC
	struct tag_header hdr8;
	struct tag_vmac vmac;
#endif
	struct tag_header hdr9;

} init_tags __initdata = {

	{tag_size(tag_core), ATAG_CORE},
	{},

	{tag_size(tag_clk_speed), ATAG_CLK_SPEED},
	{CONFIG_ARC_PLAT_CLK},

	{tag_size(tag_cache), ATAG_CACHE},

#ifdef CONFIG_ARC_HAS_ICACHE
	{1,
#else
	{0,

#endif
#ifdef CONFIG_ARC_HAS_DCACHE
	 1},
#else
	 0},

#endif

#ifdef CONFIG_ARC_SERIAL
	{tag_size(tag_serial), ATAG_SERIAL},
	{0, CONFIG_ARC_SERIAL_BAUD},
#endif

#ifdef CONFIG_ARCTANGENT_EMAC
	{tag_size(tag_vmac), ATAG_VMAC},
	{ {0, 1, 2, 3, 4, 5} },	/* Never set the mac address starting with 1 */
#endif

	{0, ATAG_NONE}
};

void __init __attribute__((weak)) arc_platform_early_init(void)
{
}

void __init setup_arch(char **cmdline_p)
{
	struct tag *tags = (struct tag *)&init_tags;	/* to shut up gcc */

	/* If parameters passed by u-boot, override compile-time parameters */
	if (atag_head) {
		tags = (struct tag *)atag_head;

		if (tags->hdr.tag == ATAG_CORE) {
			printk_init
			    ("Parsing ATAG parameters from bootloader\n");
			parse_tags(tags);
		} else
			printk_init
			    ("INVALID ATAG parameters from bootloader\n");
	} else {
		printk_init("SKIPPING ATAG parsing...\n");
	}

	/*
	 * Append .config cmdline to base command line, which might already
	 * contain u-boot "bootargs" (handled by head.S, if so configured)
	 */
	strlcat(command_line, CONFIG_CMDLINE, sizeof(command_line));

	/* Save unparsed command line copy for /proc/cmdline */
	strlcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);
	*cmdline_p = command_line;

	/* Platform/board specific: e.g. early console registration */
	arc_platform_early_init();

	setup_processor();

#ifdef CONFIG_SMP
	smp_init_cpus();
#endif

	setup_arch_memory();

	/* Can be issue if someone passes cmd line arg "ro"
	 * But that is unlikely so keeping it as it is
	 */
	root_mountflags &= ~MS_RDONLY;

	console_verbose();

#if defined(CONFIG_VT) && defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif

	paging_init();

	arc_verify_sig_sz();

	arc_unwind_init();
	arc_unwind_setup();
}

/*
 *  Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
	char *str;
	int cpu_id = (0xFFFF & (int)v);

	str = (char *)__get_free_page(GFP_TEMPORARY);
	if (!str)
		goto done;

	seq_printf(m, arc_cpu_mumbojumbo(cpu_id, str, PAGE_SIZE));

	seq_printf(m, "Bogo MIPS : \t%lu.%02lu\n",
		   loops_per_jiffy / (500000 / HZ),
		   (loops_per_jiffy / (5000 / HZ)) % 100);

	seq_printf(m, arc_mmu_mumbojumbo(cpu_id, str, PAGE_SIZE));

	seq_printf(m, arc_cache_mumbojumbo(cpu_id, str, PAGE_SIZE));

	seq_printf(m, arc_extn_mumbojumbo(cpu_id, str, PAGE_SIZE));

#ifdef CONFIG_SMP
	seq_printf(m, arc_platform_smp_cpuinfo());
#endif

	free_page((unsigned long)str);
done:
	seq_printf(m, "\n\n");

	return 0;
}

static void *c_start(struct seq_file *m, loff_t * pos)
{
	/*
	 * This 0xFF xxxx business is a simple hack.
	 * We encode cpu-id as 0x 00FF <cpu-id> and return it as a ptr
	 * We Can't return cpu-id directly because 1st cpu-id is 0, which has
	 * special meaning in seq-file framework (iterator end).
	 * Otherwise we have to kmalloc in c_start() and do a free in c_stop()
	 * which is really not required for a such a simple case
	 * show_cpuinfo() extracts the cpu-id from it.
	 */
	return *pos < num_possible_cpus() ?
		((void *)(0xFF0000 | (int)(*pos))) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t * pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo
};

/*
 * BPF JIT compiler for ARC
 *
 * Copyright (C) 2017-18 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "bpf_jit: " fmt

#include <linux/filter.h>
#include <linux/moduleloader.h>
#include <linux/printk.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <asm/byteorder.h>
#include <asm/cacheflush.h>

#include "bpf_jit.h"

int bpf_jit_enable __read_mostly;

static inline void bpf_flush_icache(void *start, void *end)
{
	flush_icache_range((unsigned long)start, (unsigned long)end);
}

struct bpf_prog *bpf_int_jit_compile(struct bpf_prog *prog)
{
	return prog;
}

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_MMAN_H__
#define __ARC_MMAN_H__

#include <linux/fs.h>
#include <asm-generic/mman.h>

/*
 * This is the ARC specific MMAP extension flag which allows user-space to
 * "hint" that mmap is elligible to be "common" across processes, implicitly
 * requesting the relevant hardware resources be assigned to it.
 */
#define MAP_SHARED_CODE 0x20000

#endif /* __ARC_MMAN_H__ */

/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_FCNTL_H
#define _ASM_ARC_FCNTL_H

/*
 * XXX: ABI hack
 * Potential issue if one of the following arch-specific O_xxx bits
 * overlap with a completely different asm-generic O_xxx flag.
 * Right now asm-generic has following, which averts the problem for-now
 *
 * #define O_DIRECT	00040000
 * #define O_LARGEFILE	00100000
 * #define O_DIRECTORY	00200000
 * #define O_NOFOLLOW	00400000
 *
 * This will go away once the OS ABI is revised
 * (stat and friends need fixing as well)
 */
#define O_DIRECTORY	00040000	/* must be a directory */
#define O_NOFOLLOW	00100000	/* don't follow links */
#define O_DIRECT	00200000	/* direct disk access hint */
#define O_LARGEFILE	00400000

#include <asm-generic/fcntl.h>

#endif

/*
 * DMA Mapping glue for ARC
 *
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Shamelessly copied from OpenRISC
 */

#ifndef ASM_ARC_DMA_MAPPING_H
#define ASM_ARC_DMA_MAPPING_H

#include <asm-generic/dma-coherent.h>
#include <linux/dma-mapping.h>
#include <linux/dma-debug.h>
#include <linux/kmemcheck.h>

extern struct dma_map_ops arc_dma_map_ops;

static inline struct dma_map_ops *get_dma_ops(struct device *dev)
{
	return &arc_dma_map_ops;
}

#include <asm-generic/dma-mapping-common.h>

#define dma_alloc_coherent(d,s,h,f) dma_alloc_attrs(d,s,h,f,NULL)

static inline void *dma_alloc_attrs(struct device *dev, size_t size,
				    dma_addr_t *dma_handle, gfp_t gfp,
				    struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);
	void *memory;

	memory = ops->alloc(dev, size, dma_handle, gfp, attrs);

	debug_dma_alloc_coherent(dev, size, *dma_handle, memory);

	return memory;
}

#define dma_free_coherent(d,s,c,h) dma_free_attrs(d,s,c,h,NULL)

static inline void dma_free_attrs(struct device *dev, size_t size,
				  void *cpu_addr, dma_addr_t dma_handle,
				  struct dma_attrs *attrs)
{
	struct dma_map_ops *ops = get_dma_ops(dev);

	debug_dma_free_coherent(dev, size, cpu_addr, dma_handle);

	ops->free(dev, size, cpu_addr, dma_handle, attrs);
}

static inline void *dma_alloc_noncoherent(struct device *dev, size_t size,
					  dma_addr_t *dma_handle, gfp_t gfp)
{
	struct dma_attrs attrs;

	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);

	return dma_alloc_attrs(dev, size, dma_handle, gfp, &attrs);
}

static inline void dma_free_noncoherent(struct device *dev, size_t size,
					 void *cpu_addr, dma_addr_t dma_handle)
{
	struct dma_attrs attrs;

	dma_set_attr(DMA_ATTR_NON_CONSISTENT, &attrs);

	dma_free_attrs(dev, size, cpu_addr, dma_handle, &attrs);
}

static inline int dma_supported(struct device *dev, u64 dma_mask)
{
	/* Support 32 bit DMA mask exclusively */
	return dma_mask == DMA_BIT_MASK(32);
}

static inline int dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return 0;
}

static inline int dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

#endif

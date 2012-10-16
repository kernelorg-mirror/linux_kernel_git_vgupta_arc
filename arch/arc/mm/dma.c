/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Helpers for Coherent DMA API.
 *
 * Allocate pages(s) but the CPU always accesses them using a V-P mapping
 * which has Cached Bit off
 *
 * If NON_CONSISTENT request, then CPU accesses page with normal paddr
 *
 * For both the cases above, actual DMA handle gen by platform as some
 * platforms are OK with 0x8000_0000 baed addresses while some are NOT.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-debug.h>
#include <linux/dma-attrs.h>
#include <linux/export.h>
#include <asm/cacheflush.h>
#include <plat/dma_addr.h>

static void *
arc_dma_alloc(struct device *dev, size_t size,
	       dma_addr_t *dma_handle, gfp_t gfp,
	       struct dma_attrs *attrs)
{
	void *paddr, *kvaddr;

	/* This is linear addr (0x8000_0000 based) */
	paddr = alloc_pages_exact(size, gfp);
	if (!paddr)
		return NULL;

	if (likely(!dma_get_attr(DMA_ATTR_NON_CONSISTENT, attrs))) {

		/* This is kernel Virtual address (0x7000_0000 based) */
		kvaddr = ioremap_nocache((unsigned long)paddr, size);
		if (kvaddr != NULL)
			memset(kvaddr, 0, size);
	}
	else {
		kvaddr = paddr;
	}

	/* This is bus address, platform dependent */
	*dma_handle = plat_kernel_addr_to_dma(dev, paddr);

	return kvaddr;
}

static void
arc_dma_free(struct device *dev, size_t size, void *kvaddr,
	     dma_addr_t dma_handle, struct dma_attrs *attrs)
{
	if (!dma_get_attr(DMA_ATTR_NON_CONSISTENT, attrs))
		iounmap((void __force __iomem *)kvaddr);

	free_pages_exact((void *)plat_dma_addr_to_kernel(dev, dma_handle),
			 size);
}

/*
 * Helper for streaming DMA...
 * CPU accesses page via normal paddr, thus needs to explicitly made
 * consistent before each use
 */
static dma_addr_t
arc_map_page(struct device *dev, struct page *page,
	      unsigned long offset, size_t size,
	      enum dma_data_direction dir,
	      struct dma_attrs *attrs)
{
	unsigned long paddr = page_to_phys(page) + offset;

	switch (dir) {
	case DMA_FROM_DEVICE:
		dma_cache_inv(paddr, size);
		break;
	case DMA_TO_DEVICE:
		dma_cache_wback(paddr, size);
		break;
	case DMA_BIDIRECTIONAL:
		dma_cache_wback_inv(paddr, size);
		break;
	default:
		pr_err("Invalid DMA dir [%d] for OP @ %lx\n", dir, paddr);
	}

	return plat_kernel_addr_to_dma(dev, (void *)paddr);
}

static void
arc_unmap_page(struct device *dev, dma_addr_t dma_handle,
		size_t size, enum dma_data_direction dir,
		struct dma_attrs *attrs)
{
	/* Nothing special to do here... */
}

static int
arc_map_sg(struct device *dev, struct scatterlist *sg,
	    int nents, enum dma_data_direction dir,
	    struct dma_attrs *attrs)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		s->dma_address = arc_map_page(dev, sg_page(s), s->offset,
					       s->length, dir, NULL);

	return nents;
}

static void
arc_unmap_sg(struct device *dev, struct scatterlist *sg,
	      int nents, enum dma_data_direction dir,
	      struct dma_attrs *attrs)
{
	struct scatterlist *s;
	int i;

	for_each_sg(sg, s, nents, i)
		arc_unmap_page(dev, sg_dma_address(s), sg_dma_len(s), dir,
				NULL);
}

static void
arc_sync_single_for_cpu(struct device *dev,
			 dma_addr_t dma_handle, size_t size,
			 enum dma_data_direction dir)
{
	dma_cache_inv(plat_dma_addr_to_kernel(dev, dma_handle), size);
}

static void
arc_sync_single_for_device(struct device *dev,
			    dma_addr_t dma_handle, size_t size,
			    enum dma_data_direction dir)
{
	dma_cache_wback(plat_dma_addr_to_kernel(dev, dma_handle), size);
}

struct dma_map_ops arc_dma_map_ops = {
	.alloc			= arc_dma_alloc,
	.free			= arc_dma_free,
	.map_page		= arc_map_page,
	.unmap_page		= arc_unmap_page,
	.map_sg			= arc_map_sg,
	.unmap_sg		= arc_unmap_sg,
	.sync_single_for_cpu	= arc_sync_single_for_cpu,
	.sync_single_for_device	= arc_sync_single_for_device,
};
EXPORT_SYMBOL(arc_dma_map_ops);

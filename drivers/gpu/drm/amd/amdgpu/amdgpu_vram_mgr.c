/*
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Christian König
 */

#include <drm/drmP.h>
#include "amdgpu.h"

struct amdgpu_vram_mgr {
	struct drm_mm mm;
	spinlock_t lock;
	atomic64_t usage;
	atomic64_t vis_usage;
};

/**
 * amdgpu_vram_mgr_init - init VRAM manager and DRM MM
 *
 * @man: TTM memory type manager
 * @p_size: maximum size of VRAM
 *
 * Allocate and initialize the VRAM manager.
 */
static int amdgpu_vram_mgr_init(struct ttm_mem_type_manager *man,
				unsigned long p_size)
{
	struct amdgpu_vram_mgr *mgr;

	mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);
	if (!mgr)
		return -ENOMEM;

	drm_mm_init(&mgr->mm, 0, p_size);
	spin_lock_init(&mgr->lock);
	man->priv = mgr;
	return 0;
}

/**
 * amdgpu_vram_mgr_fini - free and destroy VRAM manager
 *
 * @man: TTM memory type manager
 *
 * Destroy and free the VRAM manager, returns -EBUSY if ranges are still
 * allocated inside it.
 */
static int amdgpu_vram_mgr_fini(struct ttm_mem_type_manager *man)
{
	struct amdgpu_vram_mgr *mgr = man->priv;

	spin_lock(&mgr->lock);
	drm_mm_takedown(&mgr->mm);
	spin_unlock(&mgr->lock);
	kfree(mgr);
	man->priv = NULL;
	return 0;
}

/**
 * amdgpu_vram_mgr_vis_size - Calculate visible node size
 *
 * @adev: amdgpu device structure
 * @node: MM node structure
 *
 * Calculate how many bytes of the MM node are inside visible VRAM
 */
static u64 amdgpu_vram_mgr_vis_size(struct amdgpu_device *adev,
				    struct drm_mm_node *node)
{
	uint64_t start = node->start << PAGE_SHIFT;
	uint64_t end = (node->size + node->start) << PAGE_SHIFT;

	if (start >= adev->gmc.visible_vram_size)
		return 0;

	return (end > adev->gmc.visible_vram_size ?
		adev->gmc.visible_vram_size : end) - start;
}

/**
 * amdgpu_vram_mgr_new - allocate new ranges
 *
 * @man: TTM memory type manager
 * @tbo: TTM BO we need this range for
 * @place: placement flags and restrictions
 * @mem: the resulting mem object
 *
 * Allocate VRAM for the given BO.
 */
static int amdgpu_vram_mgr_new(struct ttm_mem_type_manager *man,
			       struct ttm_buffer_object *tbo,
			       const struct ttm_place *place,
			       struct ttm_mem_reg *mem)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(man->bdev);
	struct amdgpu_vram_mgr *mgr = man->priv;
	struct drm_mm *mm = &mgr->mm;
	struct drm_mm_node *nodes;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0) && \
	!defined(OS_NAME_RHEL_7_5)
	enum drm_mm_search_flags sflags = DRM_MM_SEARCH_DEFAULT;
	enum drm_mm_allocator_flags aflags = DRM_MM_CREATE_DEFAULT;
#else
	enum drm_mm_insert_mode mode;
#endif
	unsigned long lpfn, num_nodes, pages_per_node, pages_left;
	uint64_t usage = 0, vis_usage = 0;
	unsigned i;
	int r;

	lpfn = place->lpfn;
	if (!lpfn)
		lpfn = man->size;

	if (place->flags & TTM_PL_FLAG_CONTIGUOUS ||
	    amdgpu_vram_page_split == -1) {
		pages_per_node = ~0ul;
		num_nodes = 1;
	} else {
		pages_per_node = max((uint32_t)amdgpu_vram_page_split,
				     mem->page_alignment);
		num_nodes = DIV_ROUND_UP(mem->num_pages, pages_per_node);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	nodes = kcalloc(num_nodes, sizeof(*nodes), GFP_KERNEL);
#else
	nodes = kvmalloc_array(num_nodes, sizeof(*nodes),
			       GFP_KERNEL | __GFP_ZERO);
#endif
	if (!nodes)
		return -ENOMEM;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0) && \
	!defined(OS_NAME_RHEL_7_5)
	if (place->flags & TTM_PL_FLAG_TOPDOWN) {
		sflags = DRM_MM_SEARCH_BELOW;
		aflags = DRM_MM_CREATE_TOP;
	}
#else
	mode = DRM_MM_INSERT_BEST;
	if (place->flags & TTM_PL_FLAG_TOPDOWN)
		mode = DRM_MM_INSERT_HIGH;
#endif

	mem->start = 0;
	pages_left = mem->num_pages;

	spin_lock(&mgr->lock);
	for (i = 0; i < num_nodes; ++i) {
		unsigned long pages = min(pages_left, pages_per_node);
		uint32_t alignment = mem->page_alignment;
		unsigned long start;

		if (pages == pages_per_node)
			alignment = pages_per_node;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0) && \
		!defined(OS_NAME_RHEL_7_5)
		else
			sflags |= DRM_MM_SEARCH_BEST;

		r = drm_mm_insert_node_in_range_generic(mm, &nodes[i], pages,
							alignment, 0,
							place->fpfn, lpfn,
							sflags, aflags);
#else
		r = drm_mm_insert_node_in_range(mm, &nodes[i],
						pages, alignment, 0,
						place->fpfn, lpfn,
						mode);
#endif
		if (unlikely(r))
			goto error;

		usage += nodes[i].size << PAGE_SHIFT;
		vis_usage += amdgpu_vram_mgr_vis_size(adev, &nodes[i]);

		/* Calculate a virtual BO start address to easily check if
		 * everything is CPU accessible.
		 */
		start = nodes[i].start + nodes[i].size;
		if (start > mem->num_pages)
			start -= mem->num_pages;
		else
			start = 0;
		mem->start = max(mem->start, start);
		pages_left -= pages;
	}
	spin_unlock(&mgr->lock);

	atomic64_add(usage, &mgr->usage);
	atomic64_add(vis_usage, &mgr->vis_usage);

	mem->mm_node = nodes;

	return 0;

error:
	while (i--)
		drm_mm_remove_node(&nodes[i]);
	spin_unlock(&mgr->lock);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	kfree(nodes);
#else
	kvfree(nodes);
#endif
	return r == -ENOSPC ? 0 : r;
}

/**
 * amdgpu_vram_mgr_del - free ranges
 *
 * @man: TTM memory type manager
 * @tbo: TTM BO we need this range for
 * @place: placement flags and restrictions
 * @mem: TTM memory object
 *
 * Free the allocated VRAM again.
 */
static void amdgpu_vram_mgr_del(struct ttm_mem_type_manager *man,
				struct ttm_mem_reg *mem)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(man->bdev);
	struct amdgpu_vram_mgr *mgr = man->priv;
	struct drm_mm_node *nodes = mem->mm_node;
	uint64_t usage = 0, vis_usage = 0;
	unsigned pages = mem->num_pages;

	if (!mem->mm_node)
		return;

	spin_lock(&mgr->lock);
	while (pages) {
		pages -= nodes->size;
		drm_mm_remove_node(nodes);
		usage += nodes->size << PAGE_SHIFT;
		vis_usage += amdgpu_vram_mgr_vis_size(adev, nodes);
		++nodes;
	}
	spin_unlock(&mgr->lock);

	atomic64_sub(usage, &mgr->usage);
	atomic64_sub(vis_usage, &mgr->vis_usage);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
	kfree(mem->mm_node);
#else
	kvfree(mem->mm_node);
#endif
	mem->mm_node = NULL;
}

/**
 * amdgpu_vram_mgr_usage - how many bytes are used in this domain
 *
 * @man: TTM memory type manager
 *
 * Returns how many bytes are used in this domain.
 */
uint64_t amdgpu_vram_mgr_usage(struct ttm_mem_type_manager *man)
{
	struct amdgpu_vram_mgr *mgr = man->priv;

	return atomic64_read(&mgr->usage);
}

/**
 * amdgpu_vram_mgr_vis_usage - how many bytes are used in the visible part
 *
 * @man: TTM memory type manager
 *
 * Returns how many bytes are used in the visible part of VRAM
 */
uint64_t amdgpu_vram_mgr_vis_usage(struct ttm_mem_type_manager *man)
{
	struct amdgpu_vram_mgr *mgr = man->priv;

	return atomic64_read(&mgr->vis_usage);
}

/**
 * amdgpu_vram_mgr_debug - dump VRAM table
 *
 * @man: TTM memory type manager
 * @printer: DRM printer to use
 * @prefix: text prefix
 *
 * Dump the table content using printk.
 */
static void amdgpu_vram_mgr_debug(struct ttm_mem_type_manager *man,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) || \
	defined(OS_NAME_RHEL_7_5)
				  struct drm_printer *printer)
#else
				  const char *prefix)
#endif
{
	struct amdgpu_vram_mgr *mgr = man->priv;

	spin_lock(&mgr->lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) || \
	defined(OS_NAME_RHEL_7_5)
	drm_mm_print(&mgr->mm, printer);
#else
	drm_mm_debug_table(&mgr->mm, prefix);
#endif
	spin_unlock(&mgr->lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) || \
	defined(OS_NAME_RHEL_7_5)
	drm_printf(printer, "man size:%llu pages, ram usage:%lluMB, vis usage:%lluMB\n",
		   man->size, amdgpu_vram_mgr_usage(man) >> 20,
		   amdgpu_vram_mgr_vis_usage(man) >> 20);
#else
	DRM_DEBUG("man size:%llu pages, ram usage:%lluMB, vis usage:%lluMB\n",
		   man->size, amdgpu_vram_mgr_usage(man) >> 20,
		   amdgpu_vram_mgr_vis_usage(man) >> 20);
#endif
}

const struct ttm_mem_type_manager_func amdgpu_vram_mgr_func = {
	.init		= amdgpu_vram_mgr_init,
	.takedown	= amdgpu_vram_mgr_fini,
	.get_node	= amdgpu_vram_mgr_new,
	.put_node	= amdgpu_vram_mgr_del,
	.debug		= amdgpu_vram_mgr_debug
};

/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2019 Linaro, Ltd, Rob Herring <robh@kernel.org> */

#ifndef __PANFROST_GEM_H__
#define __PANFROST_GEM_H__

#include <drm/drm_gem_shmem_helper.h>
#include <drm/drm_mm.h>

struct panfrost_mmu;
struct panfrost_device;

#define PANFROST_BO_LABEL_MAXLEN	4096

enum panfrost_debugfs_gem_state_flags {
	/** @PANFROST_DEBUGFS_GEM_STATE_FLAG_IMPORTED: GEM BO is PRIME imported. */
	PANFROST_DEBUGFS_GEM_STATE_FLAG_IMPORTED = BIT(0),

	/** @PANFROST_DEBUGFS_GEM_STATE_FLAG_EXPORTED: GEM BO is PRIME exported. */
	PANFROST_DEBUGFS_GEM_STATE_FLAG_EXPORTED = BIT(1),

	/** @PANFROST_DEBUGFS_GEM_STATE_FLAG_PURGED: GEM BO was reclaimed by the shrinker. */
	PANFROST_DEBUGFS_GEM_STATE_FLAG_PURGED = BIT(2),

	/**
	 * @PANFROST_DEBUGFS_GEM_STATE_FLAG_PURGEABLE: GEM BO pages were marked as no longer
	 * needed by UM and can be reclaimed by the shrinker.
	 */
	PANFROST_DEBUGFS_GEM_STATE_FLAG_PURGEABLE = BIT(3),
};

/**
 * struct panfrost_gem_debugfs - GEM object's DebugFS list information
 */
struct panfrost_gem_debugfs {
	/**
	 * @node: Node used to insert the object in the device-wide list of
	 * GEM objects, to display information about it through a DebugFS file.
	 */
	struct list_head node;

	/** @creator: Information about the UM process which created the GEM. */
	struct {
		/** @creator.process_name: Group leader name in owning thread's process */
		char process_name[TASK_COMM_LEN];

		/** @creator.tgid: PID of the thread's group leader within its process */
		pid_t tgid;
	} creator;
};

struct panfrost_gem_object {
	struct drm_gem_shmem_object base;
	struct sg_table *sgts;

	/*
	 * Use a list for now. If searching a mapping ever becomes the
	 * bottleneck, we should consider using an RB-tree, or even better,
	 * let the core store drm_gem_object_mapping entries (where we
	 * could place driver specific data) instead of drm_gem_object ones
	 * in its drm_file->object_idr table.
	 *
	 * struct drm_gem_object_mapping {
	 *	struct drm_gem_object *obj;
	 *	void *driver_priv;
	 * };
	 */
	struct {
		struct list_head list;
		struct mutex lock;
	} mappings;

	/*
	 * Count the number of jobs referencing this BO so we don't let the
	 * shrinker reclaim this object prematurely.
	 */
	atomic_t gpu_usecount;

	/*
	 * Object chunk size currently mapped onto physical memory
	 */
	size_t heap_rss_size;

	/**
	 * @label: BO tagging fields. The label can be assigned within the
	 * driver itself or through a specific IOCTL.
	 */
	struct {
		/**
		 * @label.str: Pointer to NULL-terminated string,
		 */
		const char *str;

		/** @lock.str: Protects access to the @label.str field. */
		struct mutex lock;
	} label;

	bool noexec		:1;
	bool is_heap		:1;

#ifdef CONFIG_DEBUG_FS
	struct panfrost_gem_debugfs debugfs;
#endif
};

struct panfrost_gem_mapping {
	struct list_head node;
	struct kref refcount;
	struct panfrost_gem_object *obj;
	struct drm_mm_node mmnode;
	struct panfrost_mmu *mmu;
	bool active		:1;
};

static inline
struct  panfrost_gem_object *to_panfrost_bo(struct drm_gem_object *obj)
{
	return container_of(to_drm_gem_shmem_obj(obj), struct panfrost_gem_object, base);
}

static inline struct panfrost_gem_mapping *
drm_mm_node_to_panfrost_mapping(struct drm_mm_node *node)
{
	return container_of(node, struct panfrost_gem_mapping, mmnode);
}

struct drm_gem_object *panfrost_gem_create_object(struct drm_device *dev, size_t size);

struct drm_gem_object *
panfrost_gem_prime_import_sg_table(struct drm_device *dev,
				   struct dma_buf_attachment *attach,
				   struct sg_table *sgt);

struct panfrost_gem_object *
panfrost_gem_create(struct drm_device *dev, size_t size, u32 flags);

int panfrost_gem_open(struct drm_gem_object *obj, struct drm_file *file_priv);
void panfrost_gem_close(struct drm_gem_object *obj,
			struct drm_file *file_priv);

struct panfrost_gem_mapping *
panfrost_gem_mapping_get(struct panfrost_gem_object *bo,
			 struct panfrost_file_priv *priv);
void panfrost_gem_mapping_put(struct panfrost_gem_mapping *mapping);
void panfrost_gem_teardown_mappings_locked(struct panfrost_gem_object *bo);

int panfrost_gem_shrinker_init(struct drm_device *dev);
void panfrost_gem_shrinker_cleanup(struct drm_device *dev);

void panfrost_gem_set_label(struct drm_gem_object *obj, const char *label);
void panfrost_gem_internal_set_label(struct drm_gem_object *obj, const char *label);

#ifdef CONFIG_DEBUG_FS
void panfrost_gem_debugfs_print_bos(struct panfrost_device *pfdev,
				    struct seq_file *m);
#endif

#endif /* __PANFROST_GEM_H__ */

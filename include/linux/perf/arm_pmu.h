/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  linux/arch/arm/include/asm/pmu.h
 *
 *  Copyright (C) 2009 picoChip Designs Ltd, Jamie Iles
 */

#ifndef __ARM_PMU_H__
#define __ARM_PMU_H__

#include <linux/interrupt.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <asm/cputype.h>

#ifdef CONFIG_ARM_PMU

/*
 * The Armv7 and Armv8.8 or less CPU PMU supports up to 32 event counters.
 * The Armv8.9/9.4 CPU PMU supports up to 33 event counters.
 */
#ifdef CONFIG_ARM
#define ARMPMU_MAX_HWEVENTS		32
#else
#define ARMPMU_MAX_HWEVENTS		33
#endif
/*
 * ARM PMU hw_event flags
 */
#define ARMPMU_EVT_64BIT		0x00001 /* Event uses a 64bit counter */
#define ARMPMU_EVT_47BIT		0x00002 /* Event uses a 47bit counter */
#define ARMPMU_EVT_63BIT		0x00004 /* Event uses a 63bit counter */

static_assert((PERF_EVENT_FLAG_ARCH & ARMPMU_EVT_64BIT) == ARMPMU_EVT_64BIT);
static_assert((PERF_EVENT_FLAG_ARCH & ARMPMU_EVT_47BIT) == ARMPMU_EVT_47BIT);
static_assert((PERF_EVENT_FLAG_ARCH & ARMPMU_EVT_63BIT) == ARMPMU_EVT_63BIT);

#define HW_OP_UNSUPPORTED		0xFFFF
#define C(_x)				PERF_COUNT_HW_CACHE_##_x
#define CACHE_OP_UNSUPPORTED		0xFFFF

#define PERF_MAP_ALL_UNSUPPORTED					\
	[0 ... PERF_COUNT_HW_MAX - 1] = HW_OP_UNSUPPORTED

#define PERF_CACHE_MAP_ALL_UNSUPPORTED					\
[0 ... C(MAX) - 1] = {							\
	[0 ... C(OP_MAX) - 1] = {					\
		[0 ... C(RESULT_MAX) - 1] = CACHE_OP_UNSUPPORTED,	\
	},								\
}

/* The events for a given PMU register set. */
struct pmu_hw_events {
	/*
	 * The events that are active on the PMU for the given index.
	 */
	struct perf_event	*events[ARMPMU_MAX_HWEVENTS];

	/*
	 * A 1 bit for an index indicates that the counter is being used for
	 * an event. A 0 means that the counter can be used.
	 */
	DECLARE_BITMAP(used_mask, ARMPMU_MAX_HWEVENTS);

	/*
	 * When using percpu IRQs, we need a percpu dev_id. Place it here as we
	 * already have to allocate this struct per cpu.
	 */
	struct arm_pmu		*percpu_pmu;

	int irq;

	struct perf_branch_stack	*branch_stack;

	/* Active events requesting branch records */
	unsigned int		branch_users;
};

enum armpmu_attr_groups {
	ARMPMU_ATTR_GROUP_COMMON,
	ARMPMU_ATTR_GROUP_EVENTS,
	ARMPMU_ATTR_GROUP_FORMATS,
	ARMPMU_ATTR_GROUP_CAPS,
	ARMPMU_NR_ATTR_GROUPS
};

struct arm_pmu {
	struct pmu	pmu;
	cpumask_t	supported_cpus;
	char		*name;
	irqreturn_t	(*handle_irq)(struct arm_pmu *pmu);
	void		(*enable)(struct perf_event *event);
	void		(*disable)(struct perf_event *event);
	int		(*get_event_idx)(struct pmu_hw_events *hw_events,
					 struct perf_event *event);
	void		(*clear_event_idx)(struct pmu_hw_events *hw_events,
					 struct perf_event *event);
	int		(*set_event_filter)(struct hw_perf_event *evt,
					    struct perf_event_attr *attr);
	u64		(*read_counter)(struct perf_event *event);
	void		(*write_counter)(struct perf_event *event, u64 val);
	void		(*start)(struct arm_pmu *);
	void		(*stop)(struct arm_pmu *);
	void		(*reset)(void *);
	int		(*map_event)(struct perf_event *event);
	/*
	 * Called by KVM to map the PMUv3 event space onto non-PMUv3 hardware.
	 */
	int		(*map_pmuv3_event)(unsigned int eventsel);
	DECLARE_BITMAP(cntr_mask, ARMPMU_MAX_HWEVENTS);
	bool		secure_access; /* 32-bit ARM only */
	struct platform_device	*plat_device;
	struct pmu_hw_events	__percpu *hw_events;
	struct hlist_node	node;
	struct notifier_block	cpu_pm_nb;
	/* the attr_groups array must be NULL-terminated */
	const struct attribute_group *attr_groups[ARMPMU_NR_ATTR_GROUPS + 1];

	/* PMUv3 only */
	int		pmuver;
	u64		reg_pmmir;
	u64		reg_brbidr;
#define ARMV8_PMUV3_MAX_COMMON_EVENTS		0x40
	DECLARE_BITMAP(pmceid_bitmap, ARMV8_PMUV3_MAX_COMMON_EVENTS);
#define ARMV8_PMUV3_EXT_COMMON_EVENT_BASE	0x4000
	DECLARE_BITMAP(pmceid_ext_bitmap, ARMV8_PMUV3_MAX_COMMON_EVENTS);

	/* Only to be used by ACPI probing code */
	unsigned long acpi_cpuid;
};

#define to_arm_pmu(p) (container_of(p, struct arm_pmu, pmu))

DECLARE_PER_CPU(struct arm_pmu *, cpu_armpmu);

u64 armpmu_event_update(struct perf_event *event);

int armpmu_event_set_period(struct perf_event *event);

int armpmu_map_event(struct perf_event *event,
		     const unsigned (*event_map)[PERF_COUNT_HW_MAX],
		     const unsigned (*cache_map)[PERF_COUNT_HW_CACHE_MAX]
						[PERF_COUNT_HW_CACHE_OP_MAX]
						[PERF_COUNT_HW_CACHE_RESULT_MAX],
		     u32 raw_event_mask);

typedef int (*armpmu_init_fn)(struct arm_pmu *);

struct pmu_probe_info {
	unsigned int cpuid;
	unsigned int mask;
	armpmu_init_fn init;
};

#define PMU_PROBE(_cpuid, _mask, _fn)	\
{					\
	.cpuid = (_cpuid),		\
	.mask = (_mask),		\
	.init = (_fn),			\
}

#define ARM_PMU_PROBE(_cpuid, _fn) \
	PMU_PROBE(_cpuid, ARM_CPU_PART_MASK, _fn)

#define ARM_PMU_XSCALE_MASK	((0xff << 24) | ARM_CPU_XSCALE_ARCH_MASK)

#define XSCALE_PMU_PROBE(_version, _fn) \
	PMU_PROBE(ARM_CPU_IMP_INTEL << 24 | _version, ARM_PMU_XSCALE_MASK, _fn)

int arm_pmu_device_probe(struct platform_device *pdev,
			 const struct of_device_id *of_table,
			 const struct pmu_probe_info *probe_table);

#ifdef CONFIG_ACPI
int arm_pmu_acpi_probe(armpmu_init_fn init_fn);
#else
static inline int arm_pmu_acpi_probe(armpmu_init_fn init_fn) { return 0; }
#endif

#ifdef CONFIG_KVM
void kvm_host_pmu_init(struct arm_pmu *pmu);
#else
#define kvm_host_pmu_init(x)	do { } while(0)
#endif

bool arm_pmu_irq_is_nmi(void);

/* Internal functions only for core arm_pmu code */
struct arm_pmu *armpmu_alloc(void);
void armpmu_free(struct arm_pmu *pmu);
int armpmu_register(struct arm_pmu *pmu);
int armpmu_request_irq(int irq, int cpu);
void armpmu_free_irq(int irq, int cpu);

#define ARMV8_PMU_PDEV_NAME "armv8-pmu"

#endif /* CONFIG_ARM_PMU */

#define ARMV8_SPE_PDEV_NAME "arm,spe-v1"
#define ARMV8_TRBE_PDEV_NAME "arm,trbe"

/* Why does everything I do descend into this? */
#define __GEN_PMU_FORMAT_ATTR(cfg, lo, hi)				\
	(lo) == (hi) ? #cfg ":" #lo "\n" : #cfg ":" #lo "-" #hi

#define _GEN_PMU_FORMAT_ATTR(cfg, lo, hi)				\
	__GEN_PMU_FORMAT_ATTR(cfg, lo, hi)

#define GEN_PMU_FORMAT_ATTR(name)					\
	PMU_FORMAT_ATTR(name,						\
	_GEN_PMU_FORMAT_ATTR(ATTR_CFG_FLD_##name##_CFG,			\
			     ATTR_CFG_FLD_##name##_LO,			\
			     ATTR_CFG_FLD_##name##_HI))

#define _ATTR_CFG_GET_FLD(attr, cfg, lo, hi)				\
	((((attr)->cfg) >> lo) & GENMASK_ULL(hi - lo, 0))

#define ATTR_CFG_GET_FLD(attr, name)					\
	_ATTR_CFG_GET_FLD(attr,						\
			  ATTR_CFG_FLD_##name##_CFG,			\
			  ATTR_CFG_FLD_##name##_LO,			\
			  ATTR_CFG_FLD_##name##_HI)

#endif /* __ARM_PMU_H__ */

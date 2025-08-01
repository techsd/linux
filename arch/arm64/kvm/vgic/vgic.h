/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015, 2016 ARM Ltd.
 */
#ifndef __KVM_ARM_VGIC_NEW_H__
#define __KVM_ARM_VGIC_NEW_H__

#include <linux/irqchip/arm-gic-common.h>
#include <asm/kvm_mmu.h>

#define PRODUCT_ID_KVM		0x4b	/* ASCII code K */
#define IMPLEMENTER_ARM		0x43b

#define VGIC_ADDR_UNDEF		(-1)
#define IS_VGIC_ADDR_UNDEF(_x)  ((_x) == VGIC_ADDR_UNDEF)

#define INTERRUPT_ID_BITS_SPIS	10
#define INTERRUPT_ID_BITS_ITS	16
#define VGIC_LPI_MAX_INTID	((1 << INTERRUPT_ID_BITS_ITS) - 1)
#define VGIC_PRI_BITS		5

#define vgic_irq_is_sgi(intid) ((intid) < VGIC_NR_SGIS)

#define VGIC_AFFINITY_0_SHIFT 0
#define VGIC_AFFINITY_0_MASK (0xffUL << VGIC_AFFINITY_0_SHIFT)
#define VGIC_AFFINITY_1_SHIFT 8
#define VGIC_AFFINITY_1_MASK (0xffUL << VGIC_AFFINITY_1_SHIFT)
#define VGIC_AFFINITY_2_SHIFT 16
#define VGIC_AFFINITY_2_MASK (0xffUL << VGIC_AFFINITY_2_SHIFT)
#define VGIC_AFFINITY_3_SHIFT 24
#define VGIC_AFFINITY_3_MASK (0xffUL << VGIC_AFFINITY_3_SHIFT)

#define VGIC_AFFINITY_LEVEL(reg, level) \
	((((reg) & VGIC_AFFINITY_## level ##_MASK) \
	>> VGIC_AFFINITY_## level ##_SHIFT) << MPIDR_LEVEL_SHIFT(level))

/*
 * The Userspace encodes the affinity differently from the MPIDR,
 * Below macro converts vgic userspace format to MPIDR reg format.
 */
#define VGIC_TO_MPIDR(val) (VGIC_AFFINITY_LEVEL(val, 0) | \
			    VGIC_AFFINITY_LEVEL(val, 1) | \
			    VGIC_AFFINITY_LEVEL(val, 2) | \
			    VGIC_AFFINITY_LEVEL(val, 3))

/*
 * As per Documentation/virt/kvm/devices/arm-vgic-v3.rst,
 * below macros are defined for CPUREG encoding.
 */
#define KVM_REG_ARM_VGIC_SYSREG_OP0_MASK   0x000000000000c000
#define KVM_REG_ARM_VGIC_SYSREG_OP0_SHIFT  14
#define KVM_REG_ARM_VGIC_SYSREG_OP1_MASK   0x0000000000003800
#define KVM_REG_ARM_VGIC_SYSREG_OP1_SHIFT  11
#define KVM_REG_ARM_VGIC_SYSREG_CRN_MASK   0x0000000000000780
#define KVM_REG_ARM_VGIC_SYSREG_CRN_SHIFT  7
#define KVM_REG_ARM_VGIC_SYSREG_CRM_MASK   0x0000000000000078
#define KVM_REG_ARM_VGIC_SYSREG_CRM_SHIFT  3
#define KVM_REG_ARM_VGIC_SYSREG_OP2_MASK   0x0000000000000007
#define KVM_REG_ARM_VGIC_SYSREG_OP2_SHIFT  0

#define KVM_DEV_ARM_VGIC_SYSREG_MASK (KVM_REG_ARM_VGIC_SYSREG_OP0_MASK | \
				      KVM_REG_ARM_VGIC_SYSREG_OP1_MASK | \
				      KVM_REG_ARM_VGIC_SYSREG_CRN_MASK | \
				      KVM_REG_ARM_VGIC_SYSREG_CRM_MASK | \
				      KVM_REG_ARM_VGIC_SYSREG_OP2_MASK)

#define KVM_ICC_SRE_EL2		(ICC_SRE_EL2_ENABLE | ICC_SRE_EL2_SRE |	\
				 ICC_SRE_EL1_DIB | ICC_SRE_EL1_DFB)
#define KVM_ICH_VTR_EL2_RES0	(ICH_VTR_EL2_DVIM 	|	\
				 ICH_VTR_EL2_A3V	|	\
				 ICH_VTR_EL2_IDbits)
#define KVM_ICH_VTR_EL2_RES1	ICH_VTR_EL2_nV4

static inline u64 kvm_get_guest_vtr_el2(void)
{
	u64 vtr;

	vtr  = kvm_vgic_global_state.ich_vtr_el2;
	vtr &= ~KVM_ICH_VTR_EL2_RES0;
	vtr |= KVM_ICH_VTR_EL2_RES1;

	return vtr;
}

/*
 * As per Documentation/virt/kvm/devices/arm-vgic-its.rst,
 * below macros are defined for ITS table entry encoding.
 */
#define KVM_ITS_CTE_VALID_SHIFT		63
#define KVM_ITS_CTE_VALID_MASK		BIT_ULL(63)
#define KVM_ITS_CTE_RDBASE_SHIFT	16
#define KVM_ITS_CTE_ICID_MASK		GENMASK_ULL(15, 0)
#define KVM_ITS_ITE_NEXT_SHIFT		48
#define KVM_ITS_ITE_PINTID_SHIFT	16
#define KVM_ITS_ITE_PINTID_MASK		GENMASK_ULL(47, 16)
#define KVM_ITS_ITE_ICID_MASK		GENMASK_ULL(15, 0)
#define KVM_ITS_DTE_VALID_SHIFT		63
#define KVM_ITS_DTE_VALID_MASK		BIT_ULL(63)
#define KVM_ITS_DTE_NEXT_SHIFT		49
#define KVM_ITS_DTE_NEXT_MASK		GENMASK_ULL(62, 49)
#define KVM_ITS_DTE_ITTADDR_SHIFT	5
#define KVM_ITS_DTE_ITTADDR_MASK	GENMASK_ULL(48, 5)
#define KVM_ITS_DTE_SIZE_MASK		GENMASK_ULL(4, 0)
#define KVM_ITS_L1E_VALID_MASK		BIT_ULL(63)
/* we only support 64 kB translation table page size */
#define KVM_ITS_L1E_ADDR_MASK		GENMASK_ULL(51, 16)

#define KVM_VGIC_V3_RDIST_INDEX_MASK	GENMASK_ULL(11, 0)
#define KVM_VGIC_V3_RDIST_FLAGS_MASK	GENMASK_ULL(15, 12)
#define KVM_VGIC_V3_RDIST_FLAGS_SHIFT	12
#define KVM_VGIC_V3_RDIST_BASE_MASK	GENMASK_ULL(51, 16)
#define KVM_VGIC_V3_RDIST_COUNT_MASK	GENMASK_ULL(63, 52)
#define KVM_VGIC_V3_RDIST_COUNT_SHIFT	52

#ifdef CONFIG_DEBUG_SPINLOCK
#define DEBUG_SPINLOCK_BUG_ON(p) BUG_ON(p)
#else
#define DEBUG_SPINLOCK_BUG_ON(p)
#endif

static inline u32 vgic_get_implementation_rev(struct kvm_vcpu *vcpu)
{
	return vcpu->kvm->arch.vgic.implementation_rev;
}

/* Requires the irq_lock to be held by the caller. */
static inline bool irq_is_pending(struct vgic_irq *irq)
{
	if (irq->config == VGIC_CONFIG_EDGE)
		return irq->pending_latch;
	else
		return irq->pending_latch || irq->line_level;
}

static inline bool vgic_irq_is_mapped_level(struct vgic_irq *irq)
{
	return irq->config == VGIC_CONFIG_LEVEL && irq->hw;
}

static inline int vgic_irq_get_lr_count(struct vgic_irq *irq)
{
	/* Account for the active state as an interrupt */
	if (vgic_irq_is_sgi(irq->intid) && irq->source)
		return hweight8(irq->source) + irq->active;

	return irq_is_pending(irq) || irq->active;
}

static inline bool vgic_irq_is_multi_sgi(struct vgic_irq *irq)
{
	return vgic_irq_get_lr_count(irq) > 1;
}

static inline int vgic_write_guest_lock(struct kvm *kvm, gpa_t gpa,
					const void *data, unsigned long len)
{
	struct vgic_dist *dist = &kvm->arch.vgic;
	int ret;

	dist->table_write_in_progress = true;
	ret = kvm_write_guest_lock(kvm, gpa, data, len);
	dist->table_write_in_progress = false;

	return ret;
}

/*
 * This struct provides an intermediate representation of the fields contained
 * in the GICH_VMCR and ICH_VMCR registers, such that code exporting the GIC
 * state to userspace can generate either GICv2 or GICv3 CPU interface
 * registers regardless of the hardware backed GIC used.
 */
struct vgic_vmcr {
	u32	grpen0;
	u32	grpen1;

	u32	ackctl;
	u32	fiqen;
	u32	cbpr;
	u32	eoim;

	u32	abpr;
	u32	bpr;
	u32	pmr;  /* Priority mask field in the GICC_PMR and
		       * ICC_PMR_EL1 priority field format */
};

struct vgic_reg_attr {
	struct kvm_vcpu *vcpu;
	gpa_t addr;
};

struct its_device {
	struct list_head dev_list;

	/* the head for the list of ITTEs */
	struct list_head itt_head;
	u32 num_eventid_bits;
	gpa_t itt_addr;
	u32 device_id;
};

#define COLLECTION_NOT_MAPPED ((u32)~0)

struct its_collection {
	struct list_head coll_list;

	u32 collection_id;
	u32 target_addr;
};

#define its_is_collection_mapped(coll) ((coll) && \
				((coll)->target_addr != COLLECTION_NOT_MAPPED))

struct its_ite {
	struct list_head ite_list;

	struct vgic_irq *irq;
	struct its_collection *collection;
	u32 event_id;
};

int vgic_v3_parse_attr(struct kvm_device *dev, struct kvm_device_attr *attr,
		       struct vgic_reg_attr *reg_attr);
int vgic_v2_parse_attr(struct kvm_device *dev, struct kvm_device_attr *attr,
		       struct vgic_reg_attr *reg_attr);
const struct vgic_register_region *
vgic_get_mmio_region(struct kvm_vcpu *vcpu, struct vgic_io_device *iodev,
		     gpa_t addr, int len);
struct vgic_irq *vgic_get_irq(struct kvm *kvm, u32 intid);
struct vgic_irq *vgic_get_vcpu_irq(struct kvm_vcpu *vcpu, u32 intid);
void vgic_put_irq(struct kvm *kvm, struct vgic_irq *irq);
bool vgic_get_phys_line_level(struct vgic_irq *irq);
void vgic_irq_set_phys_pending(struct vgic_irq *irq, bool pending);
void vgic_irq_set_phys_active(struct vgic_irq *irq, bool active);
bool vgic_queue_irq_unlock(struct kvm *kvm, struct vgic_irq *irq,
			   unsigned long flags) __releases(&irq->irq_lock);
void vgic_kick_vcpus(struct kvm *kvm);
void vgic_irq_handle_resampling(struct vgic_irq *irq,
				bool lr_deactivated, bool lr_pending);

int vgic_check_iorange(struct kvm *kvm, phys_addr_t ioaddr,
		       phys_addr_t addr, phys_addr_t alignment,
		       phys_addr_t size);

void vgic_v2_fold_lr_state(struct kvm_vcpu *vcpu);
void vgic_v2_populate_lr(struct kvm_vcpu *vcpu, struct vgic_irq *irq, int lr);
void vgic_v2_clear_lr(struct kvm_vcpu *vcpu, int lr);
void vgic_v2_set_underflow(struct kvm_vcpu *vcpu);
int vgic_v2_has_attr_regs(struct kvm_device *dev, struct kvm_device_attr *attr);
int vgic_v2_dist_uaccess(struct kvm_vcpu *vcpu, bool is_write,
			 int offset, u32 *val);
int vgic_v2_cpuif_uaccess(struct kvm_vcpu *vcpu, bool is_write,
			  int offset, u32 *val);
void vgic_v2_set_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
void vgic_v2_get_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
void vgic_v2_enable(struct kvm_vcpu *vcpu);
int vgic_v2_probe(const struct gic_kvm_info *info);
int vgic_v2_map_resources(struct kvm *kvm);
int vgic_register_dist_iodev(struct kvm *kvm, gpa_t dist_base_address,
			     enum vgic_type);

void vgic_v2_init_lrs(void);
void vgic_v2_load(struct kvm_vcpu *vcpu);
void vgic_v2_put(struct kvm_vcpu *vcpu);

void vgic_v2_save_state(struct kvm_vcpu *vcpu);
void vgic_v2_restore_state(struct kvm_vcpu *vcpu);

static inline bool vgic_try_get_irq_kref(struct vgic_irq *irq)
{
	if (!irq)
		return false;

	if (irq->intid < VGIC_MIN_LPI)
		return true;

	return kref_get_unless_zero(&irq->refcount);
}

static inline void vgic_get_irq_kref(struct vgic_irq *irq)
{
	WARN_ON_ONCE(!vgic_try_get_irq_kref(irq));
}

void vgic_v3_fold_lr_state(struct kvm_vcpu *vcpu);
void vgic_v3_populate_lr(struct kvm_vcpu *vcpu, struct vgic_irq *irq, int lr);
void vgic_v3_clear_lr(struct kvm_vcpu *vcpu, int lr);
void vgic_v3_set_underflow(struct kvm_vcpu *vcpu);
void vgic_v3_set_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
void vgic_v3_get_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
void vgic_v3_enable(struct kvm_vcpu *vcpu);
int vgic_v3_probe(const struct gic_kvm_info *info);
int vgic_v3_map_resources(struct kvm *kvm);
int vgic_v3_lpi_sync_pending_status(struct kvm *kvm, struct vgic_irq *irq);
int vgic_v3_save_pending_tables(struct kvm *kvm);
int vgic_v3_set_redist_base(struct kvm *kvm, u32 index, u64 addr, u32 count);
int vgic_register_redist_iodev(struct kvm_vcpu *vcpu);
void vgic_unregister_redist_iodev(struct kvm_vcpu *vcpu);
bool vgic_v3_check_base(struct kvm *kvm);

void vgic_v3_load(struct kvm_vcpu *vcpu);
void vgic_v3_put(struct kvm_vcpu *vcpu);

bool vgic_has_its(struct kvm *kvm);
int kvm_vgic_register_its_device(void);
void vgic_enable_lpis(struct kvm_vcpu *vcpu);
void vgic_flush_pending_lpis(struct kvm_vcpu *vcpu);
int vgic_its_inject_msi(struct kvm *kvm, struct kvm_msi *msi);
int vgic_v3_has_attr_regs(struct kvm_device *dev, struct kvm_device_attr *attr);
int vgic_v3_dist_uaccess(struct kvm_vcpu *vcpu, bool is_write,
			 int offset, u32 *val);
int vgic_v3_redist_uaccess(struct kvm_vcpu *vcpu, bool is_write,
			 int offset, u32 *val);
int vgic_v3_cpu_sysregs_uaccess(struct kvm_vcpu *vcpu,
				struct kvm_device_attr *attr, bool is_write);
int vgic_v3_has_cpu_sysregs_attr(struct kvm_vcpu *vcpu, struct kvm_device_attr *attr);
const struct sys_reg_desc *vgic_v3_get_sysreg_table(unsigned int *sz);
int vgic_v3_line_level_info_uaccess(struct kvm_vcpu *vcpu, bool is_write,
				    u32 intid, u32 *val);
int kvm_register_vgic_device(unsigned long type);
void vgic_set_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
void vgic_get_vmcr(struct kvm_vcpu *vcpu, struct vgic_vmcr *vmcr);
int vgic_lazy_init(struct kvm *kvm);
int vgic_init(struct kvm *kvm);

void vgic_debug_init(struct kvm *kvm);
void vgic_debug_destroy(struct kvm *kvm);

int vgic_v5_probe(const struct gic_kvm_info *info);

static inline int vgic_v3_max_apr_idx(struct kvm_vcpu *vcpu)
{
	struct vgic_cpu *cpu_if = &vcpu->arch.vgic_cpu;

	/*
	 * num_pri_bits are initialized with HW supported values.
	 * We can rely safely on num_pri_bits even if VM has not
	 * restored ICC_CTLR_EL1 before restoring APnR registers.
	 */
	switch (cpu_if->num_pri_bits) {
	case 7: return 3;
	case 6: return 1;
	default: return 0;
	}
}

static inline bool
vgic_v3_redist_region_full(struct vgic_redist_region *region)
{
	if (!region->count)
		return false;

	return (region->free_index >= region->count);
}

struct vgic_redist_region *vgic_v3_rdist_free_slot(struct list_head *rdregs);

static inline size_t
vgic_v3_rd_region_size(struct kvm *kvm, struct vgic_redist_region *rdreg)
{
	if (!rdreg->count)
		return atomic_read(&kvm->online_vcpus) * KVM_VGIC_V3_REDIST_SIZE;
	else
		return rdreg->count * KVM_VGIC_V3_REDIST_SIZE;
}

struct vgic_redist_region *vgic_v3_rdist_region_from_index(struct kvm *kvm,
							   u32 index);
void vgic_v3_free_redist_region(struct kvm *kvm, struct vgic_redist_region *rdreg);

bool vgic_v3_rdist_overlap(struct kvm *kvm, gpa_t base, size_t size);

static inline bool vgic_dist_overlap(struct kvm *kvm, gpa_t base, size_t size)
{
	struct vgic_dist *d = &kvm->arch.vgic;

	return (base + size > d->vgic_dist_base) &&
		(base < d->vgic_dist_base + KVM_VGIC_V3_DIST_SIZE);
}

bool vgic_lpis_enabled(struct kvm_vcpu *vcpu);
int vgic_its_resolve_lpi(struct kvm *kvm, struct vgic_its *its,
			 u32 devid, u32 eventid, struct vgic_irq **irq);
struct vgic_its *vgic_msi_to_its(struct kvm *kvm, struct kvm_msi *msi);
int vgic_its_inject_cached_translation(struct kvm *kvm, struct kvm_msi *msi);
void vgic_its_invalidate_all_caches(struct kvm *kvm);

/* GICv4.1 MMIO interface */
int vgic_its_inv_lpi(struct kvm *kvm, struct vgic_irq *irq);
int vgic_its_invall(struct kvm_vcpu *vcpu);

bool system_supports_direct_sgis(void);
bool vgic_supports_direct_msis(struct kvm *kvm);
bool vgic_supports_direct_sgis(struct kvm *kvm);

static inline bool vgic_supports_direct_irqs(struct kvm *kvm)
{
	/*
	 * Deliberately conflate vLPI and vSGI support on GICv4.1 hardware,
	 * indirectly allowing userspace to control whether or not vPEs are
	 * allocated for the VM.
	 */
	if (system_supports_direct_sgis())
		return vgic_supports_direct_sgis(kvm);

	return vgic_supports_direct_msis(kvm);
}

int vgic_v4_init(struct kvm *kvm);
void vgic_v4_teardown(struct kvm *kvm);
void vgic_v4_configure_vsgis(struct kvm *kvm);
void vgic_v4_get_vlpi_state(struct vgic_irq *irq, bool *val);
int vgic_v4_request_vpe_irq(struct kvm_vcpu *vcpu, int irq);

void vcpu_set_ich_hcr(struct kvm_vcpu *vcpu);

static inline bool kvm_has_gicv3(struct kvm *kvm)
{
	return kvm_has_feat(kvm, ID_AA64PFR0_EL1, GIC, IMP);
}

void vgic_v3_sync_nested(struct kvm_vcpu *vcpu);
void vgic_v3_load_nested(struct kvm_vcpu *vcpu);
void vgic_v3_put_nested(struct kvm_vcpu *vcpu);
void vgic_v3_handle_nested_maint_irq(struct kvm_vcpu *vcpu);
void vgic_v3_nested_update_mi(struct kvm_vcpu *vcpu);

static inline bool vgic_is_v3_compat(struct kvm *kvm)
{
	return cpus_have_final_cap(ARM64_HAS_GICV5_CPUIF) &&
		kvm_vgic_global_state.has_gcie_v3_compat;
}

static inline bool vgic_is_v3(struct kvm *kvm)
{
	return kvm_vgic_global_state.type == VGIC_V3 || vgic_is_v3_compat(kvm);
}

int vgic_its_debug_init(struct kvm_device *dev);
void vgic_its_debug_destroy(struct kvm_device *dev);

#endif

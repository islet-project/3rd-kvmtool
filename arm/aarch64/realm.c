#include <linux/list.h>
#include "kvm/kvm.h"
#include "kvm/kvm-cpu.h"
#include "kvm/tpm-event-log.h"
#include "kvm/util.h"

#include "asm/realm.h"

#include "measurement/rim-measure.h"

struct realm_ram_region {
	u64 start;
	u64 file_end;
	enum kvm_image_type image_type;
	void *host_addr;
	struct list_head list;
};

static LIST_HEAD(realm_ram_regions);

#ifndef RIM_MEASURE
struct event_log_vmm_version {
	char	signature[16];
	char	name[32];
	char	version[40];
	__le64	ram_size;
	__le32	num_cpus;
	__le64	flags;
};

struct event_log_tagged {
	__le32	id;
	__le32	data_size;
	u8	data[];
};

#define TAG_REALM_CREATE	1
#define TAG_INIT_RIPAS		2
#define TAG_REC_CREATE		3

#define REALM_PARAMS_FLAG_SVE	(1 << 1)
#define REALM_PARAMS_FLAG_PMU	(1 << 2)

static int realm_log_vmm(struct kvm *kvm)
{
	u64 flags = 0;

	/* EV_NO_ACTION describing this VMM */
	struct event_log_vmm_version vmm_version = {
		.signature = "VM VERSION",
		.name = "kvmtool",
		/* Always smaller than 40 bytes, right? */
		.version = KVMTOOLS_VERSION,
		.ram_size = cpu_to_le64(kvm->ram_size),
		.num_cpus = cpu_to_le32(kvm->nrcpus),
		.flags = cpu_to_le64(flags),
	};

	return tpm_event_log_add(TCG_EV_NO_ACTION, (void *)&vmm_version,
				 sizeof(vmm_version), NULL, 0);
}


static int add_event_tag(u32 id, void *data, size_t data_size)
{
	struct event_log_tagged *event;

	event = calloc(1, sizeof(*event) + data_size);
	if (!event)
		return -ENOMEM;

	event->id = cpu_to_le32(id);
	event->data_size = cpu_to_le32(data_size);
	memcpy(&event->data, data, data_size);
	return tpm_event_log_add(TCG_EV_EVENT_TAG, (void *)event,
				 sizeof(*event) + data_size, NULL, 0);
}

static int realm_log_init_ripas(u64 base, u64 size)
{
	struct {
		__le64 base;
		__le64 size;
	} init_ripas = {
		.base = __cpu_to_le64(base),
		.size = __cpu_to_le64(size),
	};

	return add_event_tag(TAG_INIT_RIPAS, &init_ripas, sizeof(init_ripas));
}

/* REC create for a runnable REC */
void realm_log_rec(struct kvm *kvm, u64 flags, u64 pc, u64 gprs[8])
{
	struct {
		__le64 flags;
		__le64 pc;
		__le64 gprs[8];
	} create_rec = {
		.flags = __cpu_to_le64(flags),
		.pc = __cpu_to_le64(pc),
		.gprs[0] = __cpu_to_le64(gprs[0]),
		.gprs[1] = __cpu_to_le64(gprs[1]),
		.gprs[2] = __cpu_to_le64(gprs[2]),
		.gprs[3] = __cpu_to_le64(gprs[3]),
		.gprs[4] = __cpu_to_le64(gprs[4]),
		.gprs[5] = __cpu_to_le64(gprs[5]),
		.gprs[6] = __cpu_to_le64(gprs[6]),
		.gprs[7] = __cpu_to_le64(gprs[7]),
	};

	if (!kvm->cfg.arch.measurement_log)
		return;

	WARN_ON(add_event_tag(TAG_REC_CREATE, &create_rec, sizeof(create_rec)));
}
#endif

static void realm_configure_hash_algo(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_config_item hash_algo_cfg = {
		.cfg	= KVM_CAP_ARM_RME_CFG_HASH_ALGO,
		.hash_algo = kvm->arch.measurement_algo,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&hash_algo_cfg,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) hash_algo");
#else
		measurer_realm_configure_hash_algo(kvm->arch.measurement_algo);
#endif
}

static void realm_configure_rpv(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_config_item rpv_cfg  = {
		.cfg	= KVM_CAP_ARM_RME_CFG_RPV,
	};

	struct kvm_enable_cap rme_config = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CONFIG_REALM,
		.args[1] = (u64)&rpv_cfg,
	};

	if (kvm->cfg.arch.realm_pv) {
		memset(&rpv_cfg.rpv, 0, sizeof(rpv_cfg.rpv));
		memcpy(&rpv_cfg.rpv, kvm->cfg.arch.realm_pv, strlen(kvm->cfg.arch.realm_pv));
	} else if (kvm->cfg.arch.realm_pv_hex) {
		memset(&rpv_cfg.rpv, 0, sizeof(rpv_cfg.rpv));
		from_hexadecimal_string((__u8*)&rpv_cfg.rpv, kvm->cfg.arch.realm_pv_hex, sizeof(rpv_cfg.rpv));
	} else {
		return;
	}

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_config) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CONFIG_REALM) RPV");
#endif
}

static void realm_configure_parameters(struct kvm *kvm)
{
	realm_configure_hash_algo(kvm);
	realm_configure_rpv(kvm);

#ifdef RIM_MEASURE
	if (kvm->cfg.arch.disable_sve)
		measurer_realm_configure_sve(0);
	else if (kvm->cfg.arch.sve_max_vq > 0)
		measurer_realm_configure_sve(kvm->cfg.arch.sve_max_vq - 1);
	if (kvm->cfg.arch.pmu_cntrs >= 0)
		measurer_realm_configure_pmu(kvm->cfg.arch.pmu_cntrs);
	measurer_realm_configure_num_bps(kvm->cfg.arch.num_bps);
	measurer_realm_configure_num_wps(kvm->cfg.arch.num_wps);

	/* Configure IPA size if specified */
	if (kvm->cfg.arch.ipa_size != 0) {
		measurer_realm_configure_s2sz(kvm->cfg.arch.ipa_size);
	}
#endif
}

static void kvm_arm_realm_create_realm_descriptor(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_enable_cap rme_create_rd = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_CREATE_RD,
	};
#endif

	realm_configure_parameters(kvm);

#ifndef RIM_MEASURE
	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_create_rd) < 0)
		die_perror("KVM_CAP_RME(KVM_CAP_ARM_RME_CREATE_RD)");
#else
	measurer_kvm_arm_realm_create_realm_descriptor();
#endif
}

static void realm_init_ipa_range(struct kvm *kvm, u64 start, u64 size)
{
#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_init_ipa_args init_ipa_args = {
		.init_ipa_base = start,
		.init_ipa_size = size
	};
	struct kvm_enable_cap rme_init_ipa_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_INIT_IPA_REALM,
		.args[1] = (u64)&init_ipa_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_init_ipa_realm) < 0)
		die("unable to intialise IPA range for Realm %llx - %llx (size %llu)",
		    start, start + size, size);
	pr_debug("Initialized IPA range (%llx - %llx) as RAM\n",
		start, start + size);

	if (kvm->cfg.arch.measurement_log)
		WARN_ON(realm_log_init_ripas(start, size));
#else
	measurer_realm_init_ipa_range(start, start + size);
#endif
}

static void __realm_populate(struct kvm *kvm, u64 start, u64 size, bool measured)
{
	u64 align_start = ALIGN_DOWN(start, SZ_4K);
	u64 align_end = ALIGN(start + size, SZ_4K);

#ifndef RIM_MEASURE
	struct kvm_cap_arm_rme_populate_realm_args populate_args = {
		.populate_ipa_base = align_start,
		.populate_ipa_size = align_end - align_start,
		.flags		   = measured ? KVM_ARM_RME_POPULATE_FLAGS_MEASURE : 0,
	};
	struct kvm_enable_cap rme_populate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_POPULATE_REALM,
		.args[1] = (u64)&populate_args
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_populate_realm) < 0)
		die("unable to populate Realm memory %llx - %llx (size %llu)",
		    start, start + size, size);
	pr_debug("Populated Realm memory area : %llx - %llx (size %llu bytes)",
		start, start + size, size);
#else
	measurer_realm_populate(kvm, align_start, align_end);
#endif
}

static void realm_populate(struct kvm *kvm, struct realm_ram_region *region)
{
	__realm_populate(kvm, region->start,
			 region->file_end - region->start,
			 /* measured */ true);

#ifndef RIM_MEASURE
	if (!kvm->cfg.arch.measurement_log)
		return;

	WARN_ON(tpm_event_log_add_image(region->image_type, region->host_addr,
					region->start, region->file_end -
					region->start));
#endif
}

void kvm_arm_realm_populate_ram(struct kvm *kvm, void *host_addr,
				unsigned long start, unsigned long file_size,
				enum kvm_image_type image_type)
{
	struct realm_ram_region *new_region, *next;

	new_region = calloc(1, sizeof(*new_region));
	if (!new_region)
		die("cannot allocate realm RAM region");

	new_region->start = start;
	new_region->file_end = start + file_size;
	new_region->image_type = image_type;
	new_region->host_addr = host_addr;

	/* Keep the list sorted */
	list_for_each_entry(next, &realm_ram_regions, list) {
		if (next->start > new_region->start)
			break;
	}
	list_add_tail(&new_region->list, &next->list);
}

void kvm_arm_realm_populate_metadata(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	if (kvm->arch.metadata == NULL)
		return;

	struct kvm_enable_cap rme_populate_metadata = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_POPULATE_METADATA,
		.args[1] = (u64)kvm->arch.metadata
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &rme_populate_metadata) < 0)
		die("unable to populate the realm metadata %p",
		    kvm->arch.metadata);

	pr_debug("Realm metadata has been populated\n");
#endif
}

static void kvm_arm_realm_activate_realm(struct kvm *kvm)
{
#ifndef RIM_MEASURE
	struct kvm_enable_cap activate_realm = {
		.cap = KVM_CAP_ARM_RME,
		.args[0] = KVM_CAP_ARM_RME_ACTIVATE_REALM,
	};

	if (ioctl(kvm->vm_fd, KVM_ENABLE_CAP, &activate_realm) < 0)
		die_perror("KVM_CAP_ARM_RME(KVM_CAP_ARM_RME_ACTIVATE_REALM)");

	kvm->arch.realm_is_active = true;
#else
	measurer_print_rim();
	exit(0);
#endif
}

#ifndef RIM_MEASURE
static int kvm_arm_log_params(struct kvm *kvm)
{
	int ret;
	struct {
		__le64 flags;
		u8 s2sz;
		u8 sve_vl;
		u8 num_bps;
		u8 num_wps;
		u8 pmu_num_ctrs;
		u8 hash_algo;
	} params = {
		.s2sz = kvm->arch.ipa_bits,
		.num_bps = 5, /* FIXME: obtain the default values from KVM */
		.num_wps = 3,
		.pmu_num_ctrs = 6,
		.hash_algo = kvm->arch.measurement_algo,
	};

	if (!kvm->cfg.arch.measurement_log)
		return 0;

	ret = realm_log_vmm(kvm);
	if (ret)
		return ret;

	if (!kvm->cfg.arch.disable_sve) {
		/* check this: is VQ param still passed to RMM with !SVE? */
		params.sve_vl = kvm->cfg.arch.sve_max_vq - 1;
		params.flags |= cpu_to_le64(REALM_PARAMS_FLAG_SVE);
	}

	if (kvm->cfg.arch.has_pmuv3) { // FIXME: get from KVM
		if (kvm->cfg.arch.pmu_cntrs >= 0)
			params.pmu_num_ctrs = kvm->cfg.arch.pmu_cntrs;
		params.flags |= cpu_to_le64(REALM_PARAMS_FLAG_PMU);
	}

	return add_event_tag(TAG_REALM_CREATE, &params, sizeof(params));
}
#endif

static int kvm_arm_realm_finalize(struct kvm *kvm)
{
	int i;
	struct realm_ram_region *region, *next;

	if (!kvm__is_realm(kvm))
		return 0;

	kvm_arm_realm_create_realm_descriptor(kvm);
#ifndef RIM_MEASURE
	WARN_ON(kvm_arm_log_params(kvm));
#endif
	kvm_arm_realm_populate_metadata(kvm);

	realm_init_ipa_range(kvm, kvm->arch.memory_guest_start, kvm->ram_size);

	list_for_each_entry_safe(region, next, &realm_ram_regions, list) {
		realm_populate(kvm, region);
		list_del(&region->list);
		free(region);
	}

	/*
	 * VCPU reset must happen before the realm is activated, because their
	 * state is part of the cryptographic measurement for the realm.
	 */
	for (i = 0; i < kvm->nrcpus; i++)
		kvm_cpu__reset_vcpu(kvm->cpus[i]);


#ifndef RIM_MEASURE
	if (kvm->cfg.arch.measurement_log) {
		WARN_ON(tpm_event_log_add_image(KVM_IMAGE_TYPE_EVENT_LOG, NULL,
						kvm->arch.event_log_guest_start,
						EVENT_LOG_MAX_SIZE));
		/* This seals the log: future modifications aren't copied into guest mem */
		__realm_populate(kvm, kvm->arch.event_log_guest_start,
				 EVENT_LOG_MAX_SIZE,
				 /* measured */ false);
	}
#endif
	/* Activate and seal the measurement for the realm. */
	kvm_arm_realm_activate_realm(kvm);

	return 0;
}
last_init(kvm_arm_realm_finalize)

/*
* SPDX-License-Identifier: BSD-3-Clause
* SPDX-FileCopyrightText: Copyright TF-RMM Contributors.
*
* The code has been borrowed from TF-RMM (v1.0-Beta0 RMM specification).
*/

#include <string.h>

#include "measurement/measurement.h"
#include "measurement/rim-measure.h"


static enum hash_algo measurer_hash_algo;
static unsigned char rim[MAX_MEASUREMENT_SIZE];
//uint64_t block_sizes;

/**
 * These are the initial values passed by KVM for Islet RMM (main) running on Qemu RME.
 * These values depend on the content of RMM's feature0 register (RMI_FEATURES).
 * Note that TF-RMM and Islet running on different platforms can use different values of parameters.
 */
static struct rmi_realm_params realm_params = {
	.flags = RMI_REALM_PARAM_FLAG_SVE,
	.s2sz = 0x21, /* Maximum IPA size: 8GB set by host kernel (rme.c: params->s2sz = VTCR_EL2_IPA(kvm->arch.mmu.vtcr))*/
	.num_bps = 0u,
	.num_wps = 0u,
	.pmu_num_ctrs = 0,
	.sve_vl = 3u,
};

static void ripas_granule_measure(unsigned long base,
				  				  unsigned long top)
{
	struct measurement_desc_ripas measure_desc = {0};

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_RIPAS;
	measure_desc.len = sizeof(struct measurement_desc_ripas);
	measure_desc.base = base;
	measure_desc.top = top;
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
				 &measure_desc,
				 sizeof(measure_desc),
				 rim);
}

static void data_granule_measure(void *data,
				 unsigned long ipa,
				 unsigned long flags)
{
	struct measurement_desc_data measure_desc = {0};

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_DATA;
	measure_desc.len = sizeof(struct measurement_desc_data);
	measure_desc.ipa = ipa;
	measure_desc.flags = flags;
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	if (flags == RMI_MEASURE_CONTENT) {
		/*
		 * Hashing the data granules and store the result in the
		 * measurement descriptor structure.
		 */
		measurement_hash_compute(measurer_hash_algo,
					data,
					GRANULE_SIZE,
					measure_desc.content);
	}

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
			       &measure_desc,
			       sizeof(measure_desc),
			       rim);
}

static void rec_params_measure(unsigned long pc, unsigned long flags, unsigned long gprs[REC_CREATE_NR_GPRS])
{
	struct measurement_desc_rec measure_desc = {0};
	struct rmi_rec_params rec_params_measured;

	memset(&rec_params_measured, 0, sizeof(rec_params_measured));

	/* Copy the relevant parts of the rmi_rec_params structure to be
	 * measured
	 */
	rec_params_measured.pc = pc;
	rec_params_measured.flags = flags;
	memcpy(&rec_params_measured.gprs,
	       gprs,
	       sizeof(rec_params_measured.gprs));

	/* Initialize the measurement descriptior structure */
	measure_desc.desc_type = MEASURE_DESC_TYPE_REC;
	measure_desc.len = sizeof(struct measurement_desc_rec);
	memcpy(measure_desc.rim,
	       rim,
	       measurement_get_size(measurer_hash_algo));

	/*
	 * Hashing the REC params structure and store the result in the
	 * measurement descriptor structure.
	 */
	measurement_hash_compute(measurer_hash_algo,
				&rec_params_measured,
				sizeof(rec_params_measured),
				measure_desc.content);

	/*
	 * Hashing the measurement descriptor structure; the result is the
	 * updated RIM.
	 */
	measurement_hash_compute(measurer_hash_algo,
			       &measure_desc,
			       sizeof(measure_desc),
			       rim);
}

/**
 * For the purposes of handling measurements related to RTT_INIT_RIPAS RMI we need
 * to simulate an initial mapping of S2 translation tables. For this purposes
 * we have borrowed some macros related to stage-2 translation tables from TF-RMM.
 */
#define S2TT_LEVEL0 (0)
#define S2TT_LEVEL1 (1)
#define S2TT_LEVEL2 (2)
#define S2TT_PAGE_LEVEL			(3)
#define S2TT_LEVELS_COUNT (4)

#define S2TTE_16_ELEMS_BIT	(4U)
#define GRANULE_SHIFT 12
#define U(_x) (unsigned int)(_x)

/*
 * S2TTE_STRIDE: The number of bits resolved in a single level of translation
 * walk (except for the starting level which may resolve more or fewer bits).
 */
#define S2TTE_STRIDE		(U(GRANULE_SHIFT) - 3U)
#define S2TTES_PER_S2TT		(1UL << S2TTE_STRIDE)

/***************************************************************************
 * Helpers for Stage 2 Translation Table Entries (S2TTE).
 **************************************************************************/
#define s2tte_map_size(level)						\
	(1ULL << (unsigned int)(((S2TT_PAGE_LEVEL - (level)) *		\
				(int)S2TTE_STRIDE) + (int)GRANULE_SHIFT))

static uint8_t max_num_levels(uint8_t ipa_bits)
{
    uint8_t indexes_bits = ipa_bits - GRANULE_SHIFT;
    uint8_t num_levels = (indexes_bits + S2TTE_STRIDE - 1) / S2TTE_STRIDE;

    // Use concatenated tables if the top-level table contains <= 16 entries
    uint8_t top_level_entries_bits = indexes_bits % S2TTE_STRIDE;
    if (top_level_entries_bits > 0 && top_level_entries_bits <= S2TTE_16_ELEMS_BIT)
         num_levels -= 1;

    return num_levels;
}

static const uint64_t s2tt_map_sizes[S2TT_LEVELS_COUNT] = {
    s2tte_map_size(S2TT_LEVEL0),
    s2tte_map_size(S2TT_LEVEL1),
    s2tte_map_size(S2TT_LEVEL2),
    s2tte_map_size(S2TT_PAGE_LEVEL)
};

static uint64_t find_map_size(uint64_t bottom, uint64_t top, uint8_t num_levels)
{
  uint64_t range = top - bottom;

  for (unsigned int idx = S2TT_LEVELS_COUNT - num_levels; idx < S2TT_LEVELS_COUNT; idx++) {
    uint64_t map_size = s2tt_map_sizes[idx];

    if (bottom != 0 && !IS_ALIGNED(bottom, map_size))
      continue;

	// Find the largest block that can be used to map the range
    if (range >= map_size)
      return map_size;
  }

  // Should never be executed
  assert(0);
  return 0;
}

void measurer_realm_init_ipa_range(u64 start, u64 end)
{
	uint8_t num_levels = max_num_levels(realm_params.s2sz);
	u64 begin = start;
	while (begin < end) {
		uint64_t block_size = find_map_size(begin, end, num_levels);
		ripas_granule_measure(begin, begin + block_size);
		begin += block_size;
	}
}

void measurer_realm_populate(struct kvm *kvm, u64 start, u64 end)
{
	void *data_start = guest_flat_to_host(kvm, start);
	void *data_end = guest_flat_to_host(kvm, end);
	void *data;
	u64 ipa;

	for (data = data_start, ipa = start; data < data_end; data += SZ_4K, ipa += SZ_4K) {
		data_granule_measure(data, ipa, RMI_MEASURE_CONTENT);
	}
}

void measurer_realm_configure_hash_algo(uint64_t hash_algo)
{
	switch (hash_algo) {
		case KVM_CAP_ARM_RME_MEASUREMENT_ALGO_SHA256:
			measurer_hash_algo = HASH_ALGO_SHA256;
			break;
		case KVM_CAP_ARM_RME_MEASUREMENT_ALGO_SHA512:
			measurer_hash_algo = HASH_ALGO_SHA512;
			break;
	}
}

void measurer_realm_configure_sve(uint32_t sve_vq)
{
	realm_params.sve_vl = sve_vq;
	if (sve_vq != 0)
		realm_params.flags |= RMI_REALM_PARAM_FLAG_SVE;
	else
		realm_params.flags &= ~RMI_REALM_PARAM_FLAG_SVE;
}

void measurer_realm_configure_pmu(uint32_t num_pmu_cntrs)
{
	realm_params.pmu_num_ctrs = num_pmu_cntrs;
	if (num_pmu_cntrs != 0)
		realm_params.flags |= RMI_REALM_PARAM_FLAG_PMU;
	else
		realm_params.flags &= ~RMI_REALM_PARAM_FLAG_PMU;
}

void measurer_realm_configure_num_bps(uint32_t num_bps)
{
	realm_params.num_bps = num_bps;
}

void measurer_realm_configure_num_wps(uint32_t num_wps)
{
	realm_params.num_wps = num_wps;
}

void measurer_realm_configure_s2sz(uint32_t s2sz)
{
	realm_params.s2sz = s2sz;
}

static void realm_params_measure(void)
{
	/*
	 * Allocate a zero-filled RmiRealmParams data structure
	 * to hold the measured Realm parameters.
	 */
	unsigned char buffer[sizeof(struct rmi_realm_params)] = {0};
	struct rmi_realm_params *rim_params = (struct rmi_realm_params *)buffer;

	/*
	 * Copy the following attributes into the measured Realm
	 * parameters data structure:
	 * - flags
	 * - s2sz
	 * - sve_vl
	 * - num_bps
	 * - num_wps
	 * - pmu_num_ctrs
	 * - hash_algo
	 */
	rim_params->flags = realm_params.flags;
	rim_params->s2sz = realm_params.s2sz;
	rim_params->sve_vl = realm_params.sve_vl;
	rim_params->num_bps = realm_params.num_bps;
	rim_params->num_wps = realm_params.num_wps;
	rim_params->pmu_num_ctrs = realm_params.pmu_num_ctrs;
	rim_params->algorithm = measurer_hash_algo;

	printf("RmiRealmParams\n");
	printf("flags:\t%08lx\n", rim_params->flags);
	printf("s2sz:\t%u (0x%08x)\n", rim_params->s2sz, rim_params->s2sz);
	printf("sve_vl:\t%u (0x%08x)\n", rim_params->sve_vl, rim_params->sve_vl);
	printf("num_bps:\t%u (0x%08x)\n", rim_params->num_bps, rim_params->num_bps);
	printf("num_wps:\t%u (0x%08x)\n", rim_params->num_wps, rim_params->num_wps);
	printf("pmu_num_ctrs:\t%u (0x%08x)\n", rim_params->pmu_num_ctrs, rim_params->pmu_num_ctrs);
	printf("algorithm:\t%u (0x%08x)\n", rim_params->algorithm, rim_params->algorithm);

	/* Measure relevant realm params this will be the init value of RIM */
	measurement_hash_compute(measurer_hash_algo,
			       buffer,
			       sizeof(buffer),
			       rim);
}

void measurer_kvm_arm_realm_create_realm_descriptor(void)
{
	realm_params_measure();
}

void measurer_reset_vcpu_aarch64(u64 pc, u64 flags, u64 dtb)
{
	unsigned long gprs[8] = {0,};
	gprs[0] = dtb;

	rec_params_measure(pc, flags, gprs);
}

void measurer_print_rim(void)
{
	size_t i, rim_size;

	rim_size = measurement_get_size(measurer_hash_algo);

	printf("RIM: ");
	for (i = 0; i < rim_size; i++) {
		printf("%02X", rim[i]);
	}
	printf("\n");
}

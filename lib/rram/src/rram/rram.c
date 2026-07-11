#include "rram/rram.h"

#include <zephyr/kernel.h>
#include <hal/nrf_rramc.h>

#define RRAM_WORD_SIZE			16
#define RRAM_BUFFER_WORD_SIZE	32	// 32 Words of write-cache.

static void rram_check_bounds(
	const struct rram_ctx_t *ctx, volatile void *dst, size_t size
);
static void rram_disable_protection();
static void rram_enable_protection();
static void rram_raw_memcpy(volatile void *dst, const volatile void *src, size_t size);

/*
 *
 * Interface definitions.
 *
 */

void rram_memcpy(
	const struct rram_ctx_t *ctx,
	volatile void *dst, const volatile void *src, size_t size
) {
	rram_check_bounds(ctx, dst, size);

	rram_disable_protection();
	rram_raw_memcpy(dst, src, size);
	rram_enable_protection();
}

void rram_memcpy_no_irq(
	const struct rram_ctx_t *ctx,
	volatile void *dst, const volatile void *src, size_t size
) {
	rram_check_bounds(ctx, dst, size);

	unsigned int key = irq_lock();
	rram_disable_protection();
	rram_raw_memcpy(dst, src, size);
	rram_enable_protection();
	irq_unlock(key);
}

/*
 *
 * Internal functions.
 *
 */

static void rram_check_bounds(
	const struct rram_ctx_t *ctx, volatile void *dst_v, size_t size
) {
	volatile char *dst = dst_v;

	if(dst < ctx->start || dst + size > ctx->end) {
		k_panic();
	}
}

/*
 * Disable protection and enable buffering.
 */
static void rram_disable_protection() {
	nrf_rramc_config_t config_unprotect = {
        .mode_write = true,
        .write_buff_size = RRAM_BUFFER_WORD_SIZE
    };
	nrf_rramc_config_set(NRF_RRAMC_S, &config_unprotect);
}

/*
 * Enable protection and disable buffering.
 */
static void rram_enable_protection() {
	nrf_rramc_config_t config_protect = {
        .mode_write = false,
        .write_buff_size = 0,
    };

	nrf_rramc_config_set(NRF_RRAMC_S, &config_protect);
}

/*
 * Copy bytes and force a buffer flush.
 */
static void rram_raw_memcpy(volatile void *dst_v, const volatile void *src_v, size_t size) {
	// memcpy does not support volatile pointers.

	uintptr_t dst_ptr = (uintptr_t) dst_v;
	uintptr_t src_ptr = (uintptr_t) src_v;

	// Copy extra bytes one by one until we reach a natural alignment.
	// This is fine for endurance, because we're writing to the buffer.
	for(; dst_ptr % sizeof(uint32_t) && size; dst_ptr++, src_ptr++, size--) {
		volatile char *dst = (volatile char*) dst_ptr;
		volatile char *src = (volatile char*) src_ptr;

		*dst = *src;
	}

	// Copy 4 bytes at a time.
	// This is fine for endurance, because we're writing to the buffer.
	for(; size >= sizeof(uint32_t); dst_ptr += sizeof(uint32_t), src_ptr += sizeof(uint32_t), size -= sizeof(uint32_t)) {
		volatile uint32_t *dst = (volatile uint32_t*) dst_ptr;
		volatile uint32_t *src = (volatile uint32_t*) src_ptr;

		*dst = *src;
	}

	// Copy the remaining bytes.
	for(; size; dst_ptr++, src_ptr++, size--) {
		volatile char *dst = (volatile char*) dst_ptr;
		volatile char *src = (volatile char*) src_ptr;

		*dst = *src;
	}

    // Force committing to RRAM.
    nrf_rramc_task_trigger(NRF_RRAMC_S, NRF_RRAMC_TASK_COMMIT_WRITEBUF);
}

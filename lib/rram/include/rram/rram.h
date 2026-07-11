/*!
 * @file rram.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Provides interfaces to safely write to the device's RRAM.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_RRAM_RRAM_H_
#define SPV_RRAM_RRAM_H_

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

/**
 * @brief Stores configuration about a single RRAM region.
 *
 * Reads from this region can use raw pointer access. Writes should stick
 * to using the `rram_memcpy_*` functions.
 *
 * Prefer doing 16-Byte aligned writes for hardware efficiency and RRAM
 * endurance.
 */
struct rram_ctx_t {
	/**
	 * @brief Pointer to start of the region.
	 */
	volatile char *start;
	/**
	 * @brief Pointer to the end of the region (exclusive).
	 */
	volatile char *end;
};

/**
 * @brief Statically defines an RRAM context.
 */
#define RRAM_CTX_DEF_FROM_RAW(_name, _start, _end) 								\
	struct rram_ctx_t _name = {													\
		.start = _start, .end = _end,											\
	}

/**
 * @brief Statically creates an RRAM context from a partition.
 */
#define RRAM_CTX_DEF_FROM_PART(_name, _partition)								\
	struct rram_ctx_t _name = {													\
		.start = (volatile void*) FIXED_PARTITION_ADDRESS(_partition),					\
		.end = (volatile void*) (FIXED_PARTITION_ADDRESS(_partition) + FIXED_PARTITION_SIZE(_partition))\
	};

/**
 * @brief Copies `size` bytes from `src` to `dst`, where `dst` is an RRAM
 * address and `src` may either be an RRAM or SRAM address.
 *
 * This function will disable RRAM write protection temporarily. For maximum
 * security, use `rram_memcpy_no_irq` if possible.
 *
 * @param[in] ctx RRAM context.
 * @param[out] dst Destination to write to.
 * @param[in] src Source to read from.
 * @param[in] size How many bytes to write.
 *
 * @warning This function will panic if `dst` is outside the allocated memory
 * region or `dst + size` overflows the reserved space.
 *
 * @warning This function performs a buffered write, even if this function
 * returns, data may not have been saved completely. Powering off the memory
 * controller may corrupt data that's still in flight. Zephyr will
 * automatically handle this, avoiding sleeping until the write completes. Avoid
 * forcing sleep modes manually unless you check the RRAM buffer status yourself.
 */
[[gnu::nonnull(1, 2, 3)]]
void rram_memcpy(
	const struct rram_ctx_t *ctx,
	volatile void *dst, const volatile void *src, size_t size
);

/**
 * @brief Copies `size` bytes from `src` to `dst`, where `dst` is an RRAM
 * address and `src` may either be an RRAM or SRAM address.
 *
 * @param[in] ctx RRAM context.
 * @param[out] dst Destination to write to.
 * @param[in] src Source to read from.
 * @param[in] size How many bytes to write.
 *
 * @warning This function will panic if `dst` is outside the allocated memory
 * region or `dst + size` overflows the reserved space.
 *
 * @warning This function performs a buffered write, even if this function
 * returns, data may not have been saved completely. Powering off the memory
 * controller may corrupt data that's still in flight. Zephyr will
 * automatically handle this, avoiding sleeping until the write completes. Avoid
 * forcing sleep modes manually unless you check the RRAM buffer status yourself.
 *
 * @note This function acts the same as `rram_memcpy`, but disabling IRQs. Do
 * not use this function for long copies.
 */
[[gnu::nonnull(1, 2, 3)]]
void rram_memcpy_no_irq(
	const struct rram_ctx_t *ctx,
	volatile void *dst, const volatile void *src, size_t size
);

#endif

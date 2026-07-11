/*!
 * @file factory_data.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Interfaces for reading the device's factory data partition.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_FACTORYDATA_FACTORYDATA_H_
#define SPV_FACTORYDATA_FACTORYDATA_H_

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief Stores information burned during manufacturing.
 */
struct factorydata_t {

	/**
	 * @brief UNIX timestamp of the board's manufacturing date.
	 */
	uint64_t manufacturing_date;

	/**
	 * @brief Contains the hardware revision.
	 */
	const char *board_revision;

	/**
	 * Board serial number.
	 */
	const char *serial_number;

	/**
	 * @brief Pointer to the device's unique certificate.
	 */
	const char *device_cert;
	/**
	 * @brief Size of `device_cert_size` in bytes.
	 */
	size_t device_cert_size;
};

/**
 * @brief Loads factory data from Flash. Should only be called once.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int factorydata_load();

/**
 * @brief Fetches factory data. Calling `factorydata_load` before this is
 * required.
 *
 * @return struct factorydata_t Factory data information pointing to global
 * singleton.
 */
[[nodiscard]]
struct factorydata_t factorydata_get();

#endif

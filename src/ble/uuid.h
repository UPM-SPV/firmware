/*!
 * @file uuid.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Project-specific UUIDs.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BLE_UUID_H_
#define SPV_BLE_UUID_H_

#include <bluetooth/conn_ctx.h>

/**
 * @brief Encodes a 16-bit project-specific UUID into a 128-bit UUID.
 */
#define BT_UUID_SPV_ENCODE(_uuid_16bit) \
	BT_UUID_128_ENCODE(0x3C150000 | _uuid_16bit, 0x9311, 0x4A07, 0x8520, 0xA817462A9F16)

/**
 * @brief Declares a 16-bit project-specific UUID into a 128-bit UUID.
 */
#define BT_UUID_SPV_DECLARE(_uuid_16bit) \
	BT_UUID_DECLARE_128(BT_UUID_SPV_ENCODE(_uuid_16bit))

#endif

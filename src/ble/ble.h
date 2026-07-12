/*!
 * @file ble.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Peripheral BLE management.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BLE_BLE_H_
#define SPV_BLE_BLE_H_

/**
 * @brief Starts the BLE stack synchronously.
 *
 * @warning This function is not reentrant.
 *
 * @return int Error code. 0 when successful, error code otherwise.
 */
[[nodiscard]]
int ble_start();

#endif

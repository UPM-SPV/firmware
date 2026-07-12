/*!
 * @file advertise.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Advertisement management.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BLE_ADVERTISE_H_
#define SPV_BLE_ADVERTISE_H_


/**
 * @brief Starts BLE advertisements.
 */
void ble_advertising_start();

/**
 * @brief Opens the device's pairing list.
 */
void ble_enable_pairing();

/**
 * @brief Disables BLE pairing.
 */
void ble_disable_pairing();

#endif

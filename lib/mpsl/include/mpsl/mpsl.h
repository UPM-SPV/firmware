/*!
 * @file mpsl.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Radio activity event subscriptions.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef SPV_MPSL_MPSL_H_
#define SPV_MPSL_MPSL_H_

#include <zephyr/kernel.h>

/**
 * @brief Callback for MPSL radio activity callbacks. See `MPSL_CB_DEF`.
 */
typedef void (*mpsl_cb_func_t)(void);

/**
 * @brief Struct for storing MPSL callbacks. Should be declared using
 * `MPSL_CB_DEF`.
 */
struct mpsl_cb_t {
    /**
     * @brief Called before the radio is enabled. May be `NULL`.
     */
    const mpsl_cb_func_t on_active;

	/**
	 * @brief Called after the radio is disabled. May be `NULL`.
	 */
	const mpsl_cb_func_t on_inactive;
};

/**
 * @brief Subscribes the specified callback to MPSL radio status updates.
 *
 *
 */
#define MPSL_CB_DEF(_name) \
	STRUCT_SECTION_ITERABLE(mpsl_cb_t, _name)


/**
 * @brief Initializes the radio callback subsystem.
 *
 * @return 0 if successful.
 *
 * ## Implementation details.
 *
 * Depending on the defined callbacks, the following underlying
 * events will be enabled:
 *
 * - No callbacks have `on_active`: `MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE`
 * 	will be used.
 *
 * - No callbacks have `on_inactive`: `MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE`
 * 	will be used.
 *
 * - At least one `on_active` and one `on_inactive` callbacks are defined:
 * 	`MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH` will be used.
 *
 * If no callbacks are defined this function has no effect. Radio user
 * interrupts will remain disabled.
 */
int mpsl_start();

#endif

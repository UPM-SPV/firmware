/*!
 * @file config.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Stores project-specific settings.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_CONFIG_CONFIG_H_
#define SPV_CONFIG_CONFIG_H_

#include <stdint.h>
#include <time.h>

#include "sense/sense.h"

/**
 * @brief Stores all project settings.
 */
struct settings_spv_t {
	/**
	 * @brief Sampling period of the sensor package.
	 */
	time_t sensor_sampling_rate;

	/**
	 * @brief Enabled sensors.
	 */
	sense_enabled_t enabled_sensors;
};

/**
 * @brief Callback for settings updates. See `SETTINGS_CB_DEF`.
 */
[[gnu::nonnull(1)]]
typedef void (*settings_spv_cb_update_t)(const struct settings_spv_t*);

/**
 * @brief Struct for storing settings update callbacks. Should be declared using
 * `SETTINGS_CB_DEF`.
 */
struct settings_cb_t {
    /**
     * @brief Called when any of the `settings_spv_set_*` functions are called.
	 * May be NULL.
     */
    settings_spv_cb_update_t on_settings_update;
};

/**
 * @brief Subscribes the specified callback to settings updates.
 */
#define SETTINGS_CB_DEF(_name) \
	STRUCT_SECTION_ITERABLE(settings_cb_t, _name)


/**
 * @brief Saves the specified settings.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int settings_spv_set(const struct settings_spv_t *settings);


/**
 * @brief Tries to load settings from NVS.
 *
 * @param[out] settings Pointer to loaded settings
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int settings_spv_load();

/**
 * @brief Obtains a copy of the current settings.
 *
 * @param[out] settings Pointer to copied settings.
 *
 * @note This function requires initializing the settings subsystem. This
 * function will never fail.
 */
[[nodiscard, gnu::nonnull(1)]]
int settings_spv_get(struct settings_spv_t *settings);

#endif

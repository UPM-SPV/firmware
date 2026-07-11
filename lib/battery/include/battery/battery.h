/*!
 * @file battery.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Provides interfaces to read the device's battery level.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BATTERY_BATTERY_H_
#define SPV_BATTERY_BATTERY_H_

#include <stdint.h>


/**
 * @brief Battery voltage level in millivolts.
 */
typedef uint16_t battery_mv_t;

/**
 * @brief Battery level as a percentage of total capacity, from 0 to 100.
 *
 * Battery levels below and above the configured values are capped to 0 and
 * 100 respectively.
 */
typedef uint8_t battery_level_t;

/**
 * @brief Used to specify whether the battery is being charged.
 *
 * @note Currently, only `BATTERY_STATE_DISCHARGING` is used.
 */
enum battery_charging_state_t {
	/**
	 * @brief The device is operating under battery
	 */
	BATTERY_STATE_DISCHARGING,
	/**
	 * @brief The device is being charged.
	 */
	BATTERY_STATE_CHARGING,
};

/**
 * @brief Configures battery parameters required for estimating battery life.
 */
struct battery_config_t {
	/**
	 * @brief Expected battery voltage when full. Must be greater than
	 * `battery_mv_depleted`.
	 */
	battery_mv_t battery_mv_full;
	/**
	 * @brief Expected battery voltage when depleted.
	 */
	battery_mv_t battery_mv_depleted;
};

/**
 * @brief Summarizes the current battery state.
 */
struct battery_status_t {
	/**
	 * @brief Current battery voltage in millivolts.
	 */
	battery_mv_t mv;
	/**
	 * @brief Battery percentage.
	 */
	battery_level_t level;

	/**
	 * @brief Specifies if the battery is being charged.
	 */
	enum battery_charging_state_t charging_state;
};


/**
 * @brief Configures the battery module.
 *
 * Required for using any battery-related functions.
 *
 * @param[in] config Battery configuration.
 * @return 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int battery_configure(const struct battery_config_t *config);

/**
 * @brief Reads all battery parameters.
 *
 * @param[out] status Battery state summary.
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int battery_status_get(struct battery_status_t *status);

/**
 * @brief Reads the battery voltage.
 *
 * @param[out] mv Battery voltage.
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int battery_voltage_get(battery_mv_t *mv);

/**
 * @brief Reads the battery percentage.
 *
 * @param[out] level Battery level.
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int battery_level_get(battery_level_t *level);

/**
 * @brief Returns whether the battery is currently charging.
 *
 * @param[out] state Charging or discharging.
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int battery_charging_state_get(enum battery_charging_state_t *state);

#endif

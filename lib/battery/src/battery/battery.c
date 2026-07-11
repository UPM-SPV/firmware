/*!
 * @file battery.c
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Implementation for `battery.h`.
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "battery/battery.h"

#include <errno.h>
#include <memory.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(battery, LOG_LEVEL_DBG);

/**
 * @brief Device tree reference to the ADC channel connected to VDD.
 */
static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
/**
 * @brief Locks concurrent access to the battery API.
 */
static K_MUTEX_DEFINE(battery_lock);

/**
 * @brief Global battery configuration. Set by `battery_configure()`.
 */
static struct battery_config_t battery_config = { 0 };

static bool battery_is_configured();
static bool battery_is_configured_log();
static int adc_configure();
[[gnu::nonnull(1)]]
static int adc_get_blocking(int16_t *adc_value);
[[gnu::nonnull(1)]]
static int adc_to_mv(battery_mv_t *voltage, const int16_t adc_value);
[[gnu::nonnull(1)]]
static void battery_mv_to_level(battery_level_t *level, const battery_mv_t voltage);


/*
 *
 * Interface definition.
 *
 */

/**
 * Stores the given configuration in the global `battery_config` variable and
 * initializes the ADC.
 */
int battery_configure(const struct battery_config_t *config) {

	int err;

	if(config->battery_mv_depleted >= config->battery_mv_full) {
		LOG_ERR(
			"Configuring battery: `battery_mv_full` (%"PRIu32") "
			"must be greater than batery_mv_depleted (%"PRIu32")",
			config->battery_mv_full, config->battery_mv_depleted
		);
		return -EINVAL;
	}


	err = k_mutex_lock(&battery_lock, K_FOREVER);
	if(err < 0) {
		LOG_ERR("Configuring battery: error locking mutex (%s)", strerror(err));
		return err;
	}

	if(!battery_is_configured()) {
		err = adc_configure();
		if(err < 0) {
			LOG_ERR("Configuring battery: error setting up ADC (%s)", strerror(err));
			goto error_free_lock;
		}
	} else {
		LOG_WRN("Reconfiguring battery module");
	}

	memcpy(&battery_config, config, sizeof(battery_config));

	LOG_INF("Battery configured:");
	LOG_INF("	Maximum (mV):\t%"PRIu32, battery_config.battery_mv_full);
	LOG_INF("	Minimum (mV):\t%"PRIu32, battery_config.battery_mv_depleted);

	k_mutex_unlock(&battery_lock);
	return 0;


	error_free_lock:
	k_mutex_unlock(&battery_lock);
	return err;
}


/**
 * Reads an ADC value and converts it to voltage.
 */
int battery_voltage_get(battery_mv_t *mv) {

	__ASSERT(battery_is_configured(), "Battery not configured");

	int err;
	err = k_mutex_lock(&battery_lock, K_FOREVER);
	if(err < 0) {
		LOG_ERR("Reading voltage: error locking mutex (%s)", strerror(err));
		return err;
	}

	if(!battery_is_configured_log()) {
		err = -EINVAL;
		goto error_free_lock;
	}

	int16_t raw_adc = 0;
	err = adc_get_blocking(&raw_adc);
	if(err < 0) {
		LOG_ERR("Reading voltage: error reading ADC (%s)", strerror(err));
		goto error_free_lock;
	}

	err = adc_to_mv(mv, raw_adc);
	if(err < 0) {
		LOG_ERR("Reading voltage: error converting to voltage (%s)", strerror(err));
		goto error_free_lock;
	}

	k_mutex_unlock(&battery_lock);
	return 0;


	error_free_lock:
	k_mutex_unlock(&battery_lock);
	return err;
}

/**
 * Reads the battery voltage and converts it to a percentage.
 */
int battery_level_get(battery_level_t *level) {

	__ASSERT(battery_is_configured(), "Battery not configured");

	int err;
	err = k_mutex_lock(&battery_lock, K_FOREVER);
	if(err < 0) {
		LOG_ERR("Reading level: error locking mutex (%s)", strerror(err));
		return err;
	}

	if(!battery_is_configured_log()) {
		err = -EINVAL;
		goto error_free_lock;
	}

	battery_mv_t voltage;
	err = battery_voltage_get(&voltage);
	if(err < 0) {
		LOG_ERR("Reading level: error reading voltage (%s)", strerror(err));
		goto error_free_lock;
	}

	battery_mv_to_level(level, voltage);

	k_mutex_unlock(&battery_lock);
	return 0;


	error_free_lock:
	k_mutex_unlock(&battery_lock);
	return err;
}


/**
 * Stub.
 */
int battery_charging_state_get(enum battery_charging_state_t *state) {

	__ASSERT(battery_is_configured(), "Battery not configured");

	*state = BATTERY_STATE_DISCHARGING;
	return 0;
}

/**
 * Reads all values and returns them.
 */
int battery_status_get(struct battery_status_t *status) {

	__ASSERT(battery_is_configured(), "Battery not configured");

	int err;
	err = k_mutex_lock(&battery_lock, K_FOREVER);
	if(err < 0) {
		LOG_ERR("Reading status: error locking mutex (%s)", strerror(err));
		return err;
	}

	if(!battery_is_configured_log()) {
		err = -EINVAL;
		goto error_free_lock;
	}

	err = battery_voltage_get(&status->mv);
	if(err < 0) {
		LOG_ERR("Reading status: error reading voltage (%s)", strerror(err));
		goto error_free_lock;
	}

	err = battery_charging_state_get(&status->charging_state);
	if(err < 0) {
		LOG_ERR("Reading status: error reading chargin state (%s)", strerror(err));
		goto error_free_lock;
	}

	battery_mv_to_level(&status->level, status->mv);

	k_mutex_unlock(&battery_lock);
	return 0;


	error_free_lock:
	k_mutex_unlock(&battery_lock);
	return err;
}


/*
 *
 * Internal functions.
 *
 */

/**
 * @brief Checks whether `battery_configured` has been successfully called
 * previously.
 *
 * @return bool
 */
static bool battery_is_configured() {
	return battery_config.battery_mv_full > 0;
}

/**
 * @brief Checks whether the battery module is configured. If not, prints an error.
 *
 * @return bool
 */
static bool battery_is_configured_log() {
	bool configured = battery_is_configured();
	if(!configured) {
		LOG_ERR("Battery has not been configured");
	}
	return configured;
}

/**
 * @brief Initializes and configures the battery's ADC channel.
 *
 * @return int 0 if successful.
 */
static int adc_configure() {

	int err;
	if(!adc_is_ready_dt(&adc_channel)) {
		return -EAGAIN;
	}

	err = adc_channel_setup_dt(&adc_channel);
	if(err < 0) {
		return err;
	}

	return 0;
}

/**
 * @brief Reads a single ADC value. Blocks while the read is in progress.
 *
 * @param[out] adc_value ADC output.
 *
 * @return int 0 if successful.
 */
static int adc_get_blocking(int16_t *adc_value) {

	int err;

	// Using the Zephyr ADC API requires setting up a sequence.
	//
	// Doing this is almost zero-cost, as it only involves copying small
	// calibration values so it can be done each read.
	struct adc_sequence sequence = {
		.buffer = adc_value,
		.buffer_size = sizeof(*adc_value),
	};

	err = adc_sequence_init_dt(&adc_channel, &sequence);
	if(err < 0) {
		LOG_ERR("Initializing ADC sequence (%s)", strerror(err));
		return err;
	}

	err = adc_read_dt(&adc_channel, &sequence);
	if(err < 0) {
		LOG_ERR("Error reading ADC channel (%s)", strerror(err));
		return err;
	}

	return 0;
}

/**
 * @brief Converts a raw ADC value to its actual mV value.
 *
 * @param[out] voltage Equivalent voltage in millivolts.
 * @param[in] adc_value Raw ADC value.
 *
 * @return int 0 if successful.
 */
static int adc_to_mv(battery_mv_t *voltage, const int16_t adc_value) {
	int err;
	// `adc_raw_to_millivolts_dt` use the second parameter as input and output.
	int32_t result = adc_value;
	err = adc_raw_to_millivolts_dt(&adc_channel, &result);
	*voltage = result;
	return err;
}

/**
 * @brief Converts a battery voltage to a percentage level.
 *
 * @param[out] level Battery level result.
 * @param[in] voltage Voltage input.
 */
static void battery_mv_to_level(battery_level_t *level, const battery_mv_t voltage) {

	battery_mv_t min = battery_config.battery_mv_depleted;
	battery_mv_t max = battery_config.battery_mv_full;
	battery_mv_t range = max - min;
	battery_mv_t current = voltage;

	if(current > max) {
		*level = 100;
		return;
	}

	if(current < min) {
		*level = 0;
		return;
	}

	*level = (current - min) * 100 / (range);
}

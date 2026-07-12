#include "settings/settings.h"

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(spv_settings, LOG_LEVEL_DBG);

#define SETTINGS_TREE "spv"
#define SETTINGS_SAMPLE_RATE "sample_rate"
#define SETTINGS_ENABLED_SENSORS "sensors"

#define MAX_SAMPLING_RATE CONFIG_SPV_SETTINGS_MAX_SAMPLING_PERIOD
#define MIN_SAMPLING_RATE CONFIG_SPV_SETTINGS_MIN_SAMPLING_PERIOD

/**
 * Save configuration asynchronously.
 */
static void settings_save_handler(struct k_work *work);
static K_WORK_DEFINE(settings_save_work, settings_save_handler);

static K_MUTEX_DEFINE(settings_mutex);
static struct settings_spv_t settings_global = {
	// Default settings. Overwritten by settings subsystem.
	.enabled_sensors =
		SENSOR_BIT_TEMPERATURE_HUMIDITY |
		SENSOR_BIT_LUMINOSITY |
		SENSOR_BIT_NOISE |
		SENSOR_BIT_VOC,
	.sensor_sampling_rate = 60
};


/*
 *
 * Public interface.
 *
 */

int settings_spv_set(const struct settings_spv_t *settings) {
	int err;
	LOG_DBG("Updating system settings");
	err = k_mutex_lock(&settings_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking settings mutex for writing: %d", err);
		return err;
	}

	if(
		settings->enabled_sensors >
			(SENSOR_BIT_VOC |
			SENSOR_BIT_NOISE |
			SENSOR_BIT_TEMPERATURE_HUMIDITY |
			SENSOR_BIT_LUMINOSITY)

			||

		(settings->enabled_sensors & SENSOR_BIT_VOC && !(settings->enabled_sensors & SENSOR_BIT_TEMPERATURE_HUMIDITY))
	) {
		LOG_ERR("Error saving settings: invalid sensor flags.");
		return -EINVAL;
	}

	if(
		settings->sensor_sampling_rate > MAX_SAMPLING_RATE ||
		settings->sensor_sampling_rate < MIN_SAMPLING_RATE
	) {
		LOG_ERR("Error saving settings: invalid sampling rate.");
		return -EINVAL;
	}


	memcpy(&settings_global, settings, sizeof(*settings));

	k_mutex_unlock(&settings_mutex);

	STRUCT_SECTION_FOREACH(settings_cb_t, cb) {
        if(cb->on_settings_update) {
			cb->on_settings_update(settings);
		}
	}

	k_work_submit(&settings_save_work);

	return 0;
}

int settings_spv_get(struct settings_spv_t *settings) {
	int err;
	err = k_mutex_lock(&settings_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking settings mutex for reading: %d", err);
		return err;
	}

	memcpy(settings, &settings_global, sizeof(*settings));

	k_mutex_unlock(&settings_mutex);
	return 0;
}

int settings_spv_load() {
	return settings_load_subtree(SETTINGS_TREE);
}

/*
 *
 * Internal functions.
 *
 */
static void settings_save_handler(struct k_work *work) {
	int err;
	err = settings_save_subtree(SETTINGS_TREE);
	if(err) {
		LOG_ERR("Error saving settings: %d", err);
	}
}

/*
 *
 * Settings subsystem.
 *
 */

static int settings_subsys_set(
	const char *key, size_t len,
	settings_read_cb read_cb, void *cb_arg
) {
	// No locking required.
	// Nothing should call the `settings_spv_*` functions until
	// `settings_spv_load` is run, which calls this.

	const char *next;
	int err_or_len;

	if(settings_name_steq(key, SETTINGS_ENABLED_SENSORS, &next) && !next) {
		if(len != sizeof(settings_global.enabled_sensors)) {
			return -EINVAL;
		}

		err_or_len = read_cb(cb_arg, &settings_global.enabled_sensors, sizeof(settings_global.enabled_sensors));
		if(err_or_len > 0) { // > 0 is length.
			return 0;
		}
		// Otherwise return the error back.
		return err_or_len;
	}

	if(settings_name_steq(key, SETTINGS_SAMPLE_RATE, &next) && !next) {
		if(len != sizeof(settings_global.sensor_sampling_rate)) {
			return -EINVAL;
		}

		err_or_len = read_cb(cb_arg, &settings_global.sensor_sampling_rate, sizeof(settings_global.sensor_sampling_rate));
		if(err_or_len > 0) {
			return 0;
		}
		return err_or_len;
	}

	return -EINVAL;
}

int settings_subsys_export(
	int (*export_func)(const char *name, const void *val, size_t val_len)
) {
	int err;
	err = k_mutex_lock(&settings_mutex, K_FOREVER);
	if(err) {
		return err;
	}

	err = export_func(
		SETTINGS_TREE "/" SETTINGS_ENABLED_SENSORS,
		&settings_global.enabled_sensors,
		sizeof(settings_global.enabled_sensors)
	);
	if(err) {
		k_mutex_unlock(&settings_mutex);
		return err;
	}

	err = export_func(
		SETTINGS_TREE "/" SETTINGS_SAMPLE_RATE,
		&settings_global.sensor_sampling_rate,
		sizeof(settings_global.sensor_sampling_rate)
	);
	k_mutex_unlock(&settings_mutex);
	return err;
}

SETTINGS_STATIC_HANDLER_DEFINE(
	spv_settings_handler, SETTINGS_TREE,
	NULL,					// Get hook.
	settings_subsys_set,	// Called when "settings_load()" is called.
	NULL,					// Called after settings are loaded.
	settings_subsys_export	// Called when settings have to be saved (before reboot).
);

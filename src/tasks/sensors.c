#include "tasks/sensors.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "settings/settings.h"
#include "timesync/timesync.h"
#include "fs/fs.h"
#include "factory_data/factory_data.h"

LOG_MODULE_REGISTER(sensors_task, LOG_LEVEL_DBG);


static void sensor_task(void*, void*, void*);
static K_THREAD_STACK_DEFINE(sensor_thread_stack, 1024);
static struct k_thread sensor_thread_data;

static K_MUTEX_DEFINE(readings_mutex);

#define BIT_PARAMETERS_UPDATE		BIT(0)
static K_EVENT_DEFINE(time_or_setting_update);


static struct sensor_current_readings_t current_readings = SENSOR_READINGS_EMPTY;

static void on_time_update(const struct timesync_status_t *time_status);
static void on_settings_update(const struct settings_spv_t *settings);
static int wake_time_get(k_timeout_t *timeout, time_t *wake_time, const struct settings_spv_t *settings);
static void settings_get_or_panic(struct settings_spv_t *settings);
static void sensor_routine(
	bool store,
	bool force_sector_update,
	const struct settings_spv_t *settings,
	time_t current_time
);
static void update_current_readings(const struct sensor_current_readings_t *readings);

static TIMESYNC_CB_DEF(time_update) = {
	.on_time_update = on_time_update,
};

static SETTINGS_CB_DEF(settings_update) = {
	.on_settings_update = on_settings_update,
};

int sensors_task_start() {
	k_tid_t id;

	id = k_thread_create(
		&sensor_thread_data, sensor_thread_stack,
		K_THREAD_STACK_SIZEOF(sensor_thread_stack),
		sensor_task,
		NULL, NULL, NULL,
		14, 0, K_NO_WAIT
	);
	k_thread_name_set(id, "sensor");

	return 0;
}

int sensors_get_current_values(struct sensor_current_readings_t *readings) {
	int err;

	err = k_mutex_lock(&readings_mutex, K_FOREVER);
	if(err) {
		return err;
	}
	memcpy(readings, &current_readings, sizeof(*readings));
	k_mutex_unlock(&readings_mutex);
	return 0;
}


/**
 * Periodically reads sensor data and persists it to RRAM.
 */
static void sensor_task(void*_unused1, void*_unused2, void*_unused3) {
	int err;

	bool parameters_changed = true;
	struct settings_spv_t settings;
	settings_get_or_panic(&settings);
	sensor_routine(
		false, false,
		&settings, 0
	);

	while(true) {
		k_timeout_t wake_timeout;
		time_t wake_time;
		err = wake_time_get(&wake_timeout, &wake_time, &settings);
		if(err) {
			LOG_WRN("Invalid time configuration");
			LOG_WRN("Waiting for time sync");
			k_event_wait(
				&time_or_setting_update, BIT_PARAMETERS_UPDATE,
				true, K_FOREVER
			);
			parameters_changed = true;
			settings_get_or_panic(&settings);
			LOG_INF("Sensor parameters updated");
			continue;
		}

		uint32_t event_result = k_event_wait(
			&time_or_setting_update, BIT_PARAMETERS_UPDATE,
			true, wake_timeout
		);
		if(event_result & BIT_PARAMETERS_UPDATE) {
			parameters_changed = true;
			settings_get_or_panic(&settings);
			LOG_INF("Sensor parameters updated");
			continue;
		}

		sensor_routine(true, parameters_changed, &settings, wake_time);
		LOG_INF("Performed sensor reading.");
		parameters_changed = false;
	}
}

/*
 *
 * Internal functions
 *
 */

static void on_time_update(const struct timesync_status_t *time_status) {
	k_event_post(&time_or_setting_update, BIT_PARAMETERS_UPDATE);
}

static void on_settings_update(const struct settings_spv_t *settings) {
	k_event_post(&time_or_setting_update, BIT_PARAMETERS_UPDATE);
}

static void settings_get_or_panic(struct settings_spv_t *settings) {
	int err;
	err = settings_spv_get(settings);
	if(err) {
		LOG_ERR("Error obtaining settings: %d", err);
		k_panic();
	}
}

static int wake_time_get(k_timeout_t *timeout, time_t *wake_time, const struct settings_spv_t *settings) {
	int err;
	struct timesync_status_t time_status;
	err = timesync_status_get(&time_status);
	if(err) {
		LOG_ERR("Error getting time: %d", err);
		return err;
	}

	if(
		time_status.source == TIMESYNC_SOURCE_NONE ||
		time_status.unix_time < factorydata_get().manufacturing_date
	) {
		LOG_WRN("No time source configured, refusing to start sensor measurement");
		return -EINVAL;
	}

	time_t current_time = time_status.unix_time;
	time_t sampling_period = settings->sensor_sampling_rate;

	*wake_time = current_time - current_time % sampling_period + sampling_period;
	time_t sleep_seconds = *wake_time - current_time;
	*timeout = K_SECONDS(sleep_seconds);
	return 0;
}

static void sensor_routine(
	bool store,
	bool force_sector_update,
	const struct settings_spv_t *settings,
	time_t current_time
) {
	int err;

	if(store && (fs_current_sector_remaining_space() == 0 || force_sector_update)) {
		LOG_INF("Allocating new filesystem sector");
		enum timesync_source_t time_source;
		err = timesync_source_get(&time_source);
		if(err) {
			LOG_ERR("Unable to obtain time source: %d", err);
			return;
		}

		fs_sector_new(
			&(struct fs_summary_t) {
				.sensor_flags = settings->enabled_sensors,
				.sampling_rate_seconds = settings->sensor_sampling_rate,
				.unix_time_start = current_time,
				.timesync_status = time_source,
			}
		);
	}


	struct sensor_current_readings_t readings = SENSOR_READINGS_EMPTY;

	if(settings->enabled_sensors & SENSOR_BIT_TEMPERATURE_HUMIDITY) {
		err = sense_ambient_read(&readings.millicelsius, &readings.rh_permille);
		if(err) {
			LOG_ERR("Error reading ambient sensor: %d", err);
		}
	}

	if(settings->enabled_sensors & SENSOR_BIT_LUMINOSITY) {
		err = sense_luminosity_read(&readings.lux);
		if(err) {
			LOG_ERR("Error reading luminosity sensor: %d", err);
		}
	}

	if(settings->enabled_sensors & SENSOR_BIT_NOISE) {
		err = sense_noise_read(&readings.centibels);
		if(err) {
			LOG_ERR("Error reading noise sensor: %d", err);
		}
	}

	if(
		settings->enabled_sensors & SENSOR_BIT_VOC &&
		readings.millicelsius != 0xFFFFFFFE &&
		readings.rh_permille != 0xFFFE
	) {
		err = sense_voc_read(&readings.voc_raw, readings.millicelsius, readings.rh_permille);
		if(err) {
			LOG_ERR("Error reading VOC sensor: %d", err);
		}
	}

	if(store) {
		err = fs_reading_append(
			&(struct fs_data_entry_t) {
				.millicelsius = readings.millicelsius,
				.centibels = readings.centibels,
				.rh_permille = readings.rh_permille,
				.lux = readings.lux,
				.voc_raw = readings.voc_raw
			}
		);
		if(err) {
			LOG_ERR("Error appending sensor readings: %d", err);
		}
	}

	update_current_readings(&readings);
}

static void update_current_readings(const struct sensor_current_readings_t *readings) {
	int err;
	err = k_mutex_lock(&readings_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking readings mutex: %d", err);
	}

	memcpy(&current_readings, readings, sizeof(*readings));

	k_mutex_unlock(&readings_mutex);
}

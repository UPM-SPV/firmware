#include "sense/sense.h"
#include "sense/util.h"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(shtc3, LOG_LEVEL_DBG);


#define AMBIENT_SENSOR_NODE DT_ALIAS(ambient_sensor)
static const struct device *ambient_sensor = DEVICE_DT_GET(AMBIENT_SENSOR_NODE);


int sense_ambient_read(sense_millicelsius_t *temperature, sense_rh_permille_t *humidity) {
	int err;

	if(!device_is_ready(ambient_sensor)) {
		LOG_ERR("SHTC3 is not ready");
		return -EAGAIN;
	}

	struct sensor_value raw_temperature, raw_humidity;
	err = sensor_sample_fetch(ambient_sensor);
	if(err) {
		LOG_ERR("Error reading SHTC3: %d", err);
		return err;
	}

	err = sensor_channel_get(ambient_sensor, SENSOR_CHAN_AMBIENT_TEMP, &raw_temperature);
	if(err) {
		LOG_ERR("Error fetching temperature: %d", err);
		return err;
	}

	err = sensor_channel_get(ambient_sensor, SENSOR_CHAN_HUMIDITY, &raw_humidity);
	if(err) {
		LOG_ERR("Error fetching humidity: %d", err);
		return err;
	}

	*temperature = SENSE_MERGE_SENSOR_VALUE(raw_temperature, 1000);
	*humidity = SENSE_MERGE_SENSOR_VALUE(raw_humidity, 10);

	return 0;
}

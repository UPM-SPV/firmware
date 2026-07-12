#include "sense/sense.h"
#include "sense/util.h"

#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(veml6035, LOG_LEVEL_DBG);


#define LIGHT_SENSOR_NODE DT_ALIAS(light_sensor)
static const struct device *light_sensor = DEVICE_DT_GET(LIGHT_SENSOR_NODE);


int sense_luminosity_read(sense_lux_t *luminosity) {
	int err;

	err = pm_device_runtime_get(light_sensor);
	if(err) {
		LOG_ERR("Error powering up VEML6035");
		return err;
	}

	if(!device_is_ready(light_sensor)) {
		LOG_ERR("VEML6035 is not ready");
		return -EAGAIN;
	}

	struct sensor_value raw_lux;
	err = sensor_sample_fetch(light_sensor);
	if(err) {
		LOG_ERR("Error reading VEML6035: %d", err);
		return err;
	}

	err = sensor_channel_get(light_sensor, SENSOR_CHAN_LIGHT, &raw_lux);
	if(err) {
		LOG_ERR("Error fetching luminosity: %d", err);
		return err;
	}
	pm_device_runtime_put_async(light_sensor, K_NO_WAIT);

	*luminosity = raw_lux.val1;

	return 0;
}

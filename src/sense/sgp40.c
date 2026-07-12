#include "sense/sense.h"
#include "sense/util.h"

#include <zephyr/logging/log.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/sgp40.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(sgp40, LOG_LEVEL_DBG);

#define VOC_SENSOR_NODE DT_ALIAS(voc_sensor)
static const struct device *voc_sensor = DEVICE_DT_GET(VOC_SENSOR_NODE);


int sense_voc_read(
	sense_voc_raw_t *voc,
	const sense_millicelsius_t temperature_cal,
	const sense_rh_permille_t humidity_cal
) {
	int err;

	if(!device_is_ready(voc_sensor)) {
		LOG_ERR("SGP40 is not ready");
		return -EAGAIN;
	}

	err = pm_device_runtime_get(voc_sensor);
	if(err) {
		LOG_ERR("Error enabling SGP40 power domain");
		return err;
	}

	// The SGP40 only takes into account the integer part, so the precision
	// loss is fine.
	struct sensor_value raw_temperature, raw_humidity;
	SENSE_SPLIT_SENSOR_VALUE(temperature_cal, raw_temperature, 1000);
	SENSE_SPLIT_SENSOR_VALUE(humidity_cal, raw_humidity, 10);

	err = sensor_attr_set(
		voc_sensor,
		SENSOR_CHAN_GAS_RES,
		SENSOR_ATTR_SGP40_TEMPERATURE,
		&raw_temperature
	);
	if(err) {
		LOG_ERR("Error calibrating temperature: %d", err);
		goto err_suspend;
	}

	err = sensor_attr_set(
		voc_sensor,
		SENSOR_CHAN_GAS_RES,
		SENSOR_ATTR_SGP40_HUMIDITY,
		&raw_humidity
	);
	if(err) {
		LOG_ERR("Error calibrating humidity: %d", err);
		goto err_suspend;
	}


	err = sensor_sample_fetch(voc_sensor);
	if(err) {
		LOG_ERR("Error reading SGP40: %d", err);
		goto err_suspend;
	}

	// According to Sensirion, when the SGP40 is not doing continuous
	// measurements, the first reading should be discarded after waiting 170ms.
	k_msleep(170);

	err = sensor_sample_fetch(voc_sensor);
	if(err) {
		LOG_ERR("Error reading SGP40: %d", err);
		goto err_suspend;
	}

	struct sensor_value raw_voc;
	err = sensor_channel_get(voc_sensor, SENSOR_CHAN_GAS_RES, &raw_voc);
	if(err) {
		LOG_ERR("Error fetching VOC: %d", err);
		goto err_suspend;
	}

	// The SGP40 driver simply returns the result in val1.
	*voc = raw_voc.val1;

	pm_device_runtime_put_async(voc_sensor, K_NO_WAIT);
	return 0;


	err_suspend:
	pm_device_runtime_put_async(voc_sensor, K_NO_WAIT);
	return err;
}

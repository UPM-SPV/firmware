#include "ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include "ble/uuid.h"
#include "tasks/sensors.h"

LOG_MODULE_REGISTER(ble_environment, LOG_LEVEL_DBG);


// Environmental Sensing Service for quick access to latest values.
#define BLE_UUID_ESS 				BT_UUID_DECLARE_16(0x181A)
#define BLE_UUID_ESS_TEMPERATURE 	BT_UUID_DECLARE_16(0x2A6E)
#define BLE_UUID_ESS_HUMIDITY	 	BT_UUID_DECLARE_16(0x2A6F)
#define BLE_UUID_ESS_LUMINOSITY 	BT_UUID_DECLARE_16(0x2AFB)
#define BLE_UUID_ESS_NOISE		 	BT_UUID_SPV_DECLARE(0x8051)
#define BLE_UUID_ESS_VOC_RAW	 	BT_UUID_SPV_DECLARE(0x8052)

static ssize_t temperature_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE temperature");
	struct sensor_current_readings_t current_readings;
	err = sensors_get_current_values(&current_readings);
	if(err) {
		LOG_ERR("Error reading temperature level: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	if(current_readings.millicelsius == 0xFFFFFFFE) {
		LOG_WRN("Invalid temperature value");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	int16_t temperature = current_readings.millicelsius / 10;
	LOG_DBG("	Temperature: %"PRIu16" °C", temperature / 100);
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&temperature, sizeof(temperature)
	);
}

static ssize_t humidity_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE humidity");
	struct sensor_current_readings_t current_readings;
	err = sensors_get_current_values(&current_readings);
	if(err) {
		LOG_ERR("Error reading humidity level: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	if(current_readings.rh_permille == 0xFFFE) {
		LOG_WRN("Invalid humidity value");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	int16_t humidity = current_readings.rh_permille * 10;
		LOG_DBG("	Humidity:	%"PRIu16" %%", humidity / 100);
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&humidity, sizeof(humidity)
	);
}

static ssize_t luminosity_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE luminosity");
	struct sensor_current_readings_t current_readings;
	err = sensors_get_current_values(&current_readings);
	if(err) {
		LOG_ERR("Error reading luminosity level: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	if(current_readings.lux == 0xFFFE) {
		LOG_WRN("Invalid luminosity value");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	Luminosity:	%"PRIu16" lux", current_readings.lux);
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&current_readings.lux, sizeof(current_readings.lux)
	);
}

static ssize_t noise_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE noise");
	struct sensor_current_readings_t current_readings;
	err = sensors_get_current_values(&current_readings);
	if(err) {
		LOG_ERR("Error reading noise level: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	if(current_readings.centibels == 0xFFFFFFFE) {
		LOG_ERR("Invalid noise value");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	Noise:	%"PRIu16" dB", current_readings.centibels / 10);
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&current_readings.centibels, sizeof(current_readings.centibels)
	);
}

static ssize_t voc_raw_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE VOC");
	struct sensor_current_readings_t current_readings;
	err = sensors_get_current_values(&current_readings);
	if(err) {
		LOG_ERR("Error reading VOC level: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	if(current_readings.voc_raw == 0xFFFFFFFE) {
		LOG_WRN("Invalid VOC value");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	VOC:	%"PRIu16, current_readings.voc_raw);

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&current_readings.voc_raw, sizeof(current_readings.voc_raw)
	);
}


BT_GATT_SERVICE_DEFINE(
	environmental_sensing_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_ESS),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ESS_TEMPERATURE,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		temperature_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ESS_HUMIDITY,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		humidity_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ESS_LUMINOSITY,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		luminosity_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ESS_NOISE,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		noise_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ESS_VOC_RAW,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		voc_raw_read, NULL,
		NULL
	),
);

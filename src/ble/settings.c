#include "ble/ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
#include <pb_decode.h>
#include <pb_encode.h>


#include "settings/settings.h"
#include "res/protobuf/device_settings.pb.h"
#include "ble/ctx.h"
#include "ble/uuid.h"
LOG_MODULE_REGISTER(ble_settings, LOG_LEVEL_DBG);


#define BLE_UUID_SETTINGS 			BT_UUID_SPV_DECLARE(0x8040)
#define BLE_UUID_SETTINGS_CHR 		BT_UUID_SPV_DECLARE(0x8041)

#define CURRENT_SCHEMA	1


static ssize_t settings_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading BLE settings");
	struct settings_spv_t settings;

	err = settings_spv_get(&settings);
	if(err) {
		LOG_ERR("Error reading settings: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	DeviceSettings serialized_settings = {
		.schema_version = CURRENT_SCHEMA,
		.sensor_flags = settings.enabled_sensors,
		.sampling_rate_seconds = settings.sensor_sampling_rate,
	};

	char encoded_buffer[DeviceSettings_size];
	pb_ostream_t stream;
	stream = pb_ostream_from_buffer(encoded_buffer, sizeof(encoded_buffer));
	if(!pb_encode(&stream, DeviceSettings_fields, &serialized_settings)) {
		LOG_ERR("Error encoding settings");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	Sucessfully sent device settings:");
	LOG_DBG("		Sampling rate:	%"PRIu64, settings.sensor_sampling_rate);
	LOG_DBG("		Sensor flags:");
	LOG_DBG("			VOC:	%c", settings.enabled_sensors & SENSOR_BIT_VOC ? 'y' : 'n');
	LOG_DBG("			Noise:	%c", settings.enabled_sensors & SENSOR_BIT_NOISE ? 'y' : 'n');
	LOG_DBG("			T/Hum:	%c", settings.enabled_sensors & SENSOR_BIT_TEMPERATURE_HUMIDITY? 'y' : 'n');
	LOG_DBG("			Lum:	%c", settings.enabled_sensors & SENSOR_BIT_LUMINOSITY ? 'y' : 'n');
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		encoded_buffer, stream.bytes_written
	);
}


static ssize_t settings_write(
	struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf,
	uint16_t len, uint16_t offset, uint8_t flags
) {
	int err;
	LOG_DBG("Updating BLE settings");
	if(offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if(len == 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	DeviceSettings decoded_settings;
	pb_istream_t stream = pb_istream_from_buffer(buf, len);
	if(!pb_decode(&stream, DeviceSettings_fields, &decoded_settings)) {
		LOG_ERR("Error decoding device settings");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
	}
	if(decoded_settings.schema_version != CURRENT_SCHEMA) {
		LOG_ERR("Unsupported schema: %"PRIu32, decoded_settings.schema_version);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
	}

	struct settings_spv_t settings = {
		.enabled_sensors = decoded_settings.sensor_flags,
		.sensor_sampling_rate = decoded_settings.sampling_rate_seconds
	};
	err = settings_spv_set(&settings);
	if(err) {
		LOG_ERR("Error saving settings: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
	}

	LOG_DBG("	Sucessfully updated device settings:");
	LOG_DBG("		Sampling rate:	%"PRIu64, settings.sensor_sampling_rate);
	LOG_DBG("		Sensor flags:");
	LOG_DBG("			VOC:	%c", settings.enabled_sensors & SENSOR_BIT_VOC ? 'y' : 'n');
	LOG_DBG("			Noise:	%c", settings.enabled_sensors & SENSOR_BIT_NOISE ? 'y' : 'n');
	LOG_DBG("			T/Hum:	%c", settings.enabled_sensors & SENSOR_BIT_TEMPERATURE_HUMIDITY? 'y' : 'n');
	LOG_DBG("			Lum:	%c", settings.enabled_sensors & SENSOR_BIT_LUMINOSITY ? 'y' : 'n');

	return len;
}


BT_GATT_SERVICE_DEFINE(
	settings_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_SETTINGS),

	BT_GATT_CHARACTERISTIC(
		BLE_UUID_SETTINGS_CHR,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ, BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_READ_ENCRYPT,
		settings_read, settings_write,
		NULL
	)
);

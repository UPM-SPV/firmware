#include "ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include <zephyr/app_version.h>

#include "factory_data/factory_data.h"

LOG_MODULE_REGISTER(ble_dis, LOG_LEVEL_DBG);


#define BLE_UUID_DIS 					BT_UUID_DECLARE_16(0x180A)
#define BLE_UUID_DIS_SERIAL_NUMBER 		BT_UUID_DECLARE_16(0x2A25)
#define BLE_UUID_DIS_FIRMWARE_REVISION 	BT_UUID_DECLARE_16(0x2A26)
#define BLE_UUID_DIS_HARDWARE_REVISION 	BT_UUID_DECLARE_16(0x2A27)

static ssize_t dis_serial_number_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Serial number requested");
	struct factorydata_t data = factorydata_get();

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		data.serial_number, strlen(data.serial_number)
	);
}

static ssize_t dis_firmware_version_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Firmware version requested");
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		APP_VERSION_EXTENDED_STRING, strlen(APP_VERSION_EXTENDED_STRING)
	);
}

static ssize_t dis_hardware_revision_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Hardware version requested");
	struct factorydata_t data = factorydata_get();

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		data.board_revision, strlen(data.board_revision)
	);
}

BT_GATT_SERVICE_DEFINE(
	device_information_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_DIS),

	BT_GATT_CHARACTERISTIC(
		BLE_UUID_DIS_SERIAL_NUMBER,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		dis_serial_number_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_DIS_FIRMWARE_REVISION,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		dis_firmware_version_read, NULL,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_DIS_HARDWARE_REVISION,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		dis_hardware_revision_read, NULL,
		NULL
	)
);

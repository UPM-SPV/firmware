#include "ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include <zephyr/app_version.h>

#include "battery/battery.h"

LOG_MODULE_REGISTER(ble_battery, LOG_LEVEL_DBG);


#define BLE_UUID_BTS 				BT_UUID_DECLARE_16(0x180F)
#define BLE_UUID_BTS_BATTERY_LEVEL 	BT_UUID_DECLARE_16(0x2A19)

static ssize_t battery_level_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Reading battery level");
	int err;
	battery_level_t battery_level;
	err = battery_level_get(&battery_level);
	if(err) {
		LOG_ERR("Error reading battery level");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	uint8_t ble_battery_level = (uint8_t) battery_level;
	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&ble_battery_level, sizeof(ble_battery_level)
	);
}


BT_GATT_SERVICE_DEFINE(
	battery_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_BTS),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_BTS_BATTERY_LEVEL,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		battery_level_read, NULL,
		NULL
	)
);

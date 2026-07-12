#include "ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include "timesync/timesync.h"
#include "ble/uuid.h"

LOG_MODULE_REGISTER(ble_time, LOG_LEVEL_DBG);


#define BLE_UUID_CTS 					BT_UUID_DECLARE_16(0x1805)
#define BLE_UUID_CTS_CURRENT_TIME_UNIX 	BT_UUID_SPV_DECLARE(0x8031)


static ssize_t current_time_write(
	struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf,
	uint16_t len, uint16_t offset, uint8_t flags
) {
	LOG_DBG("Updating BLE device time");
	if(len != sizeof(time_t)) {
		LOG_WRN("Wrong time length: %"PRIu16, len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	if(offset != 0) {
		LOG_WRN("Wrong read offset: %"PRIu16, offset);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	int err;
	const time_t *time = buf;

	err = timesync_set(*time, TIMESYNC_SOURCE_AUTHORITATIVE);
	if(err) {
		LOG_WRN("Error updating time: %s", strerror(err));
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	Updated time to: %s", ctime(time));
	return BT_GATT_ERR(BT_ATT_ERR_SUCCESS);
}


static ssize_t current_time_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	time_t time;
	err = timesync_time_get(&time);
	if(err) {
		LOG_WRN("Error reading time: %s", strerror(err));
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		&time, sizeof(time)
	);
}


BT_GATT_SERVICE_DEFINE(
	current_time_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_CTS),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_CTS_CURRENT_TIME_UNIX,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
		current_time_read, current_time_write,
		NULL
	),
);

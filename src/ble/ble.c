#include "ble/ble.h"
#include "ble/advertise.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/gap.h>

#include <zephyr/logging/log.h>

#include <zephyr/settings/settings.h>

#include "battery/battery.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_DBG);

static struct bt_conn *bt_connection = NULL;


static void conn_update_work_handler(struct k_work *work);
static int generate_id();
static void on_connected(struct bt_conn *conn, uint8_t err);
static void on_disconnected(struct bt_conn *conn, uint8_t err);
static void recycled_cb(void);

K_WORK_DEFINE(conn_update_work, conn_update_work_handler);

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.recycled = recycled_cb,
};

/*
 *
 * Interface definition.
 *
 */

int ble_start() {
	int err;

	// Start BLE (blocking)
	err = bt_enable(NULL);
	if(err) {
		LOG_ERR("Error during BLE initialization: %d", err);
		return err;
	}

	err = settings_load();
	if(err) {
		LOG_ERR("Error loading settings: %s", strerror(err));
		return err;
	}

	// Check if we need to generate a new id.
	err = generate_id();
	if(err) {
		LOG_ERR("Error generating id: %d", err);
		return err;
	}

	k_work_init(&conn_update_work, conn_update_work_handler);
	ble_advertising_start();

	return 0;
}


/*
 *
 * Internal functions.
 *
 */

static int generate_id() {
	int err;
	LOG_DBG("Generating BLE id");

	size_t found_ids = 0;
	bt_id_get(NULL, &found_ids);
	if(found_ids == 0) {
		LOG_INF("Found no BLE ids");
		err = bt_id_create(NULL, NULL);
		if(err) {
			LOG_ERR("Error creating BLE address: %s", strerror(err));
			return err;
		}
	}

	return 0;
}

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params) {
	if(err) {
        LOG_WRN("Error updating MTU");
    } else {
        LOG_INF("MTU updated. Current MTU: %d\n", bt_gatt_get_mtu(conn));
    }
}

static struct bt_gatt_exchange_params mtu_exchange_params = {
	.func = mtu_exchange_cb
};
static void conn_update_work_handler(struct k_work *work) {
	int err;
	LOG_DBG("Updating BLE connection parameters");

	// Try to upgrade connection to 2 Mbps interface.
	const struct bt_conn_le_phy_param preferred_phy = {
		.options = BT_CONN_LE_PHY_OPT_NONE,
		.pref_rx_phy = BT_GAP_LE_PHY_2M,
		.pref_tx_phy = BT_GAP_LE_PHY_2M,
	};
	err = bt_conn_le_phy_update(bt_connection, &preferred_phy);
	if(err) {
		LOG_WRN("Unable to update PHY parameters: %s", strerror(err));
	} else {
		LOG_DBG("PHY parameters updated");
	}

	// Try to increase maximum packet data length.
	struct bt_conn_le_data_len_param data_len = {
        .tx_max_len = BT_GAP_DATA_LEN_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    err = bt_conn_le_data_len_update(bt_connection, &data_len);
    if (err) {
        LOG_WRN("Unable to update PDU size: %s", strerror(err));
    } else {
		LOG_DBG("PDU size updated");
	}

	// Try to increase MTU.
	k_msleep(1000);
	err = bt_gatt_exchange_mtu(bt_connection, &mtu_exchange_params);
	if(err) {
		LOG_WRN("Failed to renegotiate MTU size: %s", strerror(err));
	} else {
		LOG_DBG("New MTU size negotiated");
	}
}

static void update_connection_parameters(void) {
	k_work_submit(&conn_update_work);
}

static void on_connected(struct bt_conn *conn, uint8_t status) {
	if(status) {
		LOG_ERR("Connection failed, err 0x%02x %s", status, bt_hci_err_to_str(status));
		return;
	}
	bt_connection = bt_conn_ref(conn);
	LOG_INF("BLE device connected");
	update_connection_parameters();
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
	bt_conn_unref(bt_connection);
	bt_connection = NULL;

	LOG_WRN("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));
}

static void recycled_cb(void) {
	LOG_INF("Connection object successfully recycled");
	LOG_INF("Disconnect complete");
	ble_advertising_start();
}

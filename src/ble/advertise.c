#include "advertise.h"
#include "uuid.h"

#include <assert.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

#include "timesync/timesync.h"
#include "mpsl/mpsl.h"

LOG_MODULE_REGISTER(ble_advertise, LOG_LEVEL_DBG);


static const struct bt_le_adv_param *adv_param_filtered = BT_LE_ADV_PARAM(
	BT_LE_ADV_OPT_CONN |// Advertise that this device accepts connections.
	BT_LE_ADV_OPT_USE_IDENTITY |
	BT_LE_ADV_OPT_FILTER_CONN,
	3200,				// 1s advertisement interval (1600 * 0.625ms).
	4800,				// Allow different intervals to avoid collisions (2s).
	NULL				// Undirected advertising.
);

static const struct bt_le_adv_param *adv_param_open = BT_LE_ADV_PARAM(
	BT_LE_ADV_OPT_CONN |
	BT_LE_ADV_OPT_USE_IDENTITY,
	160,				// Use a lower advertising interval during pairing mode.
	320,
	NULL				// Undirected advertising.
);


static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_SOME, BT_UUID_SPV_ENCODE(0x0001)),
};

static const struct bt_data ad_open[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_SOME,
		// Advertise a different UUID to allow devices to easily scan
		// for pairable devices.
		BT_UUID_SPV_ENCODE(0x0002)
	),
};


static const unsigned char url[] = "\x17//upm.es";
static const struct bt_data sd[] = {
	BT_DATA(
		BT_DATA_NAME_COMPLETE,
		CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1
	),
	BT_DATA(
		BT_DATA_GAP_APPEARANCE,
		CONFIG_BT_DEVICE_APPEARANCE,
		sizeof(CONFIG_BT_DEVICE_APPEARANCE)
	),
	BT_DATA(
		BT_DATA_URI,
		url,
		sizeof(url) - 1
	)
};

static bool is_connection_open = false;

/**
 * Start advertising asynchronously to allow calling it in interrupts.
 */
static void adv_work_handler(struct k_work *work);
static K_WORK_DEFINE(adv_work, adv_work_handler);


/*
 *
 * Interface definition.
 *
 */

void ble_advertising_start() {
	LOG_DBG("BLE advertisements requested");
	k_work_submit(&adv_work);
}

void ble_enable_pairing() {
	LOG_DBG("BLE pairing enabled");
	is_connection_open = true;
	bt_le_adv_stop();
}

void ble_disable_pairing() {
	LOG_DBG("BLE pairing disabled");
	is_connection_open = false;
	bt_le_adv_stop();
}


/*
 *
 * Internal functions.
 *
 */

static void setup_bond_accept_list_cb(const struct bt_bond_info *info, void *user_data) {
	int err = bt_le_filter_accept_list_add(&info->addr);
	if(err) {
		LOG_ERR("Error adding peer to accept list");
	}
}

static void adv_work_handler(struct k_work *work) {
	int err;

	// === WORKAROUND ===
	//
	// Allow the BLE stack to cleanup the previous advertisement if this was
	// started by a `bt_le_adv_stop()`.
	//
	// If you remove this, you'll probably get an error like this:
	//	[00:00:14.499,182] <wrn> bt_hci_core: opcode 0x2010 status 0x0c	(You can't call `bt_le_filter_accept_list_clear` while you're advertising, you dummy!)
	//	[00:00:14.499,189] <err> bt_hci_core: Failed to clear filter accept list (Yup, makes sense)
	//	[00:00:14.499,198] <err> ble_advertise: Error starting advertisement: -120 (We can't spam the airwaves, can we? Stop the previous advertisement first!)
	//
	// Meaning that the previous advertisement is still ongoing! By the time we
	// get here (from recycled_cb), it should have been cleaned up.
	//
	// Spoiler: It is not :3
	//
	// Note for the reader: try to remove this if you're using a version that is
	//						newer than NCS v3.3.0, if it works, then great!
	//						Problem solved!
	k_msleep(20);


	if(is_connection_open) {
		bt_le_filter_accept_list_clear();
		err = bt_le_adv_start(adv_param_open, ad_open, ARRAY_SIZE(ad_open), sd, ARRAY_SIZE(sd));
	} else {
		bt_le_filter_accept_list_clear();
		bt_foreach_bond(BT_ID_DEFAULT, setup_bond_accept_list_cb, NULL);
		err = bt_le_adv_start(adv_param_filtered, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	}

	if(err < 0) {
		LOG_ERR("Error starting advertisement: %d", err);
	} else {
		LOG_INF("Advertisement started");
	}
}

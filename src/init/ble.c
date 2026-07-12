#include "init/init.h"

#include <zephyr/logging/log.h>

#include "ble/ble.h"
#include "ble/l2cap.h"

LOG_MODULE_REGISTER(ble_init, LOG_LEVEL_DBG);


static void ble_init_cb_func() {
	int err;
	err = ble_start();
	if(err) {
		LOG_ERR("Error initializing BLE: %d", err);
		k_panic();
	}

	err = ble_l2cap_init();
	if(err) {
		LOG_ERR("Error initializing L2CAP: %d", err);
		k_panic();
	}

	LOG_INF("BLE initialized");
}

INIT_CB_DEF(ble_init_cb) = {
	.on_init = ble_init_cb_func,
	.depends = (const char*[]){
		"attest", "battery", "factorydata", "fs", "mpsl", "sensors", "sensors_task",
		"settings", "timesync", 0
	},
	.provides = (const char*[]){ "ble", 0 },
};

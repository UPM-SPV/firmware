#include "init/init.h"

#include <zephyr/logging/log.h>

#include "fs/fs.h"

LOG_MODULE_REGISTER(filesystem_init, LOG_LEVEL_DBG);


static void fs_init_cb_func() {
	int err;
	err = fs_init();
	if(err) {
		LOG_ERR("Error initializing sensor data filesystem: %d", err);
		k_panic();
	}

	LOG_INF("Sensor data filesystem module initialized");
}

INIT_CB_DEF(fs_init_cb) = {
	.on_init = fs_init_cb_func,
	.provides = (const char*[]){ "fs", 0 },
};

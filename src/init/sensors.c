#include "init/init.h"

#include <zephyr/logging/log.h>

#include "sense/sense.h"

LOG_MODULE_REGISTER(sensors_init, LOG_LEVEL_DBG);


static void sensors_init_cb_func() {
	int err;
	err = sense_noise_init();
	if(err) {
		LOG_ERR("Error initializing microphone: %d", err);
		k_panic();
	}

	LOG_INF("Sensor module initialized");
}

INIT_CB_DEF(sensors_init_cb) = {
	.on_init = sensors_init_cb_func,
	.depends = (const char*[]){ "settings", 0 },
	.provides = (const char*[]){ "sensors", 0 },
};

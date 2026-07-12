#include "init/init.h"

#include <zephyr/logging/log.h>

#include "tasks/sensors.h"

LOG_MODULE_REGISTER(sensors_task_init, LOG_LEVEL_DBG);


static void sensors_task_init_cb_func() {
	int err;
	err = sensors_task_start();
	if(err) {
		LOG_ERR("Error starting sensors task: %d", err);
		k_panic();
	}

	LOG_INF("Sensors task initialized");
}

INIT_CB_DEF(sensors_task_init_cb) = {
	.on_init = sensors_task_init_cb_func,
	.depends = (const char*[]){ "fs", "sensors", "settings", "timesync", 0 },
	.provides = (const char*[]){ "sensors_task", 0 },
};

#include "init/init.h"

#include <zephyr/logging/log.h>

#include "tasks/button_led.h"

LOG_MODULE_REGISTER(button_led_task_init, LOG_LEVEL_DBG);


static void button_led_task_init_cb_func() {
	int err;
	err = button_led_task_start();
	if(err) {
		LOG_ERR("Error starting button LED task: %d", err);
		k_panic();
	}

	LOG_INF("Button LED task initialized");
}

INIT_CB_DEF(button_led_task_init_cb) = {
	.on_init = button_led_task_init_cb_func,
	.depends = (const char*[]){ "ble", 0 },
	.provides = (const char*[]){ "button_led", 0 },
};

#include "init/init.h"

#include <zephyr/logging/log.h>

#include "settings/settings.h"

LOG_MODULE_REGISTER(settings_init, LOG_LEVEL_DBG);


static void settings_init_cb_func() {
	int err;
	err = settings_spv_load();
	if(err) {
		LOG_ERR("Error initializing settings: %d", err);
		k_panic();
	}

	LOG_INF("Settings module initialized");
}

INIT_CB_DEF(settings_init_cb) = {
	.on_init = settings_init_cb_func,
	.provides = (const char*[]){ "settings", 0 },
};

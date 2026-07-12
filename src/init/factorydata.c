#include "init/init.h"

#include <zephyr/logging/log.h>

#include "factory_data/factory_data.h"

LOG_MODULE_REGISTER(factory_data_init, LOG_LEVEL_DBG);


static void factorydata_init_cb_func() {
	int err = 0;

	err = factorydata_load();
	if(err) {
		LOG_ERR("Error reading factory data: %d", err);
		k_panic();
	}

	LOG_INF("Factorydata module initialized");
}

INIT_CB_DEF(factorydata_init_cb) = {
	.on_init = factorydata_init_cb_func,
	.provides = (const char*[]){ "factorydata", 0 },
};

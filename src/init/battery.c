#include "init/init.h"

#include <zephyr/logging/log.h>

#include "battery/battery.h"

LOG_MODULE_REGISTER(battery_init, LOG_LEVEL_DBG);

#define BATTERY_MV_FULL		CONFIG_SPV_BATTERY_MV_FULL
#define BATTERY_MV_DEPLETED	CONFIG_SPV_BATTERY_MV_DEPLETED


static void battery_init_cb_func() {
	int err;
	err = battery_configure(&(struct battery_config_t) {
		.battery_mv_full = BATTERY_MV_FULL,
		.battery_mv_depleted = BATTERY_MV_DEPLETED,
	});
	if(err) {
		LOG_ERR("Error configuring battery module: %d", err);
		k_panic();
	}

	LOG_INF("Battery module initialized");
}

INIT_CB_DEF(battery_init_cb) = {
	.on_init = battery_init_cb_func,
	.provides = (const char*[]){ "battery", 0 },
};

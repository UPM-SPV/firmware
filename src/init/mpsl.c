#include "init/init.h"

#include <zephyr/logging/log.h>

#include "mpsl/mpsl.h"

LOG_MODULE_REGISTER(mpsl_radio_init, LOG_LEVEL_DBG);


static void mpsl_init_cb_func() {
	int err;
	err = mpsl_start();
	if(err) {
		LOG_ERR("Error initializing MPSL radio callbacks: %d", err);
		k_panic();
	}

	LOG_INF("MPSL radio callback module initialized");
}

INIT_CB_DEF(mpsl_init_cb) = {
	.on_init = mpsl_init_cb_func,
	.provides = (const char*[]){ "mpsl", 0 },
};

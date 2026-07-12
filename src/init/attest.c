#include "init/init.h"

#include <zephyr/logging/log.h>

#include "attest/attest.h"

LOG_MODULE_REGISTER(attest_init, LOG_LEVEL_DBG);


static void attest_init_cb_func() {
	int err;
	err = attest_init();
	if(err) {
		LOG_ERR("Error initializing attestation: %d", err);
		k_panic();
	}

	LOG_INF("Attestation module initialized");
}

INIT_CB_DEF(attest_init_cb) = {
	.on_init = attest_init_cb_func,
	.depends = (const char*[]){ "factorydata", 0 },
	.provides = (const char*[]){ "attest", 0 },
};

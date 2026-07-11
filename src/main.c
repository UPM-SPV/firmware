#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include "init/init.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);


int main(void) {
	LOG_INF("Welcome to SPV "APP_VERSION_STRING);
	init_start();

	if (!boot_is_img_confirmed()) {
		k_msleep(CONFIG_SPV_UPDATE_CONFIRM_WINDOW_MS);

		int err = boot_write_img_confirmed();
		if (err) {
			LOG_ERR("Error confirming boot image: %d", err);
			k_panic();
		}
		LOG_INF("Updated boot image confirmed");
	}

	return 0;
}

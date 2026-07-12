#include "init/init.h"

#include <zephyr/logging/log.h>

#include "timesync/timesync.h"
#include "fs/fs.h"

LOG_MODULE_REGISTER(timesync_init, LOG_LEVEL_DBG);


static void timesync_init_cb_func() {
	int err;
	time_t recovered_time;
	err = fs_try_recover_time(&recovered_time);
	if(err) {
		LOG_WRN("Unable to recover flash time, continuing without a time basis");
		return;
	}

	err = timesync_set(recovered_time, TIMESYNC_SOURCE_DEGRADED);
	if(err) {
		LOG_ERR("Unable to set recovered time: %d", err);
		k_panic();
	}

	LOG_INF("Recovered approximate current time from sensor logs: %s", ctime(&recovered_time));
}

INIT_CB_DEF(timesync_init_cb) = {
	.on_init = timesync_init_cb_func,
	.depends = (const char*[]){ "fs", 0 },
	.provides = (const char*[]){ "timesync", 0 },
};

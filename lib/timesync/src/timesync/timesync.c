#include "timesync/timesync.h"

#include <zephyr/logging/log.h>
#include <time.h>

#include "factory_data/factory_data.h"

LOG_MODULE_REGISTER(timesync, LOG_LEVEL_DBG);

static enum timesync_source_t timesync_source = TIMESYNC_SOURCE_NONE;

/*
 *
 * Interface definition.
 *
 */

int timesync_set_unchecked(time_t unix_time, enum timesync_source_t time_source) {
	int err;
	struct timespec time = {
		.tv_sec = unix_time,
		.tv_nsec = 0,
	};

	err = sys_clock_settime(SYS_CLOCK_REALTIME, &time);
	if(err) {
		LOG_ERR("Error setting time: %s", strerror(err));
		return err;
	} else {
		timesync_source = time_source;
	}

	struct timesync_status_t status = {
		.unix_time = unix_time,
		.source = time_source
	};
	STRUCT_SECTION_FOREACH(timesync_cb_t, cb) {
        if(cb->on_time_update) {
			cb->on_time_update(&status);
		}
	}

	return err;
}

int timesync_set(time_t unix_time, enum timesync_source_t time_source) {

	if(time_source < timesync_source) {
		return 0;
	}

	return timesync_set_unchecked(unix_time, time_source);
}

int timesync_status_get(struct timesync_status_t *time_status) {
	int err;
	err = timesync_source_get(&time_status->source);
	if(err) { return err; }
	return timesync_time_get(&time_status->unix_time);
}

int timesync_time_get(time_t *unix_time) {
	int err;
	struct timespec time;
	err = sys_clock_gettime(SYS_CLOCK_REALTIME, &time);
	if(err) {
		LOG_ERR("Error getting time: %s", strerror(err));
	} else {
		*unix_time = time.tv_sec;
	}

	return err;
}

int timesync_source_get(enum timesync_source_t *time_source) {
	*time_source = timesync_source;
	return 0;
}

int timesync_relatime_get(timesync_mfg_relatime_t *relatime) {
	int err;
	time_t time;

	err = timesync_time_get(&time);
	if(err) {
		return err;
	}

	*relatime = timesync_absolute_to_relatime(time);
	if(*relatime < 0) {
		return -EAGAIN;
	}
	return 0;
}

time_t timesync_relatime_to_absolute(timesync_mfg_relatime_t relatime) {
	struct factorydata_t data = factorydata_get();
	return (time_t)data.manufacturing_date + relatime;
}

timesync_mfg_relatime_t timesync_absolute_to_relatime(time_t unix_time) {
	struct factorydata_t data = factorydata_get();
	return unix_time - (time_t)data.manufacturing_date;
}

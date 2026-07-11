#include "mpsl/mpsl.h"

#include <zephyr/logging/log.h>
#include <zephyr/toolchain.h>
#include <mpsl_radio_notification.h>

LOG_MODULE_REGISTER(mpsl, LOG_LEVEL_DBG);


enum mpsl_requirement_t {
	MPSL_ON_NONE		= 0b00,
	MPSL_ON_INACTIVE	= 0b01,
	MPSL_ON_ACTIVE		= 0b10,
	MPSL_ON_BOTH		= 0b11,
};
static void on_radio_notify(mpsl_radio_notification_source_t wakeup_reason);


/*
 *
 * Public interface.
 *
 */

int mpsl_start() {

	int err;
	int mpsl_requirement = MPSL_ON_NONE;
	LOG_INF("Initializing MPSL radio callbacks");

	STRUCT_SECTION_FOREACH(mpsl_cb_t, cb) {
		if(cb->on_inactive && (mpsl_requirement & MPSL_ON_INACTIVE) == 0) {
			mpsl_requirement |= MPSL_ON_INACTIVE;
			LOG_DBG("	Enabling on inactive radio callbacks");
		}

		if(cb->on_active && (mpsl_requirement & MPSL_ON_ACTIVE) == 0) {
			mpsl_requirement |= MPSL_ON_ACTIVE;
			LOG_DBG("	Enabling on active radio callbacks");
		}

		if(mpsl_requirement == MPSL_ON_BOTH) {
			break;
		}
	}

	const char *names[] = {
		[MPSL_ON_INACTIVE]	= "on inactive",
		[MPSL_ON_ACTIVE] 	= "on active",
		[MPSL_ON_BOTH] 		= "on both",
	};
	const enum MPSL_RADIO_NOTIFICATION_TYPES callback_types[] = {
		[MPSL_ON_INACTIVE]	= MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_INACTIVE,
		[MPSL_ON_ACTIVE]	= MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_ACTIVE,
		[MPSL_ON_BOTH]		= MPSL_RADIO_NOTIFICATION_TYPE_INT_ON_BOTH,
	};

	if(mpsl_requirement == MPSL_ON_NONE) {
		LOG_DBG("	MPSL radio callbacks not enabled");
		return 0;
	}

	LOG_DBG("	Enabling %s callbacks", names[mpsl_requirement]);
	err = mpsl_radio_notification_cfg_set(
		callback_types[mpsl_requirement],
		MPSL_RADIO_NOTIFICATION_DISTANCE_MIN_US,
		on_radio_notify
	);
	if(err) {
		LOG_ERR("Error enabling MPSL radio callbacks: %d", err);
		return err;
	}

	return 0;
}


/*
 *
 * Internal functions.
 *
 */

static void on_radio_notify(mpsl_radio_notification_source_t wakeup_reason) {
	switch(wakeup_reason) {
		case MPSL_RADIO_NOTIFICATION_SOURCE_ACTIVE: {
			STRUCT_SECTION_FOREACH(mpsl_cb_t, cb) {
				if(cb->on_active) {
					cb->on_active();
				}
			}
		}
		break;

		case MPSL_RADIO_NOTIFICATION_SOURCE_INACTIVE: {
			STRUCT_SECTION_FOREACH(mpsl_cb_t, cb) {
				if(cb->on_inactive) {
					cb->on_inactive();
				}
			}
		}
		break;
	}
}

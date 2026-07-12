/*!
 * @file sensors.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Interactions with the sensors task.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_TASKS_SENSORS_H_
#define SPV_TASKS_SENSORS_H_

#include "sense/sense.h"
#include "timesync/timesync.h"

#define SENSOR_READINGS_EMPTY {													\
		.millicelsius = 0xFFFFFFFE,												\
		.centibels = 0xFFFFFFFE,												\
		.rh_permille = 0xFFFE,													\
		.lux = 0xFFFE,															\
		.voc_raw = 0xFFFFFFFE													\
	}																			\

/**
 * @brief Starts the sensors thread.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int sensors_task_start();


/**
 * @brief Holds all sensor values. A value of 0xFFFE or 0xFFFFFFFE means
 * the value is invalid.
 */
struct sensor_current_readings_t {
	sense_millicelsius_t millicelsius;
	sense_rh_permille_t rh_permille;
	sense_centibel_t centibels;
	sense_lux_t lux;
	sense_voc_raw_t voc_raw;
};

/**
 * @brief Obtain the latest sensor readings.
 *
 * @param[out] readings Current readings output.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int sensors_get_current_values(struct sensor_current_readings_t *readings);

#endif

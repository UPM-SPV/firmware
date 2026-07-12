/*!
 * @file sense.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Sensor interfaces.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_SENSE_SENSE_H_
#define SPV_SENSE_SENSE_H_

#include <zephyr/kernel.h>

/**
 * @brief Bitmap to specify which sensors should be enabled.
 */
enum sense_enable_bitmap_t {
	SENSOR_BIT_VOC = BIT(0),
	SENSOR_BIT_NOISE = BIT(1),
	SENSOR_BIT_TEMPERATURE_HUMIDITY = BIT(2),
	SENSOR_BIT_LUMINOSITY = BIT(3)
};

/**
 * @brief Bitmap to specifying which sensors are enabled.
 */
typedef uint16_t sense_enabled_t;

/**
 * @brief Millicelsius.
 */
typedef int32_t sense_millicelsius_t;
/**
 * @brief Relative humidity in permille (max of 1000 [‰], instead of 100 [%]).
 */
typedef uint16_t sense_rh_permille_t;
/**
 * @brief Centibels, cBm (tenths of a decibel).
 */
typedef int32_t sense_centibel_t;
/**
 * @brief Luminosity.
 */
typedef uint16_t sense_lux_t;
/**
 * @brief Raw VOC reading without indexing.
 */
typedef int32_t sense_voc_raw_t;

/**
 * @brief Reads temperature and humidity.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1, 2)]]
int sense_ambient_read(sense_millicelsius_t *temperature, sense_rh_permille_t *humidity);

/**
 * @brief Reads the noise level.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int sense_noise_read(sense_centibel_t *noise);

/**
 * @brief Initializes microphone-related structures.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int sense_noise_init();

/**
 * @brief Reads the luminosity level.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int sense_luminosity_read(sense_lux_t *luminosity);

/**
 * @brief Reads air quality. Requires calibration values for the sensor.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int sense_voc_read(
	sense_voc_raw_t *voc,
	const sense_millicelsius_t temperature_cal,
	const sense_rh_permille_t humidity_cal
);

#endif

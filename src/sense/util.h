/*!
 * @file util.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Utilities for sensor code.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_SENSE_UTIL_H_
#define SPV_SENSE_UTIL_H_

#define SENSE_ZEPHYR_SENSOR_VALUE_FIXED_POINT_MAGNITUDE 1000000

/**
 * @brief Transforms a split `struct sensor_value` into a fixed-point number.
 *
 * @param[in] _sensor_value `struct sensor_value`
 * @param[in] decimals Amount of decimal points to use (must be passed as a
 * power of ten).
 *
 * Examples:
 * 	- 1 decimal point	-> _decimals = 10
 * 	- 3 decimal points	-> _decimals = 1000
 */
#define SENSE_MERGE_SENSOR_VALUE(_sensor_value, _decimals) \
	(_sensor_value.val1 * _decimals + _sensor_value.val2 / (SENSE_ZEPHYR_SENSOR_VALUE_FIXED_POINT_MAGNITUDE / _decimals))


/**
 * @brief Transforms a fixed-point number into a Zephyr `struct sensor_value`.
 */
#define SENSE_SPLIT_SENSOR_VALUE(_input, _output, _decimals) 		\
	do {															\
		_output.val1 = _input / _decimals;							\
		_output.val2 = (_input - (_output.val1 * _decimals)) * (SENSE_ZEPHYR_SENSOR_VALUE_FIXED_POINT_MAGNITUDE / _decimals); \
	} while(0)

#endif

/*!
 * @file timesync.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief API for accessing wall-clock time status.
 *
 * All APIs on this file provide a non-monotonic real time clock, which may
 * skip forwards or backwards in time if updated.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_TIMESYNC_TIMESYNC_H_
#define SPV_TIMESYNC_TIMESYNC_H_

#include <time.h>

/**
 * @brief Specifies the quality of the device's time source.
 */
enum timesync_source_t {

	/**
	 * @brief The device has no information on the current time.
	 */
	TIMESYNC_SOURCE_NONE,

	/**
	 * @brief The device has obtained time information from a secondary location,
	 * such as flash storage.
	 */
	TIMESYNC_SOURCE_DEGRADED,

	/**
	 * @brief Time has been received from a peer's advertisement.
	 */
	TIMESYNC_SOURCE_PEER,

	/**
	 * @brief A mobile device has set the current time via the companion app.
	 */
	TIMESYNC_SOURCE_AUTHORITATIVE
};


/**
 * @brief Current time information.
 */
struct timesync_status_t {
	/**
	 * @brief Current time.
	 */
	time_t unix_time;

	/**
	 * @brief Quality level of the UNIX timestamp.
	 */
	enum timesync_source_t source;
};

/**
 * @brief Callback for time updates. See `TIMESYNC_CB_DEF`.
 */
[[gnu::nonnull(1)]]
typedef void (*timesync_cb_time_update_t)(const struct timesync_status_t*);

/**
 * @brief Struct for storing Timesync callbacks. Should be declared using
 * `TIMESYNC_CB_DEF`.
 */
struct timesync_cb_t {
    /**
     * @brief Called when either `timesync_set` or `timesync_set_checked` are
	 * called AND the time or time source is updated. May be NULL.
     */
     timesync_cb_time_update_t on_time_update;
};

/**
 * @brief Subscribes the specified callback to time updates. Whenever a time
 * update is successfully performed, the given function will be called.
 *
 @code
 ```
 * // Example
 * void my_time_callback_func(const struct timesync_status_t *time) {
 *     LOG_INF("Time updated! %"PRIu64, time->unix_time);
 * }
 *
 * // Create the callback.
 * TIMESYNC_CB_DEF(my_time_callback) = {
 * 	.on_time_update = my_time_callback_func
 * };
 ```
 @endcode
 */
#define TIMESYNC_CB_DEF(_name) \
	STRUCT_SECTION_ITERABLE(timesync_cb_t, _name)



/**
 * @brief Sets the current time.
 *
 * @param[in] unix_time UNIX timestamp to configure as the current time.
 * @param[in] time_source Quality level of the UNIX timestamp.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int timesync_set_unchecked(time_t unix_time, enum timesync_source_t time_source);

/**
 * @brief Sets the current time, if the given source is at least as good
 * as the current time source.
 *
 * @param[in] unix_time UNIX timestamp to configure as the current time.
 * @param[in] time_source Quality level of the UNIX timestamp.
 *
 * @return int 0 if successful or time unchanged.
 */
[[nodiscard]]
int timesync_set(time_t unix_time, enum timesync_source_t time_source);

/**
 * @brief Returns the current time status.
 *
 * @param[out] time_status Output time status.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int timesync_status_get(struct timesync_status_t *time_status);

/**
 * @brief Returns the current time.
 *
 * @param[out] unix_time Output time.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int timesync_time_get(time_t *unix_time);

/**
 * @brief Returns the current time source.
 *
 * @param[out] time_source Output time source.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int timesync_source_get(enum timesync_source_t *time_source);


/**
 * @brief Stores time as an *offset from the manufacturing date*.
 */
typedef time_t timesync_mfg_relatime_t;

/**
 * @brief Obtains the UNIX time of the system and subtracts the manufacturing
 * date.
 *
 * @param[out] relatime Time relative to the device's manufacturing date.
 *
 * @note This function will fail if the current time is behind the
 * manufacturing time.
 *
 * @return int 0 if successful.
 *
 */
[[nodiscard, gnu::nonnull(1)]]
int timesync_relatime_get(timesync_mfg_relatime_t *relatime);

/**
 * @brief Transforms the given relative time into an absolute UNIX timestamp.
 */
[[nodiscard]]
time_t timesync_relatime_to_absolute(timesync_mfg_relatime_t relatime);

/**
 * @brief Transforms the given UNIX timestamp a one relative to the
 * manufacturing date.
 *
 * @note This function will return negative values if `unix_time` is behind
 * the manufacturing date.
 */
[[nodiscard]]
timesync_mfg_relatime_t timesync_absolute_to_relatime(time_t unix_time);

#endif

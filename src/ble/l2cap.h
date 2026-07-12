/*!
 * @file l2cap.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Utilities for managing send-only L2CAP channels.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_BLE_L2CAP_H_
#define SPV_BLE_L2CAP_H_

#include <zephyr/bluetooth/l2cap.h>

/**
 * @brief Contains an L2CAP channel and its configuration.
 */
struct ble_l2cap_chan_t {
	/**
	 * @brief L2CAP channel.
	 */
	struct bt_l2cap_le_chan *chan;

	/**
	 * @brief Channel ID.
	 */
	uint16_t psm;
};

/**
 * @brief Initializes the internal L2CAP pool.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int ble_l2cap_init();

/**
 * @brief Requests an L2CAP channel from the pool.
 *
 * @note Users must call `ble_l2cap_put` when they are done using the channel.
 *
 * @param[out] channel Output channel.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int ble_l2cap_get(struct ble_l2cap_chan_t **channel);

/**
 * @brief Returns an L2CAP channel to the pool.
 *
 * @param[in] channel Channel to free.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int ble_l2cap_put(struct ble_l2cap_chan_t *channel);


/**
 * @brief Stores a buffer and its corresponding size.
 */
struct ble_l2cap_data_t {
	const void *data;
	size_t size;
};

/**
 * @brief Enqueues the given data to be sent over an L2CAP channel.
 *
 * @param[in] channel L2CAP channel to use.
 * @param[in] data Pointer to data array, will be considered to end when
 * a `0` struct is found. Data is copied to an internal buffer.
 * @param[in] timeout Maximum time to wait enqueueing the data before the
 * operation is considered a failure.
 *
 * @return int 0 if successful.
 *
 * ## Example
 @code
 ```
 * int err;
 *
 * // Get a channel handle.
 * struct ble_l2cap_chan_t *channel;
 * err = ble_l2cap_get(&channel);
 * if(err) {
 * 	LOG_ERR("Error obtaining channel! %d", err);
 * 	return;
 * }
 *
 * // Send a message with a custom header.
 * const char my_buffer[32] = "Hello from L2CAP!";
 * const char header[] = { 0xF1, 0xF3, strlen(my_buffer) };
 *
 * err = ble_l2cap_send_enqueue(
 * 	channel,
 * 	(ble_l2cap_data_t[]) {
 * 		{ header,		sizeof(header) 		},
 * 		{ my_buffer,	strlen(my_buffer)	},
 * 		{ 0 },
 * 	},
 * 	K_NO_WAIT
 * );
 * if(err) {
 * 	LOG_ERR("Error sending data! %d", err);
 * }
 *
 * // Return the channel to the pool.
 * ble_l2cap_put(channel);
 ```
 @endcode
 */
[[nodiscard, gnu::nonnull(1, 2)]]
int ble_l2cap_send_enqueue(
	const struct ble_l2cap_chan_t *channel,
	const struct ble_l2cap_data_t *data,
	k_timeout_t timeout
);

#endif

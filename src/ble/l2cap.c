#include "ble/l2cap.h"

#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/l2cap.h>

#define L2CAP_POOL_SIZE 		CONFIG_BT_MAX_CONN * CONFIG_SPV_BLE_L2CAP_CHANNELS_PER_CONNECTION
#define L2CAP_NETBUFFER_COUNT	CONFIG_BT_MAX_CONN * CONFIG_SPV_BLE_L2CAP_BUFFERS_PER_CONNECTION
#define L2CAP_NETBUFFER_SIZE	CONFIG_SPV_BLE_L2CAP_BUFFER_SIZE
#define L2CAP_MSG_QUEUE_SIZE	CONFIG_SPV_BLE_L2CAP_MSG_QUEUE_SIZE

LOG_MODULE_REGISTER(ble_l2cap, LOG_LEVEL_DBG);

struct ble_l2cap_chan_state_t {
	struct ble_l2cap_chan_t channel;
	struct k_msgq msg_queue;
	struct k_work work_queue;
	bool is_reserved:1;
};

struct ble_l2cap_msg_t {
	struct net_buf *buf;
} __attribute__ ((aligned(4))); // Required by message queues.

// Share buffers between all connections.
NET_BUF_POOL_DEFINE(
	l2cap_netbuffer_pool,
	L2CAP_NETBUFFER_COUNT, L2CAP_NETBUFFER_SIZE, CONFIG_BT_CONN_TX_USER_DATA_SIZE,
	NULL
);
K_THREAD_STACK_DEFINE(l2cap_worker_stack, 8192);
static struct k_work_q l2cap_work_queue;
static struct ble_l2cap_msg_t msg_buffers[L2CAP_POOL_SIZE][L2CAP_MSG_QUEUE_SIZE] = { 0 };
static struct bt_l2cap_server server_params[L2CAP_POOL_SIZE];

static K_MUTEX_DEFINE(pool_mutex);
static struct bt_l2cap_le_chan channel_pool[L2CAP_POOL_SIZE] = { 0 };
static struct ble_l2cap_chan_state_t channel_pool_data[L2CAP_POOL_SIZE] = { 0 };

static int l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf);
static void l2cap_send_work_handler(struct k_work *work);
static int l2cap_channel_get_by_psm(struct ble_l2cap_chan_state_t **channel, int16_t psm);
static int l2cap_accept(
	struct bt_conn *conn, struct bt_l2cap_server *server,
	struct bt_l2cap_chan **chan
);

static const struct bt_l2cap_chan_ops default_l2cap_ops = {
	.recv = l2cap_recv_cb
};


/*
 *
 * Interface definitions.
 *
 */

int ble_l2cap_init() {
	int err;
	LOG_INF("Initializing L2CAP channels");

	k_work_queue_start(
		&l2cap_work_queue, l2cap_worker_stack,
		K_THREAD_STACK_SIZEOF(l2cap_worker_stack),
		10, NULL
	);

	uint16_t psm = 0x81;
	for(size_t i = 0; i < ARRAY_SIZE(channel_pool); i++, psm += 2) {
		LOG_DBG("	Initializing channel	%zu", i);
		server_params[i] = (struct bt_l2cap_server){
			.psm = psm,
			.sec_level = BT_SECURITY_L2,
			.accept = l2cap_accept,
		};
		err = bt_l2cap_server_register(&server_params[i]);
		if(err) {
			LOG_ERR("Error initializing L2CAP channel: %d", err);
			return err;
		}
		LOG_DBG("		PSM:	%"PRIx16, psm);


		channel_pool_data[i] = (struct ble_l2cap_chan_state_t){
			.channel = {
				.chan = &channel_pool[i],
				.psm = psm
			},
			.is_reserved = false,
		};

		k_msgq_init(
			&channel_pool_data[i].msg_queue, (char*)msg_buffers[i],
			sizeof(struct ble_l2cap_msg_t), L2CAP_MSG_QUEUE_SIZE
		);

		k_work_init(&channel_pool_data[i].work_queue, l2cap_send_work_handler);
	}

	return 0;
}

int ble_l2cap_get(struct ble_l2cap_chan_t **channel) {
	int err;
	LOG_DBG("L2CAP channel requested");

	err = k_mutex_lock(&pool_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking pool mutex: %d", err);
		return err;
	}

	for(size_t i = 0; i < ARRAY_SIZE(channel_pool); i++) {
		struct ble_l2cap_chan_state_t *state = &channel_pool_data[i];
		if(!state->is_reserved) {
			state->is_reserved = true;
			k_mutex_unlock(&pool_mutex);

			state->channel.chan->chan.ops = &default_l2cap_ops;
			*channel = &state->channel;
			LOG_DBG("	Reserved L2CAP channel with PSM %"PRIx16, state->channel.psm);
			return 0;
		}
	}

	k_mutex_unlock(&pool_mutex);
	LOG_ERR("No L2CAP channels available");
	return -ENOMEM;
}

int ble_l2cap_put(struct ble_l2cap_chan_t *channel) {
	int err;
	LOG_DBG("L2CAP channel freed");

	err = k_mutex_lock(&pool_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking pool mutex: %d", err);
		return err;
	}

	struct ble_l2cap_chan_state_t *state = CONTAINER_OF(
		channel, struct ble_l2cap_chan_state_t, channel
	);
	state->is_reserved = false;
	k_mutex_unlock(&pool_mutex);

	LOG_DBG("	Freed channel %"PRIx16, state->channel.psm);
	return 0;
}

int ble_l2cap_send_enqueue(
	const struct ble_l2cap_chan_t *channel,
	const struct ble_l2cap_data_t *data,
	k_timeout_t timeout
) {
	int err;

	LOG_DBG("Enqueuing L2CAP data");

	struct ble_l2cap_chan_state_t *state = CONTAINER_OF(
		channel, struct ble_l2cap_chan_state_t, channel
	);

	// Zephyr does not support sending fragmented buffers over L2CAP.
    // To avoid allocating egregiously big buffers, we must then fragment
    // them ourselves.
	struct net_buf *head_buffer = NULL;

	struct net_buf *current_buffer = head_buffer;
	size_t available_buffer_bytes = 0;
	size_t payload_remaining_bytes = data->size;
	const char *data_ptr = data->data;
	while(data->data != NULL) {

		if(available_buffer_bytes == 0) {
			LOG_DBG("	Allocating new buffer");
			struct net_buf *next_buffer = net_buf_alloc(
				&l2cap_netbuffer_pool, timeout
			);
			if(next_buffer == NULL) {
				LOG_ERR("Error allocating buffer");
				if(head_buffer) {
					net_buf_unref(head_buffer);
				}
				return -ENOMEM;
			}
			if(head_buffer == NULL) {
				current_buffer = head_buffer = next_buffer;
			} else {
				net_buf_frag_add(current_buffer, next_buffer);
				current_buffer = next_buffer;
			}

			LOG_DBG("		Allocating L2CAP header");
			net_buf_reserve(current_buffer, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
			available_buffer_bytes = L2CAP_NETBUFFER_SIZE - BT_L2CAP_SDU_CHAN_SEND_RESERVE;
		}

		size_t bytes_to_write = min(payload_remaining_bytes, available_buffer_bytes);
		size_t written_bytes = net_buf_append_bytes(
			current_buffer,
			bytes_to_write, data_ptr,
			timeout, NULL, NULL
		);

		if(written_bytes != bytes_to_write) {
			LOG_ERR(
				"Error allocating buffer when writing %zu Bytes, could only write %zu",
				bytes_to_write, written_bytes
			);
			if(head_buffer) {
				net_buf_unref(head_buffer);
			}
			return -ENOMEM;
		}

		LOG_DBG("		Written %zu Bytes from %p", written_bytes, (void*)data_ptr);

		payload_remaining_bytes -= written_bytes;
		data_ptr += written_bytes;
		available_buffer_bytes -= written_bytes;

		if(payload_remaining_bytes == 0) {
			data++;

			data_ptr = data->data;
			payload_remaining_bytes = data->size;
		}
	}

	struct ble_l2cap_msg_t msg = {
		.buf = head_buffer
	};

	LOG_DBG("	Enqueueing into message queue");
	err = k_msgq_put(&state->msg_queue, &msg, timeout);
	if(err) {
		LOG_WRN("Error allocating L2CAP buffer");
		if(head_buffer) {
			net_buf_unref(head_buffer);
		}
		return err;
	}

	k_work_submit(&state->work_queue);

	return err;
}

/*
 *
 * Internal functions.
 *
 */

static void l2cap_send_work_handler(struct k_work *work) {
	int err;

	struct ble_l2cap_chan_state_t *state = CONTAINER_OF(
		work, struct ble_l2cap_chan_state_t, work_queue
	);
	LOG_DBG("L2CAP work handler invoked for channel %"PRIx16, state->channel.psm);

	err = 0;
	while(!err) {
		struct ble_l2cap_msg_t msg;
		err = k_msgq_get(&state->msg_queue, &msg, K_NO_WAIT);
		if(err) {
			break;
		}

		// Zephyr's l2cap implementation doesn't support fragmented buffers.
		// We have to send each fragment one by one.

		LOG_DBG("Starting buffer transmission");
		struct net_buf *current_buf = msg.buf;
		while(current_buf != NULL) {
			// Keep track of the next buffer...
			struct net_buf *next_frag = current_buf->frags;
			// ...and unlink it so that `bt_l2cap_chan_send` is none the wiser.
			current_buf->frags = NULL;

			LOG_DBG("	Sending unrolled buffer %p of %zu Bytes", current_buf, current_buf->len);
			int err = bt_l2cap_chan_send(&state->channel.chan->chan, current_buf);
			if(err) {
				LOG_ERR("Error transmitting L2CAP data: %d", err);
				// If transmission fails, clean up the entire buffer.
				net_buf_unref(current_buf);
				if(next_frag) {
					// Next fragment was explicitly unlinked, free it too.
					net_buf_unref(next_frag);
				}
				next_frag = NULL;
			}

			current_buf = next_frag;
		}
	}

	LOG_DBG("Done sending queued messages for channel %"PRIx16, state->channel.psm);
}

static int l2cap_accept(
	struct bt_conn *conn, struct bt_l2cap_server *server,
	struct bt_l2cap_chan **chan
) {
	int err;
	LOG_INF("L2CAP connection received");
	err = k_mutex_lock(&pool_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking pool mutex: %d", err);
		return err;
	}

	LOG_DBG("	Received connection request for PSM %"PRIx16, server->psm);
	struct ble_l2cap_chan_state_t *channel;
	err = l2cap_channel_get_by_psm(&channel, server->psm);
	if(err) {
		k_mutex_unlock(&pool_mutex);
		LOG_WRN("L2CAP channel with PSM %"PRIx16" not found", server->psm);
		return err;
	}

	if(!channel->is_reserved) {
		k_mutex_unlock(&pool_mutex);
		LOG_WRN(
			"Invalid connection attempt to unused channel with PSM %"PRIx16,
			server->psm
		);
		return -ENOLINK;
	}

	*chan = &channel->channel.chan->chan;
	k_mutex_unlock(&pool_mutex);

	LOG_DBG("	Accepted L2CAP channel");
	return 0;
}

static int l2cap_channel_get_by_psm(struct ble_l2cap_chan_state_t **channel, int16_t psm) {
	for(size_t i = 0; i < ARRAY_SIZE(channel_pool); i++) {
		if(channel_pool_data[i].channel.psm == psm) {
			*channel = &channel_pool_data[i];
			return 0;
		}
	}

	return -ENOENT;
}

// Required by Zephyr for no reason.
// (Even has an internal check to *make you* declare it).
static int l2cap_recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf) {
	return 0;
}

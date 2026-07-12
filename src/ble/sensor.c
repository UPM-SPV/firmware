#include "ble/ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/l2cap.h>

#include <zephyr/logging/log.h>

#include <zephyr/app_version.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "res/protobuf/sensor_data_request.pb.h"
#include "ble/ctx.h"
#include "ble/uuid.h"
#include "ble/l2cap.h"
#include "fs/fs.h"

LOG_MODULE_REGISTER(ble_sds, LOG_LEVEL_DBG);

#define CURRENT_SCHEMA	1
#define SUMMARY_HEADER_MAGIC	0x3DB74FB8
#define DATA_HEADER_MAGIC		0xADAEEDD7

#define BLE_UUID_SDS 			BT_UUID_SPV_DECLARE(0x8010)
#define BLE_UUID_SDS_CONTROL 	BT_UUID_SPV_DECLARE(0x8011)


struct sds_svc_state {
	struct ble_l2cap_chan_t *l2cap_chan;
};

static int on_connected(struct bt_conn *conn, struct sds_svc_state *ctx);
static void on_disconnected(struct bt_conn *conn, uint8_t reason, struct sds_svc_state *ctx);
static int sds_request_decode(SdsRequest *out, const char *buffer, size_t size);
static int sds_metadata_encode(char *buffer, size_t *size, const SdsServiceMetadata *data);
static int sds_summary_header_encode(char *buffer, size_t *size, const SdsSummaryResponseHeader *data);
static int sds_data_header_encode(char *buffer, size_t *size, const SdsDataResponseHeader *data);

SPV_BT_CONN_CTX_DEF_CB(ble_sds, CONFIG_BT_MAX_CONN, struct sds_svc_state, on_connected, on_disconnected);

static ssize_t sds_control_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	int err;
	LOG_DBG("Reading SDS metadata");
	SPV_BT_CTX_GET_OR_ERR(ctx, ble_sds, conn, struct sds_svc_state);
	SdsServiceMetadata metadata = {
		.schema_version = CURRENT_SCHEMA,
		.l2cap_psm = ctx->l2cap_chan->psm,
	};
	SPV_BT_CTX_RELEASE(ctx, ble_sds);

	char encoded_buffer[SdsServiceMetadata_size];
	size_t encoded_size;
	err = sds_metadata_encode(encoded_buffer, &encoded_size, &metadata);
	if(err) { return err; }

	LOG_DBG("	Successfully sent SDS metadata:");
	LOG_DBG("		PSM:	%"PRIx16, metadata.l2cap_psm);

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		encoded_buffer, encoded_size
	);
}

static ssize_t sds_control_write(
	struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf,
	uint16_t len, uint16_t offset, uint8_t flags
) {
	if(offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if(len == 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	int err;

	LOG_DBG("Processing SDS request");

	SdsRequest decoded_request;
	err = sds_request_decode(&decoded_request, buf, len);
	if(err) { return err; }

	switch(decoded_request.which_message) {
		case SdsRequest_summary_request_tag: {
			LOG_DBG("	Received summary request");
			const struct fs_summary_t *summaries;
			size_t summary_count;
			fs_summary_get_all(&summaries, &summary_count);

			SdsSummaryResponseHeader header = {
				.magic = SUMMARY_HEADER_MAGIC,
				.summary_count = summary_count
			};
			LOG_DBG("	Sending %zu summary blocks", summary_count);

			char encoded_header[SdsSummaryResponseHeader_size];
			size_t encoded_size;
			err = sds_summary_header_encode(encoded_header, &encoded_size, &header);
			if(err) { return err; }

			SPV_BT_CTX_GET_OR_ERR(ctx, ble_sds, conn, struct sds_svc_state);
			err = ble_l2cap_send_enqueue(
				ctx->l2cap_chan,
				(struct ble_l2cap_data_t[]) {
					{ encoded_header,	encoded_size 						},
					{ summaries,		summary_count * sizeof(*summaries)	},
					{ 0 }
				},
				K_NO_WAIT
			);
			SPV_BT_CTX_RELEASE(ctx, ble_sds);
			if(err) {
				LOG_ERR("Error sending L2CAP summaries: %d", err);
				return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
			}
		}
		break;


		case SdsRequest_data_request_tag: {
			LOG_DBG("	Received data request");
			SdsDataRequest *data_request = &decoded_request.message.data_request;
			const struct fs_summary_t *summary;
			err = fs_summary_get(&summary, data_request->period_index);
			if(err) {
				LOG_ERR("Error getting summary: %d", err);
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
			}
			if(summary->unix_time_start != data_request->period_unix_date) {
				LOG_ERR("Error processing request: stale request");
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
			}
			if(data_request->reading_offset_start >= summary->final_count) {
				LOG_ERR("Error processing request: read offset beyond data log");
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
			}
			const struct fs_data_entry_t *entries;
			err = fs_sector_get_all(&entries, data_request->period_index);
			if(err) {
				LOG_ERR("Error obtaining data sector: %d", err);
				return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
			}

			size_t data_block_count = (summary->final_count - data_request->reading_offset_start);
			SdsDataResponseHeader header = {
				.magic = DATA_HEADER_MAGIC,
				.data_block_count = data_block_count
			};

			LOG_DBG("	Sending %zu data blocks", data_block_count);
			char encoded_header[SdsDataResponseHeader_size];
			size_t encoded_size;
			err = sds_data_header_encode(encoded_header, &encoded_size, &header);
			if(err) { return err; }

			SPV_BT_CTX_GET_OR_ERR(ctx, ble_sds, conn, struct sds_svc_state);
			err = ble_l2cap_send_enqueue(
				ctx->l2cap_chan,
				(struct ble_l2cap_data_t[]) {
					{ encoded_header,	encoded_size },
					{
						entries + data_request->reading_offset_start,
						data_block_count * sizeof(*entries),
					},
					{ 0 }
				},
				K_NO_WAIT
			);
			SPV_BT_CTX_RELEASE(ctx, ble_sds);
			if(err) {
				LOG_ERR("Error sending L2CAP data sector: %d", err);
				return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
			}
		}
		break;
	}

	return len;
}

static int on_connected(struct bt_conn *conn, struct sds_svc_state *ctx) {
	int err;

	LOG_DBG("Requesting SDS L2CAP channel");
	err = ble_l2cap_get(&ctx->l2cap_chan);
	if(err) {
		LOG_ERR("Error obtaining L2CAP channel for SDS service, disconnecting: %d", err);
		return err;
	}

	LOG_DBG("	Successfully opened L2CAP channel %"PRIx16, ctx->l2cap_chan->psm);
	return 0;
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason, struct sds_svc_state *ctx) {
	int err;
	LOG_DBG("Closing L2CAP channel");
	err = ble_l2cap_put(ctx->l2cap_chan);
	if(err) {
		LOG_ERR("Error returning L2CAP channel");
	}
}


BT_GATT_SERVICE_DEFINE(
	sds_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_SDS),

	BT_GATT_CHARACTERISTIC(
		BLE_UUID_SDS_CONTROL,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
		sds_control_read, sds_control_write,
		NULL
	)
);


static int sds_request_decode(SdsRequest *out, const char *buffer, size_t size) {
	pb_istream_t stream = pb_istream_from_buffer(buffer, size);
	if(!pb_decode(&stream, SdsRequest_fields, out)) {
		LOG_ERR("Error decoding data request");
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
	}
	if(out->schema_version != CURRENT_SCHEMA) {
		LOG_ERR("Unsupported schema: %"PRIu32, out->schema_version);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_PDU);
	}

	return 0;
}

static int sds_metadata_encode(char *buffer, size_t *size, const SdsServiceMetadata *data) {
	pb_ostream_t stream;
	stream = pb_ostream_from_buffer(buffer, SdsServiceMetadata_size);
	if(!pb_encode(&stream, SdsServiceMetadata_fields, data)) {
		LOG_ERR("Error encoding service metadata");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	*size = stream.bytes_written;
	return 0;
}

static int sds_summary_header_encode(char *buffer, size_t *size, const SdsSummaryResponseHeader *data) {
	pb_ostream_t stream;
	stream = pb_ostream_from_buffer(buffer, SdsSummaryResponseHeader_size);
	if(!pb_encode(&stream, SdsSummaryResponseHeader_fields, data)) {
		LOG_ERR("Error encoding summary header");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	*size = stream.bytes_written;
	return 0;
}

static int sds_data_header_encode(char *buffer, size_t *size, const SdsDataResponseHeader *data) {
	pb_ostream_t stream;
	stream = pb_ostream_from_buffer(buffer, SdsDataResponseHeader_size);
	if(!pb_encode(&stream, SdsDataResponseHeader_fields, data)) {
		LOG_ERR("Error encoding data header");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	*size = stream.bytes_written;
	return 0;
}

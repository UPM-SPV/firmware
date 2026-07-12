#include "ble/ble.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>

#include <zephyr/app_version.h>

#include "factory_data/factory_data.h"
#include "attest/attest.h"
#include "ble/ctx.h"
#include "ble/uuid.h"

LOG_MODULE_REGISTER(ble_attestation, LOG_LEVEL_DBG);


#define BLE_UUID_ATTEST 			BT_UUID_SPV_DECLARE(0x8020)
#define BLE_UUID_ATTEST_CHALLENGE 	BT_UUID_SPV_DECLARE(0x8021)
#define BLE_UUID_ATTEST_RESPONSE 	BT_UUID_SPV_DECLARE(0x8022)
#define BLE_UUID_ATTEST_CERT 		BT_UUID_SPV_DECLARE(0x8023)


struct attestation_svc_state {
	struct bt_gatt_indicate_params challenge_indicate_params;
	char signature[CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE];
	bool signature_valid;
	bool notifications;
};

SPV_BT_CONN_CTX_DEF(ble_attestation, CONFIG_BT_MAX_CONN, struct attestation_svc_state);


static void indicate_release_ctx(struct bt_gatt_indicate_params *params) {
	struct attestation_svc_state *ctx = CONTAINER_OF(params, struct attestation_svc_state, challenge_indicate_params);
	ctx->signature_valid = false;
    SPV_BT_CTX_RELEASE(ctx, ble_attestation);

	LOG_DBG("	Attestation indication successful");
}

static ssize_t attest_challenge_write(
	struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	const void *buf,
	uint16_t len, uint16_t offset, uint8_t flags
) {
	int err;
	LOG_DBG("Received attestation challenge");
	SPV_BT_CTX_GET_OR_ERR(ctx, ble_attestation, conn, struct attestation_svc_state);

	if(len != CONFIG_SPV_ATTESTATION_CHALLENGE_LENGTH) {
		SPV_BT_CTX_RELEASE(ctx, ble_attestation);
		LOG_WRN("Wrong challenge write length: %"PRIu16, len);
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	err = attest_sign(ctx->signature, buf, len);
	if(err) {
		SPV_BT_CTX_RELEASE(ctx, ble_attestation);
		LOG_ERR("Error signing attestation challenge: %d", err);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	LOG_DBG("	Attestation signature successful");
	ctx->signature_valid = true;
	if(ctx->notifications) {
		LOG_DBG("	Sending via indications");
		ctx->challenge_indicate_params.uuid = BLE_UUID_ATTEST_RESPONSE;
		ctx->challenge_indicate_params.data = ctx->signature;
		ctx->challenge_indicate_params.len = sizeof(ctx->signature);
		ctx->challenge_indicate_params.destroy = indicate_release_ctx;

		bt_gatt_indicate(conn, &ctx->challenge_indicate_params);
		// Only release when indication has been received (!).
	} else {
		SPV_BT_CTX_RELEASE(ctx, ble_attestation);
	}
	return CONFIG_SPV_ATTESTATION_CHALLENGE_LENGTH;
}

static ssize_t attest_response_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Received attestation response read request");
	SPV_BT_CTX_GET_OR_ERR(ctx, ble_attestation, conn, struct attestation_svc_state);

	if(!ctx->signature_valid) {
		LOG_WRN("No attestation signatures available");
		return BT_GATT_ERR(BT_ATT_ERR_READ_NOT_PERMITTED);
	}

	ssize_t error = bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		ctx->signature, sizeof(ctx->signature)
	);

	ctx->signature_valid = false;
	SPV_BT_CTX_RELEASE(ctx, ble_attestation);
	LOG_DBG("	Sent %zd signature Bytes", error);
	return error;
}

static ssize_t attest_response_indication_write(
	struct bt_conn *conn,
	const struct bt_gatt_attr *attr,
	uint16_t value
) {
	SPV_BT_CTX_GET_OR_ERR(ctx, ble_attestation, conn, struct attestation_svc_state);
	ctx->notifications = (value == BT_GATT_CCC_INDICATE);
	SPV_BT_CTX_RELEASE(ctx, ble_attestation);
	LOG_DBG("Attestation indications %s", ctx->notifications ? "enabled" : "disabled");
	return sizeof(value);
}


static ssize_t attest_cert_read(
	struct bt_conn *conn, const struct bt_gatt_attr *attr,
	void *buf, uint16_t buf_len, uint16_t offset
) {
	LOG_DBG("Reading attestation certificate");
	struct factorydata_t data = factorydata_get();

	return bt_gatt_attr_read(
		conn, attr, buf, buf_len, offset,
		data.device_cert, data.device_cert_size
	);
}

static struct bt_gatt_ccc_managed_user_data attestation_response_indication_ccc_data =
		BT_GATT_CCC_MANAGED_USER_DATA_INIT(NULL, attest_response_indication_write, NULL);

BT_GATT_SERVICE_DEFINE(
	attestation_svc,
	BT_GATT_PRIMARY_SERVICE(BLE_UUID_ATTEST),

	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ATTEST_CHALLENGE,
		BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE_ENCRYPT,
		NULL, attest_challenge_write,
		NULL
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ATTEST_RESPONSE,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE, BT_GATT_PERM_READ_ENCRYPT,
		attest_response_read, NULL,
		NULL
	),
	BT_GATT_CCC_MANAGED(
		&attestation_response_indication_ccc_data,
		BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT
	),
	BT_GATT_CHARACTERISTIC(
		BLE_UUID_ATTEST_CERT,
		BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
		attest_cert_read, NULL,
		NULL
	)
);

#include "factory_data/factory_data.h"

#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/storage/stream_flash.h>

#include <pb_decode.h>
#include <pb_encode.h>
#include "factory_data.pb.h"

#include "attest/attest.h"

LOG_MODULE_REGISTER(factorydata, LOG_LEVEL_DBG);

#define FACTORY_PARTITION PARTITION_ID(factory_data)
#define FACTORY_PARTITION_SIZE PARTITION_SIZE(factory_data)
#define BUFFER_SIZE 128
#define FACTORYDATA_MAGIC 0xFAC1DA1A


static FactoryData factory_data_pb;

/**
 * @brief Used for tracking streamed Flash reads.
 */
struct flash_stream_ctx_t {
    const struct flash_area *fa;
    off_t offset;
};

static bool factorydata_is_configured();
static bool flash_stream_read_cb(pb_istream_t *stream, uint8_t *buf, size_t count);
static bool flash_stream_write_cb(pb_ostream_t *stream, const uint8_t *buf, size_t count);


/*
 *
 * Interface definition.
 *
 */

/**
 * Reads data from flash and stores it in a singleton.
 */
int factorydata_load() {
	int err;
	const struct flash_area *flash;

	err = flash_area_open(FACTORY_PARTITION, &flash);
	if(err) {
		LOG_ERR("Loading data: error opening flash: %s", strerror(err));
		return err;
	}


	struct flash_stream_ctx_t stream_ctx = {
		.fa = flash,
		.offset = 0
	};
	pb_istream_t stream = {
		.callback = flash_stream_read_cb,
		.state = &stream_ctx,
		.bytes_left = FACTORY_PARTITION_SIZE
	};

	if(!pb_decode_delimited(&stream, FactoryData_fields, &factory_data_pb)) {
		LOG_ERR("Loading data: error decoding flash data: %s", stream.errmsg);
		err = -EINVAL;
		goto err_close_flash;
	}

	if(factory_data_pb.magic != FACTORYDATA_MAGIC) {
		LOG_ERR("Loading data: flash data not initialized: %s", stream.errmsg);
		err = -EINVAL;
		goto err_close_flash;
	}

	if(factory_data_pb.private_key.size != 0) {
		LOG_INF("Found private key, provisioning");
		err = attest_key_import(factory_data_pb.private_key.bytes, factory_data_pb.private_key.size);
		if(err) {
			LOG_ERR("Loading data: error importing private key");
			goto err_close_flash;
		}

		LOG_INF("Deleting key from flash");
		memset(factory_data_pb.private_key.bytes, 0, factory_data_pb.private_key.size);
		factory_data_pb.private_key.size = 0;

		err = flash_area_erase(flash, 0, FACTORY_PARTITION_SIZE);
        if (err) {
            LOG_ERR("Failed to erase flash partition for rewrite: %d", err);
            goto err_close_flash;
        }

		struct flash_stream_ctx_t write_ctx = {
			.fa = flash,
			.offset = 0,
		};
		pb_ostream_t ostream = {
			.callback = flash_stream_write_cb,
			.state = &write_ctx,
			.max_size = FACTORY_PARTITION_SIZE,
			.bytes_written = 0
		};

		if(!pb_encode_delimited(&ostream, FactoryData_fields, &factory_data_pb)) {
			LOG_ERR("Failed to rewrite factory data: %s", ostream.errmsg);
			err = -EIO;
			goto err_close_flash;
		}
	}

	LOG_INF("Factory data successfully loaded");

	flash_area_close(flash);
	return 0;

	err_close_flash:
	flash_area_close(flash);
	return err;
}

/**
 * Return pointers to `factory_data_pb`.
 */
struct factorydata_t factorydata_get() {

	__ASSERT(factorydata_is_configured(), "Factorydata not configured");

	struct factorydata_t data = {
		.manufacturing_date = factory_data_pb.manufacturing_date,
		.board_revision = factory_data_pb.board_revision,
		.serial_number = factory_data_pb.serial_number,
		.device_cert = factory_data_pb.device_cert.bytes,
		.device_cert_size = factory_data_pb.device_cert.size
	};

	return data;
}



/*
 *
 * Internal functions.
 *
 */


/**
 * @brief Check if the factory data has been loaded.
 */
[[maybe_unused]]
static bool factorydata_is_configured() {
	return factory_data_pb.magic == FACTORYDATA_MAGIC;
}

/**
 * @brief Used by nanopb for reading flash data.
 */
static bool flash_stream_read_cb(pb_istream_t *stream, uint8_t *buf, size_t count) {
	struct flash_stream_ctx_t *ctx = (struct flash_stream_ctx_t *)stream->state;

    int rc = flash_area_read(ctx->fa, ctx->offset, buf, count);
    if (rc != 0) {
        return false;
    }

    ctx->offset += count;
    return true;
}

/**
 * @brief Used by nanopb for writing updated data back to flash.
 */
static bool flash_stream_write_cb(pb_ostream_t *stream, const uint8_t *buf, size_t count) {
    struct flash_stream_ctx_t *ctx = (struct flash_stream_ctx_t *)stream->state;

    int rc = flash_area_write(ctx->fa, ctx->offset, buf, count);
    if (rc != 0) {
        return false;
    }

    ctx->offset += count;
    return true;
}

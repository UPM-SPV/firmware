#include "attest/attest.h"

#include <zephyr/logging/log.h>

#include <psa/crypto.h>
#include <cracen_psa_kmu.h>
#include <cracen_psa_key_ids.h>

LOG_MODULE_REGISTER(attest, LOG_LEVEL_DBG);

#define PSA_KEY_ID (PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW,CONFIG_SPV_ATTESTATION_KEYID))


static bool is_key_provisioned();
/*
 *
 * Interface definition.
 *
 */

/**
 * Initialize the PSA API.
 */
int attest_init() {

	psa_status_t status;

	status = psa_crypto_init();
	if(status != PSA_SUCCESS) {
		LOG_ERR("Error initializing PSA: %"PRId32, status);
		return -EINVAL;
	}

	if(!is_key_provisioned()) {
		LOG_ERR("Identity key is not provisioned.");
		return -EINVAL;
	}

	LOG_INF("Attestation initialized.");
	return 0;
}

/**
 * Import a private key into the KMU.
 */
int attest_key_import(const char *key, size_t keylength) {

	int err = 0;
	psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t imported_key_id;

	// PSA is also initialized here.
    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        LOG_ERR("PSA init failed: %d", status);
        return 0;
    }

	psa_set_key_lifetime(&attributes,
		PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
			PSA_KEY_PERSISTENCE_DEFAULT,
			PSA_KEY_LOCATION_CRACEN_KMU
		)
	);
    psa_set_key_id(&attributes, PSA_KEY_ID);

    // Configure key for secp256r1
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));

    status = psa_import_key(&attributes, key, keylength, &imported_key_id);

    if (status == PSA_SUCCESS) {
        LOG_INF("Private key provisioned with ID: 0x%08X", imported_key_id);
    } else if (status == PSA_ERROR_ALREADY_EXISTS) {
        LOG_ERR("Private key is already provisioned");
		err = -EINVAL;
    } else {
        LOG_ERR("Failed to provision private key. Error code: %d", status);
		err = -EINVAL;
    }

    psa_reset_key_attributes(&attributes);
    return err;
}

/**
 * Use the PSA API directly to sign the message.
 */
int attest_sign(
	char signature[CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE],
	const char *message, size_t message_size
) {
	size_t signature_size;

	psa_status_t status = psa_sign_message(
		PSA_KEY_ID,
		PSA_ALG_ECDSA(PSA_ALG_SHA_256),
		message,
		message_size,
		signature,
		CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE,
		&signature_size
	);

	if(status != PSA_SUCCESS || signature_size != CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE) {
		return -EINVAL;
	}

	return 0;
}

/*
 *
 * Internal functions.
 *
 */

/**
 * Check if the identity key is in the KMU.
 */
static bool is_key_provisioned() {
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status = psa_get_key_attributes(PSA_KEY_ID, &attributes);

    psa_reset_key_attributes(&attributes);

    if (status == PSA_SUCCESS) {
        return true;
    } else if (status == PSA_ERROR_DOES_NOT_EXIST || status == PSA_ERROR_INVALID_HANDLE) {
        return false;
    } else {
        LOG_WRN("Error when checking key provisioning: %d", status);
        return false;
    }
}

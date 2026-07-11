/*!
 * @file attest.h
 * @author Gabriel Gutiérrez Fuentes (me@gab.gf)
 * @brief Provides interfaces to interact with the attestation API.
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef SPV_ATTEST_ATTEST_H_
#define SPV_ATTEST_ATTEST_H_

#include <stdlib.h>

/**
 * @brief Initializes the attestation API.
 *
 * @return int 0 if successful.
 */
[[nodiscard]]
int attest_init();

/**
 * @brief Imports a private key into the KMU. It is not necessary to call
 * `attest_init` before this function.
 *
 * @param[in] key Pointer to raw private values.
 * @param[in] keylength Size in bytes of the key.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1)]]
int attest_key_import(const char *key, size_t keylength);


/**
 * @brief Signs a message with the device's identity key.
 *
 * @param[out] signature Fixed-size signature. See CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE.
 * @param[in] message Message to sign.
 * @param[in] message_size Message length in bytes.
 *
 * @return int 0 if successful.
 */
[[nodiscard, gnu::nonnull(1, 2)]]
int attest_sign(
	char signature[CONFIG_SPV_ATTESTATION_SIGNATURE_SIZE],
	const char *message, size_t message_size
);

#endif

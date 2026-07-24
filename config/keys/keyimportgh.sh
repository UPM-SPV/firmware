#!/bin/sh
set -e

KEY_PATH="config/local"
SIGNKEY_PATH="${KEY_PATH}/signing_key.pem"
ENCKEY_PATH="${KEY_PATH}/encryption_key.pem"

if [ -e "${SIGNKEY_PATH}" ] || [ -e "${ENCKEY_PATH}" ]; then
	echo "Found existing keys."
	echo "Refusing to continue."
	exit 1
fi

if [ -z "${SIGNING_KEY}" ] || [ -z "${ENCRYPTION_KEY}" ]; then
	echo "Secret variables are not configured."
	echo "Go to Repository > Settings > Secrets and Variables > Actions"
	echo 'and make sure `SIGNING_KEY` and `ENCRYPTION_KEY` are set to valid keys.'
	exit 1
fi

if echo "${ENCRYPTION_KEY}" | grep "BEGIN PRIVATE KEY"; then
	echo "===== ERROR ====="
	echo "Provided encryption key contains private part."
	echo 'Make sure to set the `ENCRYPTION_KEY` Actions secret to'
	echo 'the contents of `config/local/encryption_key_pub.pem`.'
	echo ""
	echo "Refusing to continue."
	exit 1
fi

echo "Saving keys to '${KEY_PATH}'"
mkdir -p "${KEY_PATH}"
echo "${SIGNING_KEY}" > "${SIGNKEY_PATH}"
echo "${ENCRYPTION_KEY}" > "${ENCKEY_PATH}"

echo "Done."

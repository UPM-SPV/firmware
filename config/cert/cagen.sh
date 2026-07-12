#!/bin/sh
set -e

CA_PATH="config/local/ca"
CACRT_PATH="${CA_PATH}/cacert.pem"

if [ -e "${CACRT_PATH}" ]; then
	echo "Found existing CA, refusing to continue."
	exit 1
fi

mkdir -p "${CA_PATH}"

echo "Generating CA in ${CA_PATH}"
openssl genpkey -algorithm ED25519 -out config/local/ca/private_key.pem
openssl req -new -x509 -config config/cert/ca.conf -key config/local/ca/private_key.pem -out config/local/ca/cacert.pem -days 36500

echo "Done."

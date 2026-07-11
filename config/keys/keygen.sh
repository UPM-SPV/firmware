#!/bin/sh
set -e


if [ ! -x "${ZEPHYR_BASE}" ]; then
	echo 'Required environment variable `ZEPHYR_BASE` is not set.'
	exit 1
fi

NCS_PATH=$(dirname "${ZEPHYR_BASE}")
# `config/local` directory is deliberately git-ignored
KEY_PATH="config/local"
SIGNKEY_PATH="${KEY_PATH}/signing_key.pem"
ENCKEY_PATH="${KEY_PATH}/encryption_key.pem"

if [ -e "${SIGNKEY_PATH}" ] && [ -e "${ENCKEY_PATH}" ]; then
	BOLD="\033[1m"
	BLINK="\033[5m"
	RED="\033[1;31m"
	RESET="\033[0m"

	prompt="YES, DELETE MY KEYS"

	echo -e "${BOLD}!!! ${BLINK}${RED}WARNING ${RESET}${BOLD}!!!${RESET}"
	echo ""
	echo "PRIVATE KEYS HAVE ALREADY BEEN GENERATED"
	echo "RUNNING THIS SCRIPT WILL DELETE THEM AND GENERATE NEW ONES"
	echo ""
	echo -en "${BOLD}ARE YOU SURE?${RESET} (type '${BOLD}${prompt}${RESET}' in uppercase) "
	read -r ans

	if [ "${ans}" != "${prompt}" ]; then
		echo "Aborting."
		exit 1
	fi

	rm "${SIGNKEY_PATH}" "${ENCKEY_PATH}"
fi

mkdir -p "${KEY_PATH}"

if [ ! -e "${SIGNKEY_PATH}" ]; then
	echo "Generating new signing key in '${SIGNKEY_PATH}'."
	python3 "${NCS_PATH}/bootloader/mcuboot/scripts/imgtool.py" keygen -t ed25519 -k "${SIGNKEY_PATH}"
fi

if [ ! -e "${ENCKEY_PATH}" ]; then
	echo "Generating new encryption key in '${ENCKEY_PATH}'."
	# Using x25519 is required for the encryption key.
	python3 "${NCS_PATH}/bootloader/mcuboot/scripts/imgtool.py" keygen -t x25519 -k "${ENCKEY_PATH}"
fi

echo "Done."

#!/bin/env python3
import argparse
import datetime
import os
import random
import subprocess
import sys
import json
from tempfile import NamedTemporaryFile

from smartcard.System import readers
from smartcard.CardRequest import CardRequest
from smartcard.CardType import AnyCardType
from ntag import NTAG215

from cryptography.hazmat.primitives import serialization

import factory_data_pb2 as factory_data
import nfc_tag_pb2 as nfc_tag
from google.protobuf.internal.encoder import _VarintBytes
from intelhex import IntelHex

"""
Tool for quickly flashing devices.
	- Generates a device certificate and private key.
	- Generates a FactoryData partition.
	- Provisions MCUBoot keys and private key.
	- Flashes FactoryData.
	- Flashes firmware to device.
"""

LOCAL_PATH = "config/local"
ENCKEY_PATH = f"{LOCAL_PATH}/encryption_key.pem"
SIGNKEY_PATH = f"{LOCAL_PATH}/signing_key.pem"

CA_PATH = f"{LOCAL_PATH}/ca"
CAKEY_PATH = f"{CA_PATH}/private_key.pem"
CACRT_PATH = f"{CA_PATH}/cacert.pem"
CACNF_PATH = f"config/cert/ca.conf"
BUILD_DIR = "flasher_build/"
KEYFILEJSON_PATH = f"{BUILD_DIR}/keyfile.json"

FACTORYDATA_MAGIC_NUMBER = 0xFAC1DA1A
FACTORYDATA_TARGET_ADDRESS = 0xB5000

NFCTAG_MAGIC_NUMBER = 0x1A60DA1A

DEFAULT_BOARD_REVISION="v0.1.0"
MAX_SERIAL_NO=999_999_999_999
MAX_SERIAL_NO_LEN=len(str(MAX_SERIAL_NO))

WEST_OPTIMIZED_BUILD_PARAMS = [
	"--",
	"-DCONFIG_LOG_MAX_LEVEL=0",
    "-DCONFIG_CONSOLE=n",
    "-DCONFIG_RTT_CONSOLE=n",
    "-DCONFIG_USE_SEGGER_RTT=n",
    "-DCONFIG_LOG_BACKEND_RTT=n",
    "-DCONFIG_SIZE_OPTIMIZATIONS_AGGRESSIVE=y",
	"-DCONFIG_ASSERT=n",
	"-DCONFIG_SPV_INIT_PRINT_DETAILED_ERROR=n"
]

BOLD="\033[1m"
BLINK="\033[5m"
RED="\033[1;31m"
BLUE="\033[1;34m"
RESET="\033[0m"

def logi(msg):
	print(f"{BOLD}{BLUE}{msg}{RESET}")
def loge(msg):
	print(f"{BOLD}{RED}{msg}{RESET}")

def tmpfile(name, ctx, suffix=""):
	temporary_file = NamedTemporaryFile(suffix=suffix)
	ctx[name] = temporary_file
	ctx[f"{name}_PATH"] = temporary_file.name
	return (temporary_file, temporary_file.name)

def generate_serial_number():
	date = datetime.datetime.now()
	random_no = random.randint(0, MAX_SERIAL_NO)
	serial = f"SPV-{date.year}-{date.month:02d}-{date.day:02d} {random_no:0{MAX_SERIAL_NO_LEN}d}"
	return serial


def select_nfc_reader():
	try:
		available_readers = readers()
		if len(available_readers) == 0:
			loge("No NFC readers connected")
			sys.exit(1)

		if len(available_readers) == 1:
			return available_readers[0]

		logi("Multiple NFC readers are available, choose one")
		for i in range(len(available_readers)):
			logi(f"	{i}.	{available_readers[i]}")

		index = input(f"Number between 0 and {len(available_readers) - 1} [{available_readers[0]}]: ")
		if len(index) == 0:
			return available_readers[0]

		index = int(index)
		if index < 0 or index >= len(available_readers):
			loge("No such reader")
			sys.exit(1)

		return available_readers[index]

	except Exception as e:
		loge("Error NFC obtaining reader")
		print(e)
		sys.exit(1)


def build_fw(ctx, optimize=False):
	try:
		if os.path.exists(KEYFILEJSON_PATH):
			os.remove(KEYFILEJSON_PATH)

		extra_params = []
		if optimize:
			extra_params = WEST_OPTIMIZED_BUILD_PARAMS

		env = os.environ
		env["BOARD_ROOT"] = os.getcwd()
		subprocess.run(
			[
				"west", "build",
				"--board", "nrfpcb/nrf54l15/cpuapp",
				"--build-dir", BUILD_DIR
			] + extra_params,
			env=env,
			check=True
		)
	except subprocess.CalledProcessError as e:
		loge("Error building firmware")
		sys.exit(1)

	return ctx


def wipe_device(ctx):
	try:
		subprocess.run(
			[
				"nrfjprog", "--recover"
			],
			check=True
		)

	except subprocess.CalledProcessError as e:
		loge("Error wiping device")
		sys.exit(1)

	return ctx


def read_mac_address(ctx):
	try:
		process = subprocess.Popen(
			[
				"nrfutil", "device", "x-read",
				"--address", "0x00FFC3A4",
				"--width", "8",
				"--bytes", "6",
				"--json"
			],
			stdout=subprocess.PIPE,
			text=True
		)

		for line in process.stdout:
			if not line:
				break

			json_data = json.loads(line)
			if json_data['type'] == "task_end":
				ctx["MAC_ADDRESS"] = json_data['data']['data']['memoryData'][0]['values']

		if "MAC_ADDRESS" not in ctx:
			loge("Error obtaining MAC address from device")
			sys.exit(1)

		# Specify static address
		ctx["MAC_ADDRESS"][5] = ctx["MAC_ADDRESS"][5] | 0b11000000

	except subprocess.CalledProcessError as e:
		loge("Error obtaining MAC address")
		sys.exit(1)

	return ctx


def generate_privkey(ctx):
	try:
		privkey_file, privkey_path = tmpfile("PRIVKEY", ctx)

		subprocess.run(
			[
				"openssl", "ecparam",
				"-genkey",
				"-name", "secp256r1",
				"-out", privkey_path,
				"-outform", "PEM"
			],
			check=True
		)

	except subprocess.CalledProcessError as e:
		loge("Error generating private key")
		sys.exit(1)

	return ctx


def generate_cert(ctx, serialNumber):
	try:
		privkey_path = ctx["PRIVKEY_PATH"]
		mac_bytes_le = ctx["MAC_ADDRESS"]
		csr_file, csr_path = tmpfile("CSR", ctx)
		crt_file, crt_path = tmpfile("CRT", ctx)

		mac_bytes_be = reversed(mac_bytes_le)
		mac_address = [f"{byte:02X}" for byte in mac_bytes_be]
		mac_address = ":".join(mac_address)


		subprocess.run(
			[
				"openssl", "req",
				"-new",
				"-key", privkey_path,
				"-out", csr_path,
				"-subj", f"/serialNumber={serialNumber}/",
				"-addext", f"1.3.6.1.1.1.1.22=ASN1:IA5STRING:{mac_address}",
				"-nodes"
			],
			check=True
		)

		subprocess.run(
			[
				"openssl", "x509",
				"-req",
				"-in", csr_path,
				"-CA", CACRT_PATH,
				"-CAkey", CAKEY_PATH,
				"-out", crt_path,
				"-days", "36500",
				"-extfile", CACNF_PATH,
				"-extensions", "v3_leaf",
				"-copy_extensions", "copy",
				"-outform", "DER",
				"-sha256",
			]
		)


	except subprocess.CalledProcessError as e:
		loge("Error generating private key")
		sys.exit(1)

	return ctx


def generate_factorydata(ctx, board_revision, serial_number):
	try:
		factorydata_file, factorydata_path = tmpfile("FACTORYDATA", ctx, suffix=".hex")
		crt_path = ctx["CRT_PATH"]
		privkey_path = ctx["PRIVKEY_PATH"]
		with open(crt_path, 'rb') as f: crt = f.read()
		with open(privkey_path, 'r') as f:

			pem = f.read()

			key_block = pem.split("-----BEGIN EC PRIVATE KEY-----")[1]
			pem = "-----BEGIN EC PRIVATE KEY-----" + key_block
			private_key = serialization.load_pem_private_key(
				bytes(pem, encoding="utf-8"),
				password=None
			)
			private_value = private_key.private_numbers().private_value
			private_bytes = private_value.to_bytes(32, byteorder='big')


		data = factory_data.FactoryData()
		data.magic = FACTORYDATA_MAGIC_NUMBER
		data.schema_version = 1
		data.manufacturing_date = int(datetime.datetime.now().timestamp())

		data.board_revision = board_revision
		data.serial_number = serial_number

		data.device_cert = crt
		data.private_key = private_bytes

		serialized = data.SerializeToString()
		blob = _VarintBytes(len(serialized)) + serialized
		hexdata = IntelHex()
		hexdata.frombytes(blob, offset=FACTORYDATA_TARGET_ADDRESS)
		hexdata.tofile(factorydata_path, format="hex")

	except subprocess.CalledProcessError as e:
		loge("Error generating factorydata")
		sys.exit(1)

	return ctx


def write_nfc_tag(serial_number, nfc_reader, ctx):
	try:
		mac_address = ctx["MAC_ADDRESS"]

		data = nfc_tag.NfcTag()
		data.magic = NFCTAG_MAGIC_NUMBER
		data.schema_version = 1

		data.serial_number = serial_number
		data.mac_address = bytes(mac_address)

		serialized = data.SerializeToString()
		data_size = len(serialized).to_bytes(2, 'little')
		serialized = data_size + serialized

		logi("Waiting for smartcard...")
		request = CardRequest(timeout=None, readers=[nfc_reader], cardType=AnyCardType())
		response = request.waitforcard()
		response.connection.connect()
		tag = NTAG215(response.connection)

		logi("Wiping tag...")
		tag.wipe()
		logi("Writing data...")
		tag.write_bytes(serialized, 4)

		logi("Verifying data...")
		verified_data = tag.read_bytes(len(serialized), 4)
		if serialized != verified_data:
			loge("Error verifying tag data")
			sys.exit(1)


	except Exception as e:
		loge("Error writing tag data")
		print(e)
		sys.exit(1)

	return ctx

def flash_factorydata(ctx):
	try:
		factorydata_path = ctx["FACTORYDATA_PATH"]

		subprocess.run(
			[
				"nrfjprog",
				"--program", factorydata_path,
				"--sectorerase",
				"--verify"
			],
			check=True
		)

	except subprocess.CalledProcessError as e:
		loge("Error flashing factorydata")
		sys.exit(1)

	return ctx


def provision_keydata(ctx):
	try:
		subprocess.run(
			[
				"nrfutil", "device", "x-provision-keys",
				"--key-file", KEYFILEJSON_PATH
			],
			check=True
		)

	except subprocess.CalledProcessError as e:
		loge("Error provisioning keydata")
		sys.exit(1)

	return ctx


def flash_fw(ctx, optimize=False):
	try:
		env = os.environ
		env["BOARD_ROOT"] = os.getcwd()
		subprocess.run(
			[
				"west", "flash",
				"--board", "nrfpcb/nrf54l15/cpuapp",
				"--build-dir", BUILD_DIR
			],
			env=env,
			check=True
		)

	except subprocess.CalledProcessError as e:
		loge("Error flashing firmware")
		sys.exit(1)

	return ctx


def main():
	logi("Welcome to the SPV flasher!")

	if not os.path.exists(CAKEY_PATH):
		loge("CA has not been generated, run `config/cert/cagen.sh` first")
		sys.exit(1)

	if not os.path.exists(ENCKEY_PATH) or not os.path.exists(SIGNKEY_PATH):
		loge("Firmware keys not generated, run `config/keys/keygen.sh` first")
		sys.exit(1)

	if "ZEPHYR_BASE" not in os.environ:
		loge("`ZEPHYR_BASE` environment variable not defined")
		sys.exit(1)

	parser = argparse.ArgumentParser(
		prog="flash_device",
		description="Production device flasher",
		add_help=True
	)
	parser.add_argument(
		"-r", "--release",
		help="Use to enable release optimizations",
		action="store_true",
		default=False
	)
	parser.add_argument(
		"-t", "--nfc-tag",
		help="Use to enable writing an NFC tag",
		action="store_true",
		default=False
	)
	args = parser.parse_args()

	logi("Please enter the following data:")
	serial_number = input("Serial Number [empty for auto]: ")
	board_revision = input(f"Board revision [{DEFAULT_BOARD_REVISION}]:")
	optimized_build = args.release

	if len(serial_number) == 0:
		serial_number = generate_serial_number()

	if len(board_revision) == 0:
		board_revision = DEFAULT_BOARD_REVISION

	if args.nfc_tag:
		nfc_reader = select_nfc_reader()

	logi("Using:")
	logi(f"	Serial no.:	{serial_number}")
	logi(f"	Board rev.:	{board_revision}")
	if args.nfc_tag:
		logi(f"	NFC reader:	{nfc_reader}")

	logi("Building firmware")
	ctx = build_fw({}, optimized_build)

	logi("Wiping device")
	ctx = wipe_device(ctx)

	logi("Reading MAC address")
	ctx = read_mac_address(ctx)

	logi("Generating private key")
	ctx = generate_privkey(ctx)

	logi("Signing certificate")
	ctx = generate_cert(ctx, serial_number)

	logi("Generating factorydata")
	ctx = generate_factorydata(ctx, board_revision, serial_number)

	if args.nfc_tag:
		logi("Writing NFC tag")
		ctx = write_nfc_tag(serial_number, nfc_reader, ctx)

	logi("Flashing factorydata")
	ctx = flash_factorydata(ctx)

	logi("Provisioning keydata")
	ctx = provision_keydata(ctx)

	logi("Flashing firmware")
	ctx = flash_fw(ctx, optimized_build)

	logi("Done!")
	logi("Final output:")
	logi(f"	Serial number:	{serial_number}")
	logi(f"	Board revision:	{board_revision}")
	logi(f"	MAC address:	{":".join([f"{i:02X}" for i in reversed(ctx["MAC_ADDRESS"])])}")


if __name__ == '__main__':
	main()

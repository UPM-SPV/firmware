#!/bin/env python3

from typing import List
import math

from smartcard.CardConnectionDecorator import CardConnectionDecorator




class NTAG215:
	NTAG_USER_DATA_START_PAGE = 0x04
	NTAG_USER_DATA_END_PAGE = 0x80
	NTAG_PAGE_SIZE_BYTES = 4
	_connection: CardConnectionDecorator

	def __init__(self, connection: CardConnectionDecorator):
		self._connection = connection


	def wipe(self) -> None:
		pages = self.NTAG_USER_DATA_END_PAGE - self.NTAG_USER_DATA_START_PAGE
		data = [0] * self.NTAG_PAGE_SIZE_BYTES * pages
		self.write_bytes(bytes(data), self.NTAG_USER_DATA_START_PAGE)

	def write_bytes(self, data_bytes: bytes, page_start: int) -> None:

		if page_start < self.NTAG_USER_DATA_START_PAGE or page_start > self.NTAG_USER_DATA_END_PAGE:
			raise ValueError("Attempted to write to a control page")

		# The NTAG215 only supports writing pages, not individual bytes.
		# Pad the data to the page size.
		page_size = self.NTAG_PAGE_SIZE_BYTES
		padding_len = (page_size - (len(data_bytes) % page_size)) % page_size
		data = list(data_bytes) + [0] * padding_len

		# Split data into page-sized chunks
		data_page_len = len(data) // page_size
		data_chunks = [data[i:i+page_size] for i in range(0, len(data), page_size)]

		final_page = page_start + data_page_len - 1
		if final_page > self.NTAG_USER_DATA_END_PAGE:
			raise ValueError("Data does not fit in user region")

		for page, chunk in zip(range(page_start, page_start + data_page_len), data_chunks):
			apdu = NTAG215._write_apdu_skel(page)
			apdu += chunk
			self._connection.transmit(apdu)


	def read_bytes(self, length: int, page_start: int) -> bytes:
		# NTAG215 only supports reading pages.
		# Some readers suppport automatically splitting bigger reads
		# into multiple commands, others support extended reads (16+ pages).
		#
		# Assume only page-sized reads.
		data = []
		data_page_len = math.ceil(length / self.NTAG_PAGE_SIZE_BYTES)

		for page in range(page_start, page_start + data_page_len):
			read_size = min(self.NTAG_PAGE_SIZE_BYTES, length - len(data))
			apdu = NTAG215._read_apdu_skel(page, read_size)
			response = self._transmit(apdu)
			data += response

		return bytes(data)



	def _transmit(self, apdu: List[int]) -> List[int]:
		response, sw1, sw2 = self._connection.transmit(apdu)
		if sw1 != 0x90 or sw2 != 0x00:
			raise IOError(f"Error transmitting to smartcard: {sw1:02X}, {sw2:02X}")

		return response

	@staticmethod
	def _write_apdu_skel(page: int) -> List[int]:
		return [0xFF, 0xD6, 0x00, page, NTAG215.NTAG_PAGE_SIZE_BYTES]

	@staticmethod
	def _read_apdu_skel(page: int, length: int) -> List[int]:
		return [0xFF, 0xB0, 0x00, page, length]

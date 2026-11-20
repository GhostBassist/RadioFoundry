// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_link.h"

#include <string.h>

static const char alphabet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

bool m17_address_encode(const char *callsign, uint8_t address[M17_ADDRESS_BYTES])
{
	if (!callsign)
		return false;
	uint64_t value = 0;
	uint64_t place = 1;
	for (unsigned i = 0; callsign[i]; i++) {
		if (i == M17_CALLSIGN_LEN)
			return false;
		char c = callsign[i];
		if (c >= 'a' && c <= 'z')
			c += 'A' - 'a';
		const char *symbol = strchr(alphabet, c);
		if (!symbol)
			return false;
		value += (uint64_t)(symbol - alphabet) * place;
		place *= 40u;
	}
	if (!value)
		return false;
	for (unsigned i = M17_ADDRESS_BYTES; i > 0; i--) {
		address[i - 1] = (uint8_t)value;
		value >>= 8;
	}
	return true;
}

bool m17_address_decode(const uint8_t address[M17_ADDRESS_BYTES], char callsign[M17_CALLSIGN_LEN + 1])
{
	uint64_t value = 0;
	for (unsigned i = 0; i < M17_ADDRESS_BYTES; i++)
		value = (value << 8) | address[i];
	if (!value || value >= UINT64_C(0xEE6B28000000))
		return false;
	unsigned i = 0;
	do {
		callsign[i++] = alphabet[value % 40u];
		value /= 40u;
	} while (value);
	callsign[i] = '\0';
	return true;
}

bool m17_packet_link(const char *source, const char *destination, uint8_t can,
	uint8_t link[M17_LINK_DATA_BYTES])
{
	uint8_t data[M17_LINK_DATA_BYTES] = { 0 };
	if (can > M17_CAN_MAX || !m17_address_encode(source, data + M17_ADDRESS_BYTES))
		return false;
	if (destination) {
		if (!m17_address_encode(destination, data))
			return false;
	} else {
		memset(data, 0xFF, M17_ADDRESS_BYTES);
	}
	data[12] = can >> 1;
	data[13] = (uint8_t)(can << 7);
	memcpy(link, data, sizeof(data));
	return true;
}

uint8_t m17_link_can(const uint8_t link[M17_LINK_DATA_BYTES])
{
	return (uint8_t)(((link[12] << 1) | (link[13] >> 7)) & M17_CAN_MAX);
}

bool m17_voice_link(const char *source, const char *destination, uint8_t can,
	uint8_t link[M17_LINK_DATA_BYTES])
{
	if (!m17_packet_link(source, destination, can, link))
		return false;
	link[13] |= 0x05u;	// stream mode, Codec 2 3200 voice only
	return true;
}

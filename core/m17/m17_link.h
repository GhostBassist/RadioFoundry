// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_LINK_H
#define M17_LINK_H

#include <stdbool.h>

#include "m17_frame.h"

#define M17_CALLSIGN_LEN 9
#define M17_ADDRESS_BYTES 6
#define M17_CAN_MAX 15

// Standard base-40 callsigns. Lowercase input is normalized to uppercase.
bool m17_address_encode(const char *callsign, uint8_t address[M17_ADDRESS_BYTES]);
bool m17_address_decode(const uint8_t address[M17_ADDRESS_BYTES], char callsign[M17_CALLSIGN_LEN + 1]);

// Build the link data for a clear packet. A NULL destination selects broadcast.
bool m17_packet_link(const char *source, const char *destination, uint8_t can,
	uint8_t link[M17_LINK_DATA_BYTES]);
bool m17_voice_link(const char *source, const char *destination, uint8_t can,
	uint8_t link[M17_LINK_DATA_BYTES]);
uint8_t m17_link_can(const uint8_t link[M17_LINK_DATA_BYTES]);

#endif

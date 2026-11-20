// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_PACKET_H
#define M17_PACKET_H

#include "m17_frame.h"

#define M17_PACKET_MAX_BYTES 823

typedef struct {
	const uint8_t *data;
	size_t len, offset;
	uint16_t crc;
} m17_packet_tx_t;

typedef struct {
	m17_packet_tx_t packet;
	uint8_t link[M17_LINK_DATA_BYTES];
	uint8_t stage;
} m17_packet_burst_t;

typedef struct {
	uint8_t data[M17_PACKET_MAX_BYTES + 2];
	size_t len;
	uint8_t active;
} m17_packet_rx_t;

// The payload includes the application protocol identifier and must outlive the transfer.
int m17_packet_tx_begin(m17_packet_tx_t *tx, const uint8_t *data, size_t len);
int m17_packet_tx_next(m17_packet_tx_t *tx, m17_packet_chunk_t *chunk);

// Produce consecutive 40 ms blocks from preamble through end-of-transmission.
int m17_packet_burst_begin(m17_packet_burst_t *burst, const uint8_t link[M17_LINK_DATA_BYTES],
	const uint8_t *data, size_t len);
int m17_packet_burst_next(m17_packet_burst_t *burst, uint8_t frame[M17_FRAME_BYTES]);

// Start after accepting a packet-mode link setup frame. Returns 0 while collecting,
// 1 on completion with rx->len payload bytes, or -1 on a sequence or CRC error.
void m17_packet_rx_begin(m17_packet_rx_t *rx);
int m17_packet_rx_push(m17_packet_rx_t *rx, const m17_packet_chunk_t *chunk);

#endif

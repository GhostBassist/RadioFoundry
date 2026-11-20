// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_RX_H
#define M17_RX_H

#include "air/air_framer.h"
#include "m17_packet.h"

#define M17_FRAME_SYMBOLS (M17_FRAME_BYTES * 4)

typedef struct {
	air_framer_t framer;
	m17_packet_rx_t packet;
	uint8_t hard[M17_FRAME_BYTES];
	uint8_t link[M17_LINK_DATA_BYTES];
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx);
	void *ctx;
} m17_rx_t;

extern const air_mode_t m17_packet_air_mode;

// The packet and link buffers are valid during the callback.
void m17_rx_init(m17_rx_t *rx,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx), void *ctx);
void m17_rx_reset(m17_rx_t *rx);
void m17_rx_feed(m17_rx_t *rx, const air_chunk_t *chunk);

#endif

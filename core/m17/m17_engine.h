// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_ENGINE_H
#define M17_ENGINE_H

#include "air/air_session.h"
#include "m17_rx.h"

typedef struct {
	air_session_t session;
	m17_rx_t rx;
	m17_packet_burst_t burst;
	uint8_t rx_frame[M17_FRAME_BYTES];
	uint8_t tx_frame[M17_FRAME_BYTES];
	uint16_t tx_pos;
} m17_engine_t;

// The caller owns tuning, channel access, permission to key, and RF power.
bool m17_engine_open(m17_engine_t *engine, const hal_air_t *air,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx), void *ctx);
void m17_engine_retuned(m17_engine_t *engine);

// Prepare a send while idle. The payload stays valid until the session signals TX done.
// On success the owner can proceed with air_session_tx_prime and air_session_tx_go.
bool m17_engine_prepare(m17_engine_t *engine, const uint8_t link[M17_LINK_DATA_BYTES],
	const uint8_t *data, size_t len);

#endif

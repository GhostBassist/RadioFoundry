// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_engine.h"

static void tx_fill(void *ctx)
{
	m17_engine_t *engine = ctx;
	for (;;) {
		if (engine->tx_pos == M17_FRAME_SYMBOLS) {
			if (m17_packet_burst_next(&engine->burst, engine->tx_frame) <= 0) {
				air_session_tx_finish(&engine->session);
				return;
			}
			engine->tx_pos = 0;
		}
		uint16_t space = air_session_tx_space(&engine->session);
		if (!space)
			return;
		uint16_t count = M17_FRAME_SYMBOLS - engine->tx_pos;
		if (count > space)
			count = space;
		if (!air_session_tx(&engine->session, engine->tx_frame, engine->tx_pos, count))
			return;
		engine->tx_pos += count;
	}
}

static void tx_done(void *ctx)
{
	m17_engine_t *engine = ctx;
	m17_rx_reset(&engine->rx);
}

static const air_session_ops_t ops = { .tx_fill = tx_fill, .tx_done = tx_done };

bool m17_engine_open(m17_engine_t *engine, const hal_air_t *air,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx), void *ctx)
{
	engine->burst = (m17_packet_burst_t){ 0 };
	engine->tx_pos = M17_FRAME_SYMBOLS;
	m17_rx_init(&engine->rx, received, ctx);
	return air_session_open(&engine->session, air, &m17_packet_air_mode, &engine->rx, engine->rx_frame,
		NULL, M17_FRAME_SYMBOLS, &ops, engine);
}

void m17_engine_retuned(m17_engine_t *engine)
{
	m17_rx_reset(&engine->rx);
	air_session_retuned(&engine->session);
}

bool m17_engine_prepare(m17_engine_t *engine, const uint8_t link[M17_LINK_DATA_BYTES],
	const uint8_t *data, size_t len)
{
	const air_session_t *session = &engine->session;
	if (!air_session_is_open(session) || air_session_tx_state(session) != AIR_TX_IDLE)
		return false;
	// Permit receive on this provider, but require a conforming waveform for transmit.
	if ((session->fit & (AIR_FIT_TX | AIR_FIT_SHAPE_APPROX | AIR_FIT_DEV_APPROX)) != AIR_FIT_TX)
		return false;
	engine->tx_pos = M17_FRAME_SYMBOLS;
	return m17_packet_burst_begin(&engine->burst, link, data, len) == 0;
}

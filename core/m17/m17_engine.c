// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_engine.h"

#include <string.h>

enum { TX_PACKET, TX_VOICE_PREAMBLE, TX_VOICE_LINK, TX_VOICE_SPEECH, TX_VOICE_END, TX_VOICE_DONE };
enum { FRAME_DONE, FRAME_READY, FRAME_WAIT };

static int next_frame(m17_engine_t *e)
{
	switch (e->voice_stage) {
	case TX_PACKET:
		return m17_packet_burst_next(&e->burst, e->tx_frame) > 0 ? FRAME_READY : FRAME_DONE;
	case TX_VOICE_PREAMBLE:
		m17_preamble(e->tx_frame);
		e->voice_stage = TX_VOICE_LINK;
		return FRAME_READY;
	case TX_VOICE_LINK:
		memcpy(e->tx_frame, e->voice_link, sizeof(e->tx_frame));
		e->voice_stage = TX_VOICE_SPEECH;
		return FRAME_READY;
	case TX_VOICE_SPEECH: {
		uint8_t speech[M17_STREAM_BYTES];
		bool final = false;
		if (!air_session_tx_space(&e->session))
			return FRAME_WAIT;
		if (!e->tx_speech(speech, &final, e->speech_ctx)) {
			const hal_air_t *air = e->session.air;
			uint32_t lead = (uint32_t)air->tx_lead_ms * m17_air_mode.profile.sym_rate / 1000u;
			if (air_session_tx_state(&e->session) == AIR_TX_PRIMING ||
			    air_session_tx_queued(&e->session) >= lead + air->tx_chunk_syms)
				return FRAME_WAIT;
			memcpy(speech, e->silence, sizeof(speech));
			final = false;
		}
		if (!m17_voice_tx_frame(&e->voice_tx, speech, final, e->tx_frame))
			return FRAME_DONE;
		if (final)
			e->voice_stage = TX_VOICE_END;
		return FRAME_READY;
	}
	case TX_VOICE_END:
		m17_end_marker(e->tx_frame);
		e->voice_stage = TX_VOICE_DONE;
		return FRAME_READY;
	default:
		return FRAME_DONE;
	}
}

static void tx_fill(void *ctx)
{
	m17_engine_t *engine = ctx;
	for (;;) {
		if (engine->tx_pos == M17_FRAME_SYMBOLS) {
			int next = next_frame(engine);
			if (next == FRAME_WAIT)
				return;
			if (next == FRAME_DONE) {
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

bool m17_engine_open(m17_engine_t *engine, const hal_air_t *air, uint8_t rx_can,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx),
	const m17_voice_rx_ops_t *voice, void *ctx)
{
	engine->burst = (m17_packet_burst_t){ 0 };
	engine->tx_pos = M17_FRAME_SYMBOLS;
	engine->voice_stage = TX_PACKET;
	m17_rx_init(&engine->rx, rx_can, received, voice, ctx);
	return air_session_open(&engine->session, air, &m17_air_mode, &engine->rx, engine->rx_frame,
		NULL, M17_FRAME_SYMBOLS, &ops, engine);
}

void m17_engine_retuned(m17_engine_t *engine)
{
	m17_rx_reset(&engine->rx);
	air_session_retuned(&engine->session);
}

static bool can_prepare(const m17_engine_t *engine)
{
	const air_session_t *session = &engine->session;
	if (!air_session_is_open(session) || air_session_tx_state(session) != AIR_TX_IDLE)
		return false;
	// Permit receive on this provider, but require a conforming waveform for transmit.
	if ((session->fit & (AIR_FIT_TX | AIR_FIT_SHAPE_APPROX | AIR_FIT_DEV_APPROX)) != AIR_FIT_TX)
		return false;
	return true;
}

bool m17_engine_prepare(m17_engine_t *engine, const char *source, const char *destination, uint8_t can,
	const uint8_t *data, size_t len)
{
	if (!can_prepare(engine))
		return false;
	engine->voice_stage = TX_PACKET;
	engine->tx_pos = M17_FRAME_SYMBOLS;
	engine->burst = (m17_packet_burst_t){ 0 };
	uint8_t link[M17_LINK_DATA_BYTES];
	return m17_packet_link(source, destination, can, link) &&
	       m17_packet_burst_begin(&engine->burst, link, data, len) == 0;
}

bool m17_engine_prepare_voice(m17_engine_t *engine, const char *source, const char *destination, uint8_t can,
	const uint8_t silence[M17_STREAM_BYTES], m17_tx_speech_fn speech, void *ctx)
{
	if (!can_prepare(engine))
		return false;
	engine->voice_stage = TX_VOICE_DONE;
	engine->tx_pos = M17_FRAME_SYMBOLS;
	if (!silence || !speech || !m17_voice_tx_start(&engine->voice_tx, source, destination, can, engine->voice_link))
		return false;
	memcpy(engine->silence, silence, sizeof(engine->silence));
	engine->tx_speech = speech;
	engine->speech_ctx = ctx;
	engine->voice_stage = TX_VOICE_PREAMBLE;
	return true;
}

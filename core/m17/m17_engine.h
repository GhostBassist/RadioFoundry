// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_ENGINE_H
#define M17_ENGINE_H

#include "air/air_session.h"
#include "m17_rx.h"

// Called from the owner while priming and from the provider during transmit.
// Return false when speech is temporarily unavailable, without blocking.
typedef bool (*m17_tx_speech_fn)(uint8_t speech[M17_STREAM_BYTES], bool *final, void *ctx);

typedef struct {
	air_session_t session;
	m17_rx_t rx;
	m17_packet_burst_t burst;
	m17_voice_tx_t voice_tx;
	m17_tx_speech_fn tx_speech;
	void *speech_ctx;
	uint8_t voice_link[M17_FRAME_BYTES];
	uint8_t silence[M17_STREAM_BYTES];
	uint8_t rx_frame[M17_FRAME_BYTES];
	uint8_t tx_frame[M17_FRAME_BYTES];
	uint16_t tx_pos;
	uint8_t voice_stage;
} m17_engine_t;

// The caller owns tuning, channel access, permission to key, and RF power.
bool m17_engine_open(m17_engine_t *engine, const hal_air_t *air, uint8_t rx_can,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx),
	const m17_voice_rx_ops_t *voice, void *ctx);
void m17_engine_retuned(m17_engine_t *engine);

// Prepare a send while idle. The payload stays valid until the session signals TX done.
// On success the owner can proceed with air_session_tx_prime and air_session_tx_go.
bool m17_engine_prepare(m17_engine_t *engine, const char *source, const char *destination, uint8_t can,
	const uint8_t *data, size_t len);
// Prepare a voice transmission. Silence holds two valid Codec 2 3200 blocks for late audio.
bool m17_engine_prepare_voice(m17_engine_t *engine, const char *source, const char *destination, uint8_t can,
	const uint8_t silence[M17_STREAM_BYTES], m17_tx_speech_fn speech, void *ctx);

#endif

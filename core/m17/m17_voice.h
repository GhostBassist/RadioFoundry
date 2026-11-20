// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_VOICE_H
#define M17_VOICE_H

#include "m17_link.h"

#define M17_CODEC2_3200_BYTES 8
#define M17_CODEC2_3200_SAMPLES 160

typedef struct {
	uint8_t link[M17_LINK_DATA_BYTES + 2];
	uint16_t number;
	uint8_t lich_index;
	bool active;
} m17_voice_tx_t;

typedef struct {
	void (*start)(const uint8_t *link, bool late, void *ctx);
	void (*speech)(const uint8_t *data, uint16_t number, void *ctx);
	void (*end)(void *ctx);
} m17_voice_rx_ops_t;

typedef struct {
	m17_voice_rx_ops_t ops;
	void *ctx;
	uint8_t fragments[M17_LINK_DATA_BYTES + 2];
	uint16_t next_number;
	uint8_t next_fragment, can;
	bool active;
} m17_voice_rx_t;

void m17_voice_rx_init(m17_voice_rx_t *rx, uint8_t can, const m17_voice_rx_ops_t *ops, void *ctx);
void m17_voice_rx_reset(m17_voice_rx_t *rx);
void m17_voice_rx_link(m17_voice_rx_t *rx, const uint8_t link[M17_LINK_DATA_BYTES]);
void m17_voice_rx_frame(m17_voice_rx_t *rx, const m17_stream_frame_t *frame);

// Produce the initial link frame. The caller sends the preamble before it.
bool m17_voice_tx_start(m17_voice_tx_t *tx, const char *source, const char *destination, uint8_t can,
	uint8_t frame[M17_FRAME_BYTES]);
// Pack two successive 20 ms Codec 2 3200 blocks into one 40 ms stream frame.
// The caller sends an end-of-transmission marker after the final frame.
bool m17_voice_tx_frame(m17_voice_tx_t *tx, const uint8_t speech[M17_STREAM_BYTES], bool final,
	uint8_t frame[M17_FRAME_BYTES]);

#endif

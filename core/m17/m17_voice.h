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

// Produce the initial link frame. The caller sends the preamble before it.
bool m17_voice_tx_start(m17_voice_tx_t *tx, const char *source, const char *destination, uint8_t can,
	uint8_t frame[M17_FRAME_BYTES]);
// Pack two successive 20 ms Codec 2 3200 blocks into one 40 ms stream frame.
// The caller sends an end-of-transmission marker after the final frame.
bool m17_voice_tx_frame(m17_voice_tx_t *tx, const uint8_t speech[M17_STREAM_BYTES], bool final,
	uint8_t frame[M17_FRAME_BYTES]);

#endif

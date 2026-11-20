// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_CODEC2_H
#define M17_CODEC2_H

#include "m17_voice.h"

#define M17_VOICE_PCM_SAMPLES (2 * M17_CODEC2_3200_SAMPLES)

struct CODEC2;

typedef struct {
	struct CODEC2 *encoder;
	struct CODEC2 *decoder;
} m17_codec2_t;

// Start each direction with a fresh predictor state. Initialize the struct to zero before first use.
bool m17_codec2_start_tx(m17_codec2_t *codec);
bool m17_codec2_start_rx(m17_codec2_t *codec);
void m17_codec2_close(m17_codec2_t *codec);

// Each operation converts one M17 frame of 8 kHz mono PCM. Calls must be serialized with start and close.
bool m17_codec2_encode(m17_codec2_t *codec, int16_t pcm[M17_VOICE_PCM_SAMPLES],
	uint8_t speech[M17_STREAM_BYTES]);
bool m17_codec2_decode(m17_codec2_t *codec, const uint8_t speech[M17_STREAM_BYTES],
	int16_t pcm[M17_VOICE_PCM_SAMPLES]);

#endif

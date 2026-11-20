// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_PCM_H
#define M17_PCM_H

#include "m17_codec2.h"
#include "../../hal/hal_audio.h"

#define M17_PCM_QUEUE 4

typedef struct {
	uint8_t speech[M17_PCM_QUEUE][M17_STREAM_BYTES];
	uint8_t read, write;
	uint8_t silence[M17_STREAM_BYTES];
	int16_t samples[M17_VOICE_PCM_SAMPLES];
	m17_codec2_t codec;
	const hal_audio_t *audio;
	bool transmit, second;
	uint8_t finish;
} m17_pcm_t;

// Zero before first use. Start and stop in the audio owner's context. Quiesce the modem
// callbacks before stopping the bridge. The owner gives this mode exclusive use of audio.
bool m17_pcm_start(m17_pcm_t *pcm, const hal_audio_t *audio, bool transmit);
void m17_pcm_stop(m17_pcm_t *pcm);
bool m17_pcm_rx_push(m17_pcm_t *pcm, const uint8_t speech[M17_STREAM_BYTES]);
// PTT release: source emits a final block even if the microphone queue is empty.
void m17_pcm_tx_finish(m17_pcm_t *pcm);
bool m17_pcm_tx_speech(uint8_t speech[M17_STREAM_BYTES], bool *final, void *ctx);

#endif

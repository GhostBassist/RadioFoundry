// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17/m17_codec2.h"

#include <assert.h>
#include <string.h>

int main(void)
{
	m17_codec2_t codec = { 0 };
	int16_t pcm[M17_VOICE_PCM_SAMPLES];
	uint8_t speech[M17_STREAM_BYTES] = { 0 };
	assert(!m17_codec2_encode(&codec, pcm, speech));
	assert(!m17_codec2_decode(&codec, speech, pcm));
	assert(m17_codec2_start_tx(&codec) && m17_codec2_start_rx(&codec));

	m17_voice_tx_t tx;
	uint8_t frame[M17_FRAME_BYTES];
	assert(m17_voice_tx_start(&tx, "AB1CD", NULL, 0, frame));
	for (unsigned block = 0; block < 8; block++) {
		for (unsigned i = 0; i < M17_VOICE_PCM_SAMPLES; i++)
			pcm[i] = (int16_t)(((int)(i % 40u) - 20) * 400);
		assert(m17_codec2_encode(&codec, pcm, speech));
		assert(m17_voice_tx_frame(&tx, speech, block == 7, frame));
		m17_stream_frame_t received;
		assert(m17_stream_decode(frame, &received) == 0);
		assert(memcmp(speech, received.data, sizeof(speech)) == 0);
		memset(pcm, 0, sizeof(pcm));
		assert(m17_codec2_decode(&codec, received.data, pcm));
	}
	for (unsigned half = 0; half < 2; half++) {
		int peak = 0;
		for (unsigned i = 0; i < M17_CODEC2_3200_SAMPLES; i++) {
			int sample = pcm[half * M17_CODEC2_3200_SAMPLES + i];
			if (sample < 0)
				sample = -sample;
			if (sample > peak)
				peak = sample;
		}
		assert(peak > 100);
	}
	assert(m17_codec2_start_tx(&codec) && m17_codec2_start_rx(&codec));
	m17_codec2_close(&codec);
	m17_codec2_close(&codec);
	assert(!m17_codec2_encode(&codec, pcm, speech));
	assert(!m17_codec2_decode(&codec, speech, pcm));
	return 0;
}

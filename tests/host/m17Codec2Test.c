// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17/m17_codec2.h"
#include "m17/m17_pcm.h"

#include <assert.h>
#include <string.h>

static audio_block_cb_t callback;
static void *callback_ctx;
static bool speaker;

static bool audio_start(audio_block_cb_t cb, void *ctx)
{
	callback = cb;
	callback_ctx = ctx;
	return true;
}

static void audio_stop(void)
{
	callback = NULL;
}

static void speaker_pcm(bool on)
{
	speaker = on;
}

static void pcm_bridge(void)
{
	static const hal_audio_t audio = { .start = audio_start, .stop = audio_stop, .speaker_pcm = speaker_pcm };
	int16_t mic[AUDIO_BLOCK_SAMPLES];
	int16_t out[AUDIO_BLOCK_SAMPLES];
	uint8_t speech[M17_STREAM_BYTES], next[M17_STREAM_BYTES];
	m17_pcm_t pcm = { 0 };
	bool final = false;
	assert(m17_pcm_start(&pcm, &audio, true));
	assert(!speaker && callback);
	for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
		mic[i] = (int16_t)(((int)(i % 40u) - 20) * 400);
	callback(mic, out, callback_ctx);
	assert(!m17_pcm_tx_speech(speech, &final, &pcm));
	callback(mic, out, callback_ctx);
	assert(m17_pcm_tx_speech(speech, &final, &pcm) && !final);
	m17_pcm_tx_finish(&pcm);
	assert(m17_pcm_tx_speech(next, &final, &pcm) && final);
	assert(memcmp(next, pcm.silence, sizeof(next)) == 0);
	m17_pcm_stop(&pcm);
	assert(!callback);

	assert(m17_pcm_start(&pcm, &audio, false) && speaker);
	for (unsigned i = 0; i < M17_PCM_QUEUE - 1; i++)
		assert(m17_pcm_rx_push(&pcm, speech));
	assert(!m17_pcm_rx_push(&pcm, speech));
	unsigned audible = 0;
	for (unsigned block = 0; block < 2 * (M17_PCM_QUEUE - 1); block++) {
		callback(mic, out, callback_ctx);
		for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
			audible += out[i] != 0;
	}
	assert(audible > 0);
	callback(mic, out, callback_ctx);
	for (unsigned i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
		assert(out[i] == 0);
	m17_pcm_stop(&pcm);
	assert(!speaker && !callback);
}

int main(void)
{
	pcm_bridge();
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

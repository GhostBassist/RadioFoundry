// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_pcm.h"

#include <string.h>

#define LOAD(x) __atomic_load_n(&(x), __ATOMIC_ACQUIRE)
#define STORE(x, v) __atomic_store_n(&(x), (v), __ATOMIC_RELEASE)

_Static_assert(AUDIO_BLOCK_SAMPLES == M17_CODEC2_3200_SAMPLES, "Codec 2 block size");
_Static_assert(AUDIO_RATE_HZ == 8000, "Codec 2 sample rate");

static bool push(m17_pcm_t *pcm, const uint8_t *speech)
{
	uint8_t write = LOAD(pcm->write);
	uint8_t next = (write + 1u) % M17_PCM_QUEUE;
	if (next == LOAD(pcm->read))
		return false;
	memcpy(pcm->speech[write], speech, M17_STREAM_BYTES);
	STORE(pcm->write, next);
	return true;
}

static bool pop(m17_pcm_t *pcm, uint8_t *speech)
{
	uint8_t read = LOAD(pcm->read);
	if (read == LOAD(pcm->write))
		return false;
	memcpy(speech, pcm->speech[read], M17_STREAM_BYTES);
	STORE(pcm->read, (read + 1u) % M17_PCM_QUEUE);
	return true;
}

static void audio_block(const int16_t *mic, int16_t *spk, void *ctx)
{
	m17_pcm_t *pcm = ctx;
	uint8_t speech[M17_STREAM_BYTES];
	size_t half_bytes = AUDIO_BLOCK_SAMPLES * sizeof(*spk);
	if (pcm->transmit) {
		memset(spk, 0, half_bytes);
		if (LOAD(pcm->finish))
			return;
		memcpy(pcm->samples + (pcm->second ? AUDIO_BLOCK_SAMPLES : 0), mic, half_bytes);
		if (pcm->second && m17_codec2_encode(&pcm->codec, pcm->samples, speech))
			push(pcm, speech);
	} else {
		if (!pcm->second) {
			if (!pop(pcm, speech)) {
				memset(spk, 0, half_bytes);
				return;
			}
			m17_codec2_decode(&pcm->codec, speech, pcm->samples);
		}
		memcpy(spk, pcm->samples + (pcm->second ? AUDIO_BLOCK_SAMPLES : 0), half_bytes);
	}
	pcm->second = !pcm->second;
}

bool m17_pcm_start(m17_pcm_t *pcm, const hal_audio_t *audio, bool transmit)
{
	if (pcm->audio || !audio || !audio->start || !audio->stop || (!transmit && !audio->speaker_pcm))
		return false;
	pcm->read = pcm->write = pcm->finish = 0;
	pcm->second = false;
	pcm->transmit = transmit;
	if (!(transmit ? m17_codec2_start_tx(&pcm->codec) : m17_codec2_start_rx(&pcm->codec)))
		return false;
	if (transmit) {
		memset(pcm->samples, 0, sizeof(pcm->samples));
		m17_codec2_encode(&pcm->codec, pcm->samples, pcm->silence);
	}
	if (!audio->start(audio_block, pcm)) {
		m17_codec2_close(&pcm->codec);
		return false;
	}
	pcm->audio = audio;
	if (!transmit)
		audio->speaker_pcm(true);
	return true;
}

void m17_pcm_stop(m17_pcm_t *pcm)
{
	if (!pcm->audio)
		return;
	if (!pcm->transmit)
		pcm->audio->speaker_pcm(false);
	pcm->audio->stop();
	pcm->audio = NULL;
	m17_codec2_close(&pcm->codec);
}

bool m17_pcm_rx_push(m17_pcm_t *pcm, const uint8_t speech[M17_STREAM_BYTES])
{
	return pcm->audio && !pcm->transmit && push(pcm, speech);
}

void m17_pcm_tx_finish(m17_pcm_t *pcm)
{
	STORE(pcm->finish, 1);
}

bool m17_pcm_tx_speech(uint8_t speech[M17_STREAM_BYTES], bool *final, void *ctx)
{
	m17_pcm_t *pcm = ctx;
	if (!pcm->audio || !pcm->transmit)
		return false;
	bool ending = LOAD(pcm->finish) != 0;
	if (pop(pcm, speech)) {
		*final = ending && LOAD(pcm->read) == LOAD(pcm->write);
		return true;
	}
	if (!ending)
		return false;
	memcpy(speech, pcm->silence, M17_STREAM_BYTES);
	*final = true;
	return true;
}

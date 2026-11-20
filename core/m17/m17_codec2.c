// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_codec2.h"

#include <codec2.h>

static bool start(struct CODEC2 **state)
{
	if (*state)
		codec2_destroy(*state);
	*state = codec2_create(CODEC2_MODE_3200);
	if (!*state)
		return false;
	if (codec2_samples_per_frame(*state) == M17_CODEC2_3200_SAMPLES &&
	    codec2_bits_per_frame(*state) == M17_CODEC2_3200_BYTES * 8)
		return true;
	codec2_destroy(*state);
	*state = NULL;
	return false;
}

bool m17_codec2_start_tx(m17_codec2_t *codec)
{
	return start(&codec->encoder);
}

bool m17_codec2_start_rx(m17_codec2_t *codec)
{
	return start(&codec->decoder);
}

void m17_codec2_close(m17_codec2_t *codec)
{
	if (codec->encoder)
		codec2_destroy(codec->encoder);
	if (codec->decoder)
		codec2_destroy(codec->decoder);
	*codec = (m17_codec2_t){ 0 };
}

bool m17_codec2_encode(m17_codec2_t *codec, int16_t pcm[M17_VOICE_PCM_SAMPLES],
	uint8_t speech[M17_STREAM_BYTES])
{
	if (!codec->encoder)
		return false;
	codec2_encode(codec->encoder, speech, pcm);
	codec2_encode(codec->encoder, speech + M17_CODEC2_3200_BYTES, pcm + M17_CODEC2_3200_SAMPLES);
	return true;
}

bool m17_codec2_decode(m17_codec2_t *codec, const uint8_t speech[M17_STREAM_BYTES],
	int16_t pcm[M17_VOICE_PCM_SAMPLES])
{
	if (!codec->decoder)
		return false;
	codec2_decode(codec->decoder, pcm, speech);
	codec2_decode(codec->decoder, pcm + M17_CODEC2_3200_SAMPLES, speech + M17_CODEC2_3200_BYTES);
	return true;
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_voice.h"

#include <string.h>

bool m17_voice_tx_start(m17_voice_tx_t *tx, const char *source, const char *destination, uint8_t can,
	uint8_t frame[M17_FRAME_BYTES])
{
	*tx = (m17_voice_tx_t){ 0 };
	if (!m17_voice_link(source, destination, can, tx->link))
		return false;
	uint16_t crc = m17_crc(tx->link, M17_LINK_DATA_BYTES);
	tx->link[M17_LINK_DATA_BYTES] = (uint8_t)(crc >> 8);
	tx->link[M17_LINK_DATA_BYTES + 1] = (uint8_t)crc;
	m17_link_encode(tx->link, frame);
	tx->active = true;
	return true;
}

bool m17_voice_tx_frame(m17_voice_tx_t *tx, const uint8_t speech[M17_STREAM_BYTES], bool final,
	uint8_t frame[M17_FRAME_BYTES])
{
	if (!tx->active)
		return false;
	m17_stream_frame_t stream = {
		.number = (uint16_t)(tx->number | (final ? M17_STREAM_FINAL : 0u)),
		.lich_index = tx->lich_index,
	};
	memcpy(stream.lich, tx->link + tx->lich_index * M17_LICH_BYTES, sizeof(stream.lich));
	memcpy(stream.data, speech, sizeof(stream.data));
	if (m17_stream_encode(&stream, frame) < 0)
		return false;
	tx->number = (uint16_t)((tx->number + 1u) & (M17_STREAM_FINAL - 1u));
	tx->lich_index = (uint8_t)((tx->lich_index + 1u) % 6u);
	tx->active = !final;
	return true;
}

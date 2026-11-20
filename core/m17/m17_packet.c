// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_packet.h"

#include <string.h>

enum { BURST_DONE, BURST_PREAMBLE, BURST_LINK, BURST_PAYLOAD };

int m17_packet_tx_begin(m17_packet_tx_t *tx, const uint8_t *data, size_t len)
{
	*tx = (m17_packet_tx_t){ 0 };
	if (!data || len == 0 || len > M17_PACKET_MAX_BYTES)
		return -1;
	tx->data = data;
	tx->len = len;
	tx->crc = m17_crc(data, len);
	return 0;
}

int m17_packet_tx_next(m17_packet_tx_t *tx, m17_packet_chunk_t *chunk)
{
	if (!tx->data || tx->offset == tx->len + 2)
		return 0;
	size_t remaining = tx->len + 2 - tx->offset;
	*chunk = (m17_packet_chunk_t){ 0 };
	chunk->final = remaining <= M17_PACKET_CHUNK_BYTES;
	chunk->counter = chunk->final ? remaining : tx->offset / M17_PACKET_CHUNK_BYTES;
	size_t count = chunk->final ? remaining : M17_PACKET_CHUNK_BYTES;
	for (size_t i = 0; i < count; i++) {
		size_t pos = tx->offset++;
		if (pos < tx->len)
			chunk->data[i] = tx->data[pos];
		else if (pos == tx->len)
			chunk->data[i] = (uint8_t)(tx->crc >> 8);
		else
			chunk->data[i] = (uint8_t)tx->crc;
	}
	return 1;
}

int m17_packet_burst_begin(m17_packet_burst_t *burst, const uint8_t link[M17_LINK_DATA_BYTES],
	const uint8_t *data, size_t len)
{
	burst->stage = BURST_DONE;
	// TYPE is big endian at bytes 12–13. Bit zero selects stream mode.
	if ((link[13] & 1u) || m17_packet_tx_begin(&burst->packet, data, len) < 0)
		return -1;
	memcpy(burst->link, link, sizeof(burst->link));
	burst->stage = BURST_PREAMBLE;
	return 0;
}

int m17_packet_burst_next(m17_packet_burst_t *burst, uint8_t frame[M17_FRAME_BYTES])
{
	switch (burst->stage) {
	case BURST_PREAMBLE:
		m17_preamble(frame);
		burst->stage = BURST_LINK;
		return 1;
	case BURST_LINK:
		m17_link_encode(burst->link, frame);
		burst->stage = BURST_PAYLOAD;
		return 1;
	case BURST_PAYLOAD: {
		m17_packet_chunk_t chunk;
		if (m17_packet_tx_next(&burst->packet, &chunk))
			return m17_packet_encode(&chunk, frame) == 0 ? 1 : -1;
		m17_end_marker(frame);
		burst->stage = BURST_DONE;
		return 1;
	}
	default:
		return 0;
	}
}

void m17_packet_rx_begin(m17_packet_rx_t *rx)
{
	rx->len = 0;
	rx->active = 1;
}

static int reject(m17_packet_rx_t *rx)
{
	rx->active = 0;
	rx->len = 0;
	return -1;
}

int m17_packet_rx_push(m17_packet_rx_t *rx, const m17_packet_chunk_t *chunk)
{
	if (!rx->active)
		return -1;
	if (chunk->final > 1)
		return reject(rx);
	size_t count = M17_PACKET_CHUNK_BYTES;
	if (chunk->final) {
		count = chunk->counter;
		if (count == 0 || count > M17_PACKET_CHUNK_BYTES)
			return reject(rx);
	} else if (chunk->counter > 31 || chunk->counter != rx->len / M17_PACKET_CHUNK_BYTES) {
		return reject(rx);
	}
	memcpy(rx->data + rx->len, chunk->data, count);
	rx->len += count;
	if (!chunk->final)
		return 0;
	if (rx->len < 3 || m17_crc(rx->data, rx->len) != 0)
		return reject(rx);
	rx->len -= 2;
	rx->active = 0;
	return 1;
}

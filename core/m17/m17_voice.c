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

void m17_voice_rx_init(m17_voice_rx_t *rx, uint8_t can, const m17_voice_rx_ops_t *ops, void *ctx)
{
	*rx = (m17_voice_rx_t){ .ctx = ctx, .can = can };
	if (ops)
		rx->ops = *ops;
}

void m17_voice_rx_reset(m17_voice_rx_t *rx)
{
	rx->next_fragment = 0;
	if (rx->active) {
		rx->active = false;
		if (rx->ops.end)
			rx->ops.end(rx->ctx);
	}
}

static bool start_rx(m17_voice_rx_t *rx, const uint8_t *link, bool late)
{
	// Accept clear, unsigned Codec 2 3200 voice. META type does not affect speech.
	if (!rx->ops.speech || (link[13] & 0x1Fu) != 0x05u || (link[12] & 0x40u) ||
	    (rx->can != M17_CAN_ANY && rx->can != m17_link_can(link)))
		return false;
	rx->active = true;
	if (rx->ops.start)
		rx->ops.start(link, late, rx->ctx);
	return true;
}

void m17_voice_rx_link(m17_voice_rx_t *rx, const uint8_t link[M17_LINK_DATA_BYTES])
{
	m17_voice_rx_reset(rx);
	if (start_rx(rx, link, false))
		rx->next_number = 0;
}

void m17_voice_rx_frame(m17_voice_rx_t *rx, const m17_stream_frame_t *frame)
{
	uint16_t number = frame->number & (M17_STREAM_FINAL - 1u);
	bool final = (frame->number & M17_STREAM_FINAL) != 0;
	if (rx->active && number != rx->next_number)
		m17_voice_rx_reset(rx);
	if (!rx->active) {
		if (frame->lich_index != 0 &&
		    (frame->lich_index != rx->next_fragment || number != rx->next_number)) {
			rx->next_fragment = 0;
			return;
		}
		memcpy(rx->fragments + frame->lich_index * M17_LICH_BYTES, frame->lich, M17_LICH_BYTES);
		rx->next_fragment = frame->lich_index + 1u;
		if (rx->next_fragment == 6u) {
			rx->next_fragment = 0;
			if (!final && m17_crc(rx->fragments, sizeof(rx->fragments)) == 0)
				start_rx(rx, rx->fragments, true);
		}
	}
	rx->next_number = (number + 1u) & (M17_STREAM_FINAL - 1u);
	if (rx->active)
		rx->ops.speech(frame->data, number, rx->ctx);
	if (final)
		m17_voice_rx_reset(rx);
}

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_rx.h"

#include "../../hal/hal_radio.h"

#include <string.h>

enum { SYNC_LINK, SYNC_PACKET, SYNC_STREAM };

static void rx_lost(void *ctx)
{
	m17_rx_t *rx = ctx;
	rx->packet.active = 0;
	m17_voice_rx_reset(&rx->voice);
}

static uint16_t rx_head(const air_frame_t *frame, void *ctx)
{
	m17_rx_t *rx = ctx;
	if (frame->flags & AIR_FRAME_NO_SYNC)
		return 0;
	if (frame->sync_id == SYNC_LINK)
		return M17_FRAME_SYMBOLS;
	if (frame->sync_id == SYNC_STREAM)
		return rx->voice.ops.speech && !rx->packet.active ? M17_FRAME_SYMBOLS : 0;
	return rx->packet.active ? M17_FRAME_SYMBOLS : 0;
}

static void rx_frame(const air_frame_t *frame, void *ctx)
{
	m17_rx_t *rx = ctx;
	uint8_t packed[M17_FRAME_BYTES];
	memcpy(packed, frame->hard, sizeof(packed));

	// The framer already accepted the sync, which can contain a symbol error.
	if (frame->sync_id == SYNC_LINK) {
		packed[0] = 0x55;
		packed[1] = 0xF7;
		rx_lost(rx);
		if (m17_link_decode(packed, rx->link) < 0)
			return;
		if (rx->link[13] & 1u)
			m17_voice_rx_link(&rx->voice, rx->link);
		else if (rx->can == M17_CAN_ANY || rx->can == m17_link_can(rx->link))
			m17_packet_rx_begin(&rx->packet);
		return;
	}

	if (frame->sync_id == SYNC_STREAM) {
		packed[0] = 0xFF;
		packed[1] = 0x5D;
		m17_stream_frame_t stream;
		if (m17_stream_decode(packed, &stream) >= 0)
			m17_voice_rx_frame(&rx->voice, &stream);
		return;
	}

	packed[0] = 0x75;
	packed[1] = 0xFF;
	m17_packet_chunk_t chunk;
	if (m17_packet_decode(packed, &chunk) < 0) {
		rx->packet.active = 0;
		return;
	}
	if (m17_packet_rx_push(&rx->packet, &chunk) == 1 && rx->received)
		rx->received(rx->link, rx->packet.data, rx->packet.len, rx->ctx);
}

const air_mode_t m17_air_mode = {
	.name = "M17",
	.profile = {
		.sym_rate = 4800,
		.levels = 4,
		.shape = AIR_SHAPE_RRC,
		.shape_param = 50,
		.dev_outer_hz = 2400,
		.timing = AIR_TIMING_CONTINUOUS,
		.bw = RADIO_BW_12K5,
		.sync_any_syms = M17_FRAME_SYMBOLS,
		.idle = 0x55,
		.nsyncs = 3,
		.sync = {
			{ .id = SYNC_LINK, .nsyms = 24, .max_errs = 2, .flags = AIR_SYNC_START_ONLY,
			  .frame_at = 16, .syms = { 0x77, 0x77, 0x77, 0x77, 0x55, 0xF7 } },
			{ .id = SYNC_PACKET, .nsyms = 8, .max_errs = 1, .syms = { 0x75, 0xFF } },
			{ .id = SYNC_STREAM, .nsyms = 8, .max_errs = 1, .syms = { 0xFF, 0x5D } },
		},
	},
	.frame_syms = M17_FRAME_SYMBOLS,
	.head_syms = 8,
	.lost_frames = 1,
	.head = rx_head,
	.rx_frame = rx_frame,
	.rx_lost = rx_lost,
};

void m17_rx_init(m17_rx_t *rx, uint8_t can,
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx),
	const m17_voice_rx_ops_t *voice, void *ctx)
{
	rx->packet.active = 0;
	rx->can = can;
	rx->received = received;
	rx->ctx = ctx;
	m17_voice_rx_init(&rx->voice, can, voice, ctx);
}

void m17_rx_reset(m17_rx_t *rx)
{
	rx_lost(rx);
}

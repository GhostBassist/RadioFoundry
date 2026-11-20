// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_rx.h"

#include "../../hal/hal_radio.h"

#include <string.h>

enum { SYNC_LINK, SYNC_PACKET };

static void rx_lost(void *ctx)
{
	m17_rx_t *rx = ctx;
	rx->packet.active = 0;
}

static uint16_t rx_head(const air_frame_t *frame, void *ctx)
{
	m17_rx_t *rx = ctx;
	if (frame->flags & AIR_FRAME_NO_SYNC)
		return 0;
	if (frame->sync_id == SYNC_LINK)
		return M17_FRAME_SYMBOLS;
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
		rx->packet.active = 0;
		if (m17_link_decode(packed, rx->link) >= 0 && !(rx->link[13] & 1u) &&
		    (rx->can == M17_CAN_ANY || rx->can == m17_link_can(rx->link)))
			m17_packet_rx_begin(&rx->packet);
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

const air_mode_t m17_packet_air_mode = {
	.name = "M17 packet",
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
		.nsyncs = 2,
		.sync = {
			{ .id = SYNC_LINK, .nsyms = 24, .max_errs = 2, .flags = AIR_SYNC_START_ONLY,
			  .frame_at = 16, .syms = { 0x77, 0x77, 0x77, 0x77, 0x55, 0xF7 } },
			{ .id = SYNC_PACKET, .nsyms = 8, .max_errs = 1, .syms = { 0x75, 0xFF } },
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
	void (*received)(const uint8_t *link, const uint8_t *data, size_t len, void *ctx), void *ctx)
{
	rx->packet.active = 0;
	rx->can = can;
	rx->received = received;
	rx->ctx = ctx;
}

void m17_rx_reset(m17_rx_t *rx)
{
	rx_lost(rx);
}

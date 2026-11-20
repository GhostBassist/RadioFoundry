// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17/m17_frame.h"
#include "m17/m17_packet.h"
#include "m17/m17_engine.h"

#include <assert.h>
#include <string.h>

// Generated from the published bit ordering and FEC independently of the C codec.
static const uint8_t link_vector[M17_FRAME_BYTES] = {
	0x55, 0xF7, 0xD6, 0xB5, 0xE2, 0xB0, 0x82, 0xFF,
	0x85, 0x62, 0xBB, 0x4E, 0x96, 0x90, 0xD8, 0x98,
	0xDD, 0x7D, 0x0C, 0xC8, 0x52, 0x43, 0xD1, 0x1D,
	0xF8, 0x6E, 0x68, 0x2F, 0x3D, 0xDA, 0x14, 0xEA,
	0xDD, 0x66, 0x19, 0x8D, 0xD5, 0x82, 0xD1, 0x33,
	0x87, 0x13, 0x57, 0x18, 0x29, 0x29, 0x78, 0xC3,
};

static const uint8_t packet_vector[M17_FRAME_BYTES] = {
	0x75, 0xFF, 0xD6, 0x34, 0xE2, 0x31, 0x82, 0xFF,
	0x84, 0x63, 0xBA, 0x4E, 0xB6, 0x90, 0xF8, 0x98,
	0xDD, 0x5D, 0x0C, 0xC8, 0x52, 0x0B, 0x99, 0x15,
	0xF8, 0x66, 0x60, 0x3F, 0x35, 0xDA, 0x14, 0xFA,
	0xDF, 0x76, 0x19, 0x8F, 0xD5, 0x80, 0xD3, 0x33,
	0x83, 0x13, 0x53, 0x18, 0x2D, 0x29, 0xF8, 0x43,
};

static void packet_round_trip(size_t len)
{
	uint8_t payload[M17_PACKET_MAX_BYTES];
	for (size_t i = 0; i < len; i++)
		payload[i] = (uint8_t)i;
	m17_packet_tx_t tx;
	m17_packet_rx_t rx;
	m17_packet_chunk_t chunk, decoded;
	uint8_t frame[M17_FRAME_BYTES];
	assert(m17_packet_tx_begin(&tx, payload, len) == 0);
	m17_packet_rx_begin(&rx);
	unsigned count = 0;
	while (m17_packet_tx_next(&tx, &chunk)) {
		assert(m17_packet_encode(&chunk, frame) == 0);
		assert(m17_packet_decode(frame, &decoded) == 0);
		assert(m17_packet_rx_push(&rx, &decoded) == chunk.final);
		count++;
	}
	assert(count == (len + 26) / 25);
	assert(!rx.active && rx.len == len);
	assert(memcmp(payload, rx.data, len) == 0);
	assert(m17_packet_rx_push(&rx, &decoded) == -1);
}

static void packet_rejects_errors(void)
{
	const uint8_t payload[24] = { 1 };
	m17_packet_tx_t tx;
	m17_packet_rx_t rx;
	m17_packet_chunk_t chunk;
	assert(m17_packet_tx_begin(&tx, payload, 0) == -1);
	assert(m17_packet_tx_next(&tx, &chunk) == 0);
	assert(m17_packet_tx_begin(&tx, payload, sizeof(payload)) == 0);
	assert(m17_packet_tx_next(&tx, &chunk) == 1);
	m17_packet_rx_begin(&rx);
	chunk.counter = 1;
	assert(m17_packet_rx_push(&rx, &chunk) == -1 && !rx.active);
	chunk.counter = 0;
	m17_packet_rx_begin(&rx);
	assert(m17_packet_rx_push(&rx, &chunk) == 0);
	assert(m17_packet_tx_next(&tx, &chunk) == 1);
	chunk.data[0] ^= 1;
	assert(m17_packet_rx_push(&rx, &chunk) == -1 && !rx.active && rx.len == 0);
}

static void packet_burst(void)
{
	m17_packet_burst_t burst;
	uint8_t link[M17_LINK_DATA_BYTES] = { 0 };
	const uint8_t payload[] = { 1 };
	uint8_t frame[M17_FRAME_BYTES];
	assert(m17_packet_burst_begin(&burst, link, payload, sizeof(payload)) == 0);
	assert(m17_packet_burst_next(&burst, frame) == 1);
	for (unsigned i = 0; i < sizeof(frame); i++)
		assert(frame[i] == 0x77);
	assert(m17_packet_burst_next(&burst, frame) == 1);
	assert(memcmp(frame, link_vector, sizeof(frame)) == 0);
	assert(m17_packet_burst_next(&burst, frame) == 1);
	m17_packet_chunk_t chunk;
	assert(m17_packet_decode(frame, &chunk) == 0 && chunk.final && chunk.counter == 3);
	assert(m17_packet_burst_next(&burst, frame) == 1);
	for (unsigned i = 0; i < sizeof(frame); i += 2)
		assert(frame[i] == 0x55 && frame[i + 1] == 0x5D);
	assert(m17_packet_burst_next(&burst, frame) == 0);
	link[13] = 1;
	assert(m17_packet_burst_begin(&burst, link, payload, sizeof(payload)) == -1);
	assert(m17_packet_burst_next(&burst, frame) == 0);
}

static void received(const uint8_t *link, const uint8_t *data, size_t len, void *ctx)
{
	assert(link[0] == 42 && len == 24);
	for (size_t i = 0; i < len; i++)
		assert(data[i] == i);
	++*(unsigned *)ctx;
}

static air_event_cb_t on_air;
static void *air_ctx;
static uint32_t fit_flags;
static air_tx_chunk_t queued;
static bool pending;

static uint32_t fit(const air_profile_t *profile)
{
	assert(profile->sym_rate == 4800 && profile->shape_param == 50);
	return fit_flags;
}

static bool open_air(const air_profile_t *profile, air_event_cb_t cb, void *ctx)
{
	(void)profile;
	on_air = cb;
	air_ctx = ctx;
	pending = false;
	return true;
}

static bool queue_air(const air_tx_chunk_t *chunk)
{
	if (pending)
		return false;
	queued = *chunk;
	pending = true;
	return true;
}

static uint8_t space_air(void)
{
	return pending ? 0 : 1;
}

static bool begin_air(void)
{
	return pending;
}

static void end_air(void)
{
	assert(pending);
	queued.flags |= AIR_TX_LAST;
}

static const hal_air_t provider = {
	.fit = fit, .open = open_air, .tx_queue = queue_air, .tx_begin = begin_air,
	.tx_space = space_air, .tx_end = end_air, .tx_queue_depth = 1, .tx_chunk_syms = 80,
};

static void transmit_burst(void)
{
	const uint8_t payload[] = { 1 };
	const uint8_t link[M17_LINK_DATA_BYTES] = { 0 };
	m17_engine_t engine;
	fit_flags = AIR_FIT_RX | AIR_FIT_TX | AIR_FIT_SHAPE_APPROX;
	assert(m17_engine_open(&engine, &provider, NULL, NULL));
	assert(!m17_engine_prepare(&engine, link, payload, sizeof(payload)));
	fit_flags = AIR_FIT_RX | AIR_FIT_TX;
	assert(m17_engine_open(&engine, &provider, NULL, NULL));
	assert(m17_engine_prepare(&engine, link, payload, sizeof(payload)));
	assert(air_session_tx_prime(&engine.session));
	assert(air_session_tx_go(&engine.session));

	m17_packet_burst_t expected;
	uint8_t wire[4 * M17_FRAME_BYTES];
	assert(m17_packet_burst_begin(&expected, link, payload, sizeof(payload)) == 0);
	for (unsigned i = 0; i < 4; i++)
		assert(m17_packet_burst_next(&expected, wire + i * M17_FRAME_BYTES) == 1);
	unsigned bytes = 0;
	while (pending) {
		assert(bytes + queued.nsyms / 4 <= sizeof(wire));
		assert(memcmp(queued.syms, wire + bytes, queued.nsyms / 4) == 0);
		bytes += queued.nsyms / 4;
		const air_event_t event = { .kind = (queued.flags & AIR_TX_LAST) ? AIR_EV_TX_DONE : AIR_EV_TX_SPACE };
		pending = false;
		on_air(&event, air_ctx);
	}
	assert(bytes == sizeof(wire));
	assert(air_session_tx_state(&engine.session) == AIR_TX_DONE);
}

static void receive_burst(bool gap)
{
	uint8_t data[24];
	for (unsigned i = 0; i < sizeof(data); i++)
		data[i] = (uint8_t)i;
	const uint8_t link[M17_LINK_DATA_BYTES] = { 42 };
	m17_packet_burst_t burst;
	assert(m17_packet_burst_begin(&burst, link, data, sizeof(data)) == 0);
	uint8_t wire[5 * M17_FRAME_BYTES];
	for (unsigned i = 0; i < 5; i++)
		assert(m17_packet_burst_next(&burst, wire + i * M17_FRAME_BYTES) == 1);
	// Damage one sync symbol. The framer can still identify the packet sync.
	wire[2 * M17_FRAME_BYTES] ^= 0x40;
	m17_engine_t engine;
	unsigned deliveries = 0;
	fit_flags = AIR_FIT_RX;
	assert(m17_engine_open(&engine, &provider, received, &deliveries));
	for (unsigned offset = 0; offset < sizeof(wire); offset += 23) {
		unsigned bytes = sizeof(wire) - offset;
		if (bytes > 23)
			bytes = 23;
		const air_chunk_t chunk = {
			.t_us = offset * 4u * 1000000u / 4800u,
			.nsyms = (uint16_t)(bytes * 4u),
			.sync_at = -1,
			.flags = gap && offset == 115 ? AIR_CHUNK_GAP : 0,
			.hard = wire + offset,
		};
		const air_event_t event = { .kind = AIR_EV_RX, .chunk = &chunk };
		on_air(&event, air_ctx);
	}
	assert(deliveries == (gap ? 0u : 1u));
}

int main(void)
{
	transmit_burst();
	receive_burst(false);
	receive_burst(true);
	packet_burst();
	packet_round_trip(1);
	packet_round_trip(23);
	packet_round_trip(24);
	packet_round_trip(25);
	packet_round_trip(M17_PACKET_MAX_BYTES);
	packet_rejects_errors();
	assert(m17_crc(NULL, 0) == 0xFFFF);
	assert(m17_crc((const uint8_t *)"A", 1) == 0x206E);
	assert(m17_crc((const uint8_t *)"123456789", 9) == 0x772B);

	uint8_t frame[M17_FRAME_BYTES];
	const uint8_t link[M17_LINK_DATA_BYTES] = { 0 };
	uint8_t decoded_link[M17_LINK_DATA_BYTES];
	m17_link_encode(link, frame);
	assert(memcmp(frame, link_vector, sizeof(frame)) == 0);
	frame[5] ^= 0x20;
	assert(m17_link_decode(frame, decoded_link) == 1);
	assert(memcmp(link, decoded_link, sizeof(link)) == 0);

	memset(frame + 2, 0, sizeof(frame) - 2);
	assert(m17_link_decode(frame, decoded_link) == -1);
	assert(m17_link_decode(packet_vector, decoded_link) == -1);

	m17_packet_chunk_t chunk = { .data = { 0x12, 0x34, 0xFF }, .counter = 2, .final = 1 };
	m17_packet_chunk_t decoded_chunk;
	assert(m17_packet_encode(&chunk, frame) == 0);
	assert(memcmp(frame, packet_vector, sizeof(frame)) == 0);
	assert(m17_packet_decode(frame, &decoded_chunk) == 0);
	assert(decoded_chunk.counter == 2 && decoded_chunk.final == 1);
	assert(decoded_chunk.data[0] == 0x12 && decoded_chunk.data[1] == 0x34 && decoded_chunk.data[2] == 0);
	assert(m17_packet_decode(link_vector, &decoded_chunk) == -1);

	chunk.counter = 31;
	assert(m17_packet_encode(&chunk, frame) == -1);
	chunk.final = 0;
	assert(m17_packet_encode(&chunk, frame) == 0);
	assert(m17_packet_decode(frame, &decoded_chunk) == 0);
	assert(decoded_chunk.counter == 31 && decoded_chunk.final == 0);
	assert(memcmp(decoded_chunk.data, chunk.data, sizeof(chunk.data)) == 0);
	return 0;
}

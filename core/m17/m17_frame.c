// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_frame.h"
#include "fec/conv.h"

#include <string.h>

#define LINK_SYNC 0x55F7
#define PACKET_SYNC 0x75FF
#define LINK_INPUT_BITS 244
#define PACKET_INPUT_BITS 210
#define BODY_BYTES (M17_FRAME_BYTES - 2)

static const fec_conv_code_t code = { 0x19, 0x17 };
static const uint8_t randomizer[BODY_BYTES] = {
	0xD6, 0xB5, 0xE2, 0x30, 0x82, 0xFF, 0x84, 0x62, 0xBA, 0x4E,
	0x96, 0x90, 0xD8, 0x98, 0xDD, 0x5D, 0x0C, 0xC8, 0x52, 0x43,
	0x91, 0x1D, 0xF8, 0x6E, 0x68, 0x2F, 0x35, 0xDA, 0x14, 0xEA,
	0xCD, 0x76, 0x19, 0x8D, 0xD5, 0x80, 0xD1, 0x33, 0x87, 0x13,
	0x57, 0x18, 0x2D, 0x29, 0x78, 0xC3,
};

uint16_t m17_crc(const uint8_t *data, size_t len)
{
	uint16_t crc = 0xFFFF;
	while (len--) {
		crc ^= (uint16_t)*data++ << 8;
		for (unsigned bit = 0; bit < 8; bit++)
			crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x5935 : 0));
	}
	return crc;
}

static unsigned interleave(unsigned bit)
{
	return (45u * bit + 92u * bit * bit) % (BODY_BYTES * 8u);
}

static int kept(unsigned bit, unsigned nbits)
{
	// P1 repeats every 61 coded bits. P3 repeats every eight.
	if (nbits == LINK_INPUT_BITS)
		return (bit % 61u) % 4u != 2u;
	return bit % 8u != 7u;
}

static unsigned get_bit(const uint8_t *data, unsigned bit)
{
	return (data[bit / 8u] >> (7u - bit % 8u)) & 1u;
}

static void encode(const uint8_t *data, unsigned nbits, uint16_t sync, uint8_t *frame)
{
	uint8_t coded[(LINK_INPUT_BITS * 2 + 7) / 8];
	fec_conv_encode(&code, data, nbits, coded);
	frame[0] = (uint8_t)(sync >> 8);
	frame[1] = (uint8_t)sync;
	memcpy(frame + 2, randomizer, BODY_BYTES);
	unsigned index = 0;
	for (unsigned bit = 0; bit < 2u * nbits; bit++) {
		if (!kept(bit, nbits))
			continue;
		unsigned pos = interleave(index++);
		frame[2 + pos / 8u] ^= (uint8_t)(get_bit(coded, bit) << (7u - pos % 8u));
	}
}

static int decode(const uint8_t *frame, unsigned nbits, uint16_t sync, uint8_t *data)
{
	if (((frame[0] << 8) | frame[1]) != sync)
		return -1;
	int8_t soft[LINK_INPUT_BITS * 2] = { 0 };
	uint16_t decisions[LINK_INPUT_BITS];
	unsigned index = 0;
	for (unsigned bit = 0; bit < 2u * nbits; bit++) {
		if (!kept(bit, nbits))
			continue;
		unsigned pos = interleave(index++);
		soft[bit] = (get_bit(frame + 2, pos) ^ get_bit(randomizer, pos)) ? 1 : -1;
	}
	return fec_conv_decode(&code, soft, nbits, decisions, data);
}

void m17_link_encode(const uint8_t data[M17_LINK_DATA_BYTES], uint8_t frame[M17_FRAME_BYTES])
{
	uint8_t input[M17_LINK_DATA_BYTES + 3] = { 0 };
	memcpy(input, data, M17_LINK_DATA_BYTES);
	uint16_t crc = m17_crc(data, M17_LINK_DATA_BYTES);
	input[M17_LINK_DATA_BYTES] = (uint8_t)(crc >> 8);
	input[M17_LINK_DATA_BYTES + 1] = (uint8_t)crc;
	encode(input, LINK_INPUT_BITS, LINK_SYNC, frame);
}

int m17_link_decode(const uint8_t frame[M17_FRAME_BYTES], uint8_t data[M17_LINK_DATA_BYTES])
{
	uint8_t input[M17_LINK_DATA_BYTES + 3];
	int corrections = decode(frame, LINK_INPUT_BITS, LINK_SYNC, input);
	if (corrections < 0 || m17_crc(input, M17_LINK_DATA_BYTES + 2) != 0)
		return -1;
	memcpy(data, input, M17_LINK_DATA_BYTES);
	return corrections;
}

static int valid_counter(unsigned counter, unsigned final)
{
	return final ? counter >= 1 && counter <= M17_PACKET_CHUNK_BYTES : counter <= 31;
}

int m17_packet_encode(const m17_packet_chunk_t *chunk, uint8_t frame[M17_FRAME_BYTES])
{
	if (chunk->final > 1 || !valid_counter(chunk->counter, chunk->final))
		return -1;
	uint8_t input[M17_PACKET_CHUNK_BYTES + 2] = { 0 };
	memcpy(input, chunk->data, chunk->final ? chunk->counter : M17_PACKET_CHUNK_BYTES);
	input[M17_PACKET_CHUNK_BYTES] = (uint8_t)((chunk->final << 7) | (chunk->counter << 2));
	encode(input, PACKET_INPUT_BITS, PACKET_SYNC, frame);
	return 0;
}

int m17_packet_decode(const uint8_t frame[M17_FRAME_BYTES], m17_packet_chunk_t *chunk)
{
	uint8_t input[M17_PACKET_CHUNK_BYTES + 2];
	int corrections = decode(frame, PACKET_INPUT_BITS, PACKET_SYNC, input);
	if (corrections < 0)
		return -1;
	unsigned final = input[M17_PACKET_CHUNK_BYTES] >> 7;
	unsigned counter = (input[M17_PACKET_CHUNK_BYTES] >> 2) & 31u;
	if (!valid_counter(counter, final))
		return -1;
	memcpy(chunk->data, input, M17_PACKET_CHUNK_BYTES);
	chunk->counter = (uint8_t)counter;
	chunk->final = (uint8_t)final;
	return corrections;
}

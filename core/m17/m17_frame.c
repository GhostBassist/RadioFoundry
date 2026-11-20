// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#include "m17_frame.h"
#include "fec/conv.h"
#include "fec/golay.h"

#include <string.h>

#define LINK_SYNC 0x55F7
#define PACKET_SYNC 0x75FF
#define STREAM_SYNC 0xFF5D
#define LINK_INPUT_BITS 244
#define PACKET_INPUT_BITS 210
#define STREAM_INPUT_BITS 148
#define ENCODED_LICH_BYTES 12
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

void m17_preamble(uint8_t frame[M17_FRAME_BYTES])
{
	memset(frame, 0x77, M17_FRAME_BYTES);
}

void m17_end_marker(uint8_t frame[M17_FRAME_BYTES])
{
	for (unsigned i = 0; i < M17_FRAME_BYTES; i += 2) {
		frame[i] = 0x55;
		frame[i + 1] = 0x5D;
	}
}

static unsigned interleave(unsigned bit)
{
	return (45u * bit + 92u * bit * bit) % (BODY_BYTES * 8u);
}

static int kept(unsigned bit, unsigned nbits)
{
	// P1, P2 and P3 repeat every 61, 12 and eight coded bits respectively.
	if (nbits == LINK_INPUT_BITS)
		return (bit % 61u) % 4u != 2u;
	if (nbits == STREAM_INPUT_BITS)
		return bit % 12u != 11u;
	return bit % 8u != 7u;
}

static unsigned get_bit(const uint8_t *data, unsigned bit)
{
	return (data[bit / 8u] >> (7u - bit % 8u)) & 1u;
}

static void encode(const uint8_t *data, unsigned nbits, uint16_t sync, const uint8_t *lich, uint8_t *frame)
{
	uint8_t coded[(LINK_INPUT_BITS * 2 + 7) / 8];
	fec_conv_encode(&code, data, nbits, coded);
	frame[0] = (uint8_t)(sync >> 8);
	frame[1] = (uint8_t)sync;
	memcpy(frame + 2, randomizer, BODY_BYTES);
	unsigned index = 0;
	if (lich) {
		for (; index < ENCODED_LICH_BYTES * 8u; index++) {
			unsigned pos = interleave(index);
			frame[2 + pos / 8u] ^= (uint8_t)(get_bit(lich, index) << (7u - pos % 8u));
		}
	}
	for (unsigned bit = 0; bit < 2u * nbits; bit++) {
		if (!kept(bit, nbits))
			continue;
		unsigned pos = interleave(index++);
		frame[2 + pos / 8u] ^= (uint8_t)(get_bit(coded, bit) << (7u - pos % 8u));
	}
}

static unsigned read_body_bit(const uint8_t *frame, unsigned index)
{
	unsigned pos = interleave(index);
	return get_bit(frame + 2, pos) ^ get_bit(randomizer, pos);
}

static int decode(const uint8_t *frame, unsigned nbits, uint16_t sync, uint8_t *lich, uint8_t *data)
{
	if (((frame[0] << 8) | frame[1]) != sync)
		return -1;
	int8_t soft[LINK_INPUT_BITS * 2] = { 0 };
	uint16_t decisions[LINK_INPUT_BITS];
	unsigned index = 0;
	if (lich) {
		memset(lich, 0, ENCODED_LICH_BYTES);
		for (; index < ENCODED_LICH_BYTES * 8u; index++)
			lich[index / 8u] |= (uint8_t)(read_body_bit(frame, index) << (7u - index % 8u));
	}
	for (unsigned bit = 0; bit < 2u * nbits; bit++) {
		if (!kept(bit, nbits))
			continue;
		soft[bit] = read_body_bit(frame, index++) ? 1 : -1;
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
	encode(input, LINK_INPUT_BITS, LINK_SYNC, NULL, frame);
}

int m17_link_decode(const uint8_t frame[M17_FRAME_BYTES], uint8_t data[M17_LINK_DATA_BYTES])
{
	uint8_t input[M17_LINK_DATA_BYTES + 3];
	int corrections = decode(frame, LINK_INPUT_BITS, LINK_SYNC, NULL, input);
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
	encode(input, PACKET_INPUT_BITS, PACKET_SYNC, NULL, frame);
	return 0;
}

int m17_packet_decode(const uint8_t frame[M17_FRAME_BYTES], m17_packet_chunk_t *chunk)
{
	uint8_t input[M17_PACKET_CHUNK_BYTES + 2];
	int corrections = decode(frame, PACKET_INPUT_BITS, PACKET_SYNC, NULL, input);
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

int m17_stream_encode(const m17_stream_frame_t *stream, uint8_t frame[M17_FRAME_BYTES])
{
	if (stream->lich_index >= 6)
		return -1;
	uint8_t lich[M17_LICH_BYTES + 1];
	memcpy(lich, stream->lich, M17_LICH_BYTES);
	lich[M17_LICH_BYTES] = (uint8_t)(stream->lich_index << 5);
	uint8_t coded_lich[ENCODED_LICH_BYTES];
	for (unsigned i = 0; i < ENCODED_LICH_BYTES / 3u; i++) {
		unsigned bit = i * 12u;
		uint16_t word = 0;
		for (unsigned j = 0; j < 12; j++)
			word = (uint16_t)((word << 1) | get_bit(lich, bit + j));
		uint32_t coded = fec_golay24_encode(word);
		coded_lich[3 * i] = (uint8_t)(coded >> 16);
		coded_lich[3 * i + 1] = (uint8_t)(coded >> 8);
		coded_lich[3 * i + 2] = (uint8_t)coded;
	}
	uint8_t input[2 + M17_STREAM_BYTES + 1] = { 0 };
	input[0] = (uint8_t)(stream->number >> 8);
	input[1] = (uint8_t)stream->number;
	memcpy(input + 2, stream->data, M17_STREAM_BYTES);
	encode(input, STREAM_INPUT_BITS, STREAM_SYNC, coded_lich, frame);
	return 0;
}

static uint8_t decode_lich(const uint8_t *coded, uint8_t *out)
{
	uint8_t lich[M17_LICH_BYTES + 1] = { 0 };
	for (unsigned i = 0; i < ENCODED_LICH_BYTES / 3u; i++) {
		uint32_t word = ((uint32_t)coded[3 * i] << 16) |
				((uint32_t)coded[3 * i + 1] << 8) | coded[3 * i + 2];
		uint16_t data;
		if (fec_golay24_decode(word, &data) < 0)
			return M17_LICH_INVALID;
		for (unsigned j = 0; j < 12; j++) {
			unsigned bit = i * 12u + j;
			lich[bit / 8u] |= (uint8_t)(((data >> (11u - j)) & 1u) << (7u - bit % 8u));
		}
	}
	uint8_t index = lich[M17_LICH_BYTES] >> 5;
	if (index >= 6)
		return M17_LICH_INVALID;
	memcpy(out, lich, M17_LICH_BYTES);
	return index;
}

int m17_stream_decode(const uint8_t frame[M17_FRAME_BYTES], m17_stream_frame_t *stream)
{
	uint8_t coded_lich[ENCODED_LICH_BYTES];
	uint8_t input[2 + M17_STREAM_BYTES + 1];
	int corrections = decode(frame, STREAM_INPUT_BITS, STREAM_SYNC, coded_lich, input);
	if (corrections < 0)
		return -1;
	stream->number = (uint16_t)((input[0] << 8) | input[1]);
	memcpy(stream->data, input + 2, M17_STREAM_BYTES);
	stream->lich_index = decode_lich(coded_lich, stream->lich);
	return corrections;
}

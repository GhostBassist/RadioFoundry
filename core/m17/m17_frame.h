// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors

#ifndef M17_FRAME_H
#define M17_FRAME_H

#include <stddef.h>
#include <stdint.h>

#define M17_FRAME_BYTES 48
#define M17_LINK_DATA_BYTES 28
#define M17_PACKET_CHUNK_BYTES 25
#define M17_STREAM_BYTES 16
#define M17_LICH_BYTES 5
#define M17_STREAM_FINAL 0x8000u
#define M17_LICH_INVALID 0xFFu

typedef struct {
	uint16_t number;	// high bit signals the final frame
	uint8_t data[M17_STREAM_BYTES];
	uint8_t lich[M17_LICH_BYTES];
	uint8_t lich_index;
} m17_stream_frame_t;

typedef struct {
	uint8_t data[M17_PACKET_CHUNK_BYTES];
	uint8_t counter;	// chunk index, or valid byte count for the final chunk
	uint8_t final;
} m17_packet_chunk_t;

uint16_t m17_crc(const uint8_t *data, size_t len);

// A complete aligned frame in air order, including the sync burst.
void m17_link_encode(const uint8_t data[M17_LINK_DATA_BYTES], uint8_t frame[M17_FRAME_BYTES]);
int m17_link_decode(const uint8_t frame[M17_FRAME_BYTES], uint8_t data[M17_LINK_DATA_BYTES]);

// Return -1 for invalid input; decode otherwise returns the FEC correction count.
// Packet integrity must be checked over the reassembled payload and its CRC.
int m17_packet_encode(const m17_packet_chunk_t *chunk, uint8_t frame[M17_FRAME_BYTES]);
int m17_packet_decode(const uint8_t frame[M17_FRAME_BYTES], m17_packet_chunk_t *chunk);

int m17_stream_encode(const m17_stream_frame_t *stream, uint8_t frame[M17_FRAME_BYTES]);
// An uncorrectable LICH is flagged separately so speech can still be delivered.
int m17_stream_decode(const uint8_t frame[M17_FRAME_BYTES], m17_stream_frame_t *stream);

#endif

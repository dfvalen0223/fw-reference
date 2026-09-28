// Parallel WS2812 bit-stream encoder: one data line ("lane") per letter,
// all driven at once from a single DMA buffer on the ESP32-S3 LCD bus.
// Pure C (no ESP-IDF), unit-tested on the host: see firmware/test/.
//
// The bus shifts out one word per slot at 3.2 MHz (312.5 ns). Each WS2812
// bit takes 4 slots (1.25 us): '0' = 1000 (312 ns high), '1' = 1100
// (625 ns high). Bit n of every word drives lane n. Lanes shorter than the
// longest one are held low once their LEDs are sent.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define LANES_SLOT_HZ 3200000u
#define LANES_SLOTS_PER_BIT 4u
#define LANES_MAX 16

// Bytes needed for `max_leds` LEDs (3 bytes each) on an 8- or 16-bit bus.
size_t lanes_buffer_bytes(size_t max_leds, int bus_bits);

// lane_bytes[k] points to lane k's LED bytes, already in wire order (e.g.
// G,R,B), lane_leds[k] LEDs long. lane_count <= bus_bits.
void lanes_encode(void *buf, int bus_bits, size_t lane_count, const uint8_t *const lane_bytes[],
                  const size_t lane_leds[], size_t max_leds);

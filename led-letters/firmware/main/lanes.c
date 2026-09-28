#include "lanes.h"

size_t lanes_buffer_bytes(size_t max_leds, int bus_bits)
{
    return max_leds * 24u * LANES_SLOTS_PER_BIT * (size_t)(bus_bits / 8);
}

static void put(void *buf, int bus_bits, size_t slot, uint16_t v)
{
    if (bus_bits == 16)
        ((uint16_t *)buf)[slot] = v;
    else
        ((uint8_t *)buf)[slot] = (uint8_t)v;
}

void lanes_encode(void *buf, int bus_bits, size_t lane_count, const uint8_t *const lane_bytes[],
                  const size_t lane_leds[], size_t max_leds)
{
    size_t slot = 0;
    for (size_t led = 0; led < max_leds; led++) {
        uint16_t active = 0;
        for (size_t k = 0; k < lane_count; k++)
            if (led < lane_leds[k]) active |= (uint16_t)(1u << k);

        for (size_t byte = 0; byte < 3; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                uint16_t ones = 0;
                for (size_t k = 0; k < lane_count; k++)
                    if ((active >> k & 1u) && (lane_bytes[k][led * 3 + byte] >> bit & 1u))
                        ones |= (uint16_t)(1u << k);
                put(buf, bus_bits, slot++, active);  // every bit starts high
                put(buf, bus_bits, slot++, ones);    // stays high for a '1'
                put(buf, bus_bits, slot++, 0);
                put(buf, bus_bits, slot++, 0);       // ends low: bus idles low
            }
        }
    }
}

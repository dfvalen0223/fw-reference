#pragma once

#include <cstdint>
#include <cstddef>

namespace util {

/// COBS encode: returns encoded size, or 0 if output buffer too small.
/// Output always gets a 0x00 terminator appended.
std::size_t cobs_encode(const uint8_t* input, std::size_t input_len,
                        uint8_t* output, std::size_t output_len);

/// COBS decode: returns decoded size, or 0 if input is invalid.
/// Expects input to have a 0x00 terminator.
std::size_t cobs_decode(const uint8_t* input, std::size_t input_len,
                        uint8_t* output, std::size_t output_len);

}  // namespace util

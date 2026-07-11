#include "util/cobs.hpp"

namespace util {

std::size_t cobs_encode(const uint8_t* input, std::size_t input_len,
                        uint8_t* output, std::size_t output_len) {
    if (output_len < (input_len + (input_len / 254) + 2)) {
        return 0;
    }

    std::size_t write_idx = 0;
    std::size_t code_idx = 0;
    uint8_t code = 1;

    output[write_idx++] = 0;

    for (std::size_t i = 0; i < input_len; i++) {
        if (input[i] == 0) {
            output[code_idx] = code;
            code_idx = write_idx;
            output[write_idx++] = 0;
            code = 1;
        } else {
            output[write_idx++] = input[i];
            code++;
            if (code == 0xFF) {
                output[code_idx] = 0xFF;
                code_idx = write_idx;
                output[write_idx++] = 0;
                code = 1;
            }
        }
    }

    output[code_idx] = code;
    output[write_idx++] = 0;

    return write_idx;
}

std::size_t cobs_decode(const uint8_t* input, std::size_t input_len,
                        uint8_t* output, std::size_t output_len) {
    // A valid COBS frame with a delimiter must be at least 2 bytes long (Overhead + 0x00)
    if (input_len < 2 || input[input_len - 1] != 0) return 0;

    std::size_t read_idx = 0;
    std::size_t write_idx = 0;

    // The last byte is the delimiter 0x00, it is not processed in the loop
    while (read_idx < input_len - 1) {
        uint8_t code = input[read_idx++];
        if (code == 0) return 0; // Protocol error: Unexpected zero within the frame

        for (uint8_t i = 1; i < code; i++) {
            if (read_idx >= input_len - 1) return 0;

            uint8_t byte = input[read_idx++];
            if (byte == 0) return 0;

            if (write_idx >= output_len) return 0; // Output overflow protection
            output[write_idx++] = byte;
        }

        if (code < 0xFF && read_idx < input_len - 1) {
            if (write_idx >= output_len) return 0;
            output[write_idx++] = 0;
        }
    }

    return write_idx;
}

}  // namespace util

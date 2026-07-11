#pragma once

#include <cstdint>
#include <cstddef>

namespace util {

struct CircularBuffer {
    static constexpr std::size_t SIZE = 256;
    uint8_t data[SIZE];
    std::size_t head = 0;
    std::size_t tail = 0;

    bool push(uint8_t byte) {
        std::size_t next = (head + 1) % SIZE;
        if (next == tail) return false;
        data[head] = byte;
        head = next;
        return true;
    }

    bool pop(uint8_t& byte) {
        if (head == tail) return false;
        byte = data[tail];
        tail = (tail + 1) % SIZE;
        return true;
    }
};

}

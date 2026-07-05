#!/usr/bin/env python3
import random
import csv


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc


def main():
    # Verify canonical vector: "123456789" -> 0x29B1
    assert crc16_ccitt(b"123456789") == 0x29B1, "Canonical vector failed!"

    # Generate 100 random payloads and write CSV
    with open("test_vectors.csv", "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["payload_hex", "crc_hex"])
        for _ in range(100):
            length = random.randint(1, 64)
            payload = bytes(random.getrandbits(8) for _ in range(length))
            crc = crc16_ccitt(payload)
            writer.writerow([payload.hex(), f"{crc:04X}"])

    print("Generated test_vectors.csv with 100 CRC-16 CCITT vectors")


if __name__ == "__main__":
    main()

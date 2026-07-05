#!/usr/bin/env python3
"""
Stub for BMP280 pressure/temperature sensor.
Generates simulated telemetry frames matching the format expected
by the C++ TelemetryProtocol driver.

Frame format: [SOF=0xAA][LEN][PAYLOAD][CRC16]
Payload: [PRESSURE_HI][PRESSURE_LO][TEMP_HI][TEMP_LO]
"""

import random
import struct

SOF = 0xAA

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

def generate_bmp280_frame(pressure_pa: float, temp_c: float) -> bytes:
    # Simulate 24-bit pressure + 16-bit temperature
    press_int = int(pressure_pa / 100) & 0xFFFFFF  # hPa, 24-bit
    temp_int = int(temp_c * 100) & 0xFFFF           # centidegrees, 16-bit

    payload = struct.pack(">IH", press_int, temp_int)  # 5 bytes
    crc_data = bytes([len(payload)]) + payload
    crc = crc16_ccitt(crc_data)

    frame = struct.pack(">BB", SOF, len(payload)) + payload + struct.pack(">H", crc)
    return frame

def main():
    # Generate 10 sample frames
    for i in range(10):
        press = random.uniform(95000, 105000)  # 950-1050 hPa
        temp = random.uniform(-10, 45)         # -10 to 45 C
        frame = generate_bmp280_frame(press, temp)
        print(f"Frame {i}: {frame.hex()} ({press/100:.1f} hPa, {temp:.1f} C)")

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
BMP280 pressure/temperature sensor stub.
Generates simulated telemetry frames as the C++ driver would expect them.

Frame format: [SOF=0xAA][LEN=0x04][PAYLOAD (4 bytes)][CRC16 (2 bytes)]
Payload: [PRESSURE_HI][PRESSURE_LO][TEMP_HI][TEMP_LO] (All Big-Endian)
"""

import random
import struct

SOF = 0xAA


def crc16_ccitt(data: bytes) -> int:
    """Calculates canonical CRC-16/CCITT-FALSE (Init=0xFFFF, Poly=0x1021)."""
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
    # Convert to fixed 16-bit integers for the payload
    press_hpa = int(pressure_pa / 100) & 0xFFFF # hPa fits perfectly in a uint16_t
    temp_int = int(temp_c * 100) & 0xFFFF # Hundreds of degrees

    # >HH ensures (dos enteros de 16 bits, Big-Endian) 2 bytes for pressure and 2 bytes for temperature (Total = 4 bytes)
    # https://docs.python.org/3/library/struct.html
    payload = struct.pack(">HH", press_hpa, temp_int)

    # CRC calculation includes the LEN and the PAYLOAD (Aligned with your architecture)
    crc_data = bytes([len(payload)]) + payload
    crc = crc16_ccitt(crc_data)

    # Final frame construction: SOF (1B) + LEN (1B) + PAYLOAD (4B) + CRC (2B) = 8 Bytes total 
    # https://docs.python.org/3/library/struct.html
    frame = struct.pack(">BB", SOF, len(payload)) + payload + struct.pack(">H", crc) 
    return frame


def main(): 
    print("Generating 10 BMP280 simulation telemetry frames...\n") 
    for i in range(10): 
        press = random.uniform(95000, 105000) 
        temp = random.uniform(-10, 45) 
        frame = generate_bmp280_frame(press, temp) 

        print(f"Frame {i}: {frame.hex().upper()}") 
        print(f" -> Data: {press/100:.1f} hPa, {temp:.1f} °C") 
        print(f" -> Structure: SOF={frame[0]:02X} | LEN={frame[1]:02X} | Payload={frame[2:6].hex().upper()} | CRC={frame[6:].hex().upper()}\n")


if __name__ == "__main__": 
main()

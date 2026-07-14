#!/usr/bin/env python3
"""
HTerm-style serial terminal: hex + ASCII side by side, with timestamps.

Usage:
    python3 tools/serial_hexterm.py /dev/tty.usbmodem21302 115200

RX is printed in rows of 16 bytes:
    12.345  AA BB 06 F9 01 02 03 04                          ........
A row is flushed when full or after IDLE_FLUSH_S without new bytes
(so protocol bursts group naturally into rows).

TX: type hex bytes ("AA BB 01") or quoted ASCII ("hello") + Enter.
Ctrl-C to exit.
"""

import sys
import time
import threading

import serial

IDLE_FLUSH_S = 0.05
ROW_WIDTH = 16


def printable(b: int) -> str:
    return chr(b) if 32 <= b <= 126 else "."


class RxPrinter:
    def __init__(self, t0: float):
        self.t0 = t0
        self.row: list[int] = []
        self.row_ts = 0.0
        self.last_rx = 0.0
        self.lock = threading.Lock()

    def flush(self):
        if not self.row:
            return
        hex_part = " ".join(f"{b:02X}" for b in self.row)
        ascii_part = "".join(printable(b) for b in self.row)
        pad = " " * (ROW_WIDTH * 3 - len(hex_part))
        print(f"\r{self.row_ts:9.3f}  {hex_part}{pad}  {ascii_part}")
        self.row = []

    def feed(self, data: bytes):
        with self.lock:
            now = time.monotonic()
            for b in data:
                if not self.row:
                    self.row_ts = now - self.t0
                self.row.append(b)
                if len(self.row) >= ROW_WIDTH:
                    self.flush()
            self.last_rx = now

    def idle_tick(self):
        with self.lock:
            if self.row and (time.monotonic() - self.last_rx) > IDLE_FLUSH_S:
                self.flush()


def parse_tx(line: str) -> bytes:
    line = line.strip()
    if not line:
        return b""
    if line.startswith('"') and line.endswith('"') and len(line) >= 2:
        return line[1:-1].encode()
    try:
        return bytes(int(tok, 16) for tok in line.replace(",", " ").split())
    except ValueError:
        print(f'!! not hex — use "quotes" for ASCII: {line}')
        return b""


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    ser = serial.Serial(port, baud, timeout=0.02)
    print(f"-- {port} @ {baud} 8N1 — hex TX: 'AA BB 01', ASCII TX: \"hi\", Ctrl-C exits --")

    rx = RxPrinter(time.monotonic())

    def reader():
        while True:
            data = ser.read(256)
            if data:
                rx.feed(data)
            rx.idle_tick()

    threading.Thread(target=reader, daemon=True).start()

    try:
        while True:
            out = parse_tx(input())
            if out:
                ser.write(out)
                print(f"     TX>  {' '.join(f'{b:02X}' for b in out)}")
    except (KeyboardInterrupt, EOFError):
        print("\n-- bye --")
    finally:
        ser.close()


if __name__ == "__main__":
    main()

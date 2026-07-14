# Logic Analyzer Captures

Signal-level evidence of the RS-485 protocol running between the LPC1768
(96 MHz, FreeRTOS) and an RP2040 HIL responder, at 115200 baud 8N1.

## Files

- `rs485_hil_protocol.sal` — Saleae Logic 2 capture (open with
  [Logic 2](https://www.saleae.com/downloads/), analyzers included)
- `rs485_hil_protocol_decoded.csv` — decoded byte stream (hex) exported
  from the Async Serial analyzers

## Channel map

| Channel | Signal |
|---------|--------|
| CH7 | LPC1768 UART2 TX (P0.10) — DATA frames + ACKs |
| CH5 | RP2040 UART1 TX (GP4) — ACKs + echoed DATA frames |
| CH3 | LPC1768 DE (P0.9) — RS-485 driver-enable |
| CH1 | RP2040 DE (GP6) — RS-485 driver-enable |

## What one protocol round looks like (t ≈ 0.92 s in the capture)

```
time        who   DE        frame on the wire
--------------------------------------------------------------------
0.9215 s    LPC   DE=1 --> DATA seq=68 payload [AA BB 4A 4B] CRC ok
0.9224 s    pico  DE=1 --> ACK  seq=68                       CRC ok
0.9230 s    pico  DE=1 --> DATA seq=68 payload [AA BB 4A 4B] CRC ok (echo)
0.9240 s    LPC   DE=1 --> ACK  seq=68                       CRC ok
0.9246 s    idle          bus free until the next round (+1 s)
```

Wire format per frame: `[SOF=0x55][COBS(TYPE,SEQ,PAYLOAD,CRC16-BE)][0x00]`.

Points to note:

1. **Stop-and-wait handshake** — four frames per round, all CRC-valid;
   the sequence number only advances after a successful ACK.
2. **Half-duplex arbitration** — the DE pulses are contiguous and never
   overlap: the LPC drops DE at 0.9224 s exactly as the pico raises its
   own. On a real RS-485 differential bus this is what prevents two
   drivers from fighting.
3. **Clean driver release** — each DE falls only after the last stop
   bit (TX drained before direction switch).
4. Measured bit time 8.68 µs → 115385 baud, +0.16 % of nominal
   (integer divisor DL=13 from PCLK = 24 MHz).

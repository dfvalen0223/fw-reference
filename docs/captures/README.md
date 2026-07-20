# Logic Analyzer Captures

Signal-level evidence of the RS-485 protocol running between the LPC1768 (96 MHz, FreeRTOS) and an RP2040 HIL responder, at 115200 baud 8N1.

## Files

- `rs485_hil_protocol.sal` — Saleae Logic 2 capture (open with [Logic 2](https://www.saleae.com/downloads/), analyzers included)
- `rs485_hil_protocol_decoded.csv` — decoded byte stream (hex) exported from the Async Serial analyzers

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

1. **Stop-and-wait handshake** — four frames per round, all CRC-valid; the sequence number only advances after a successful ACK.
2. **Half-duplex arbitration** — the DE pulses are contiguous and never overlap: the LPC drops DE at 0.9224 s exactly as the pico raises its own. On a real RS-485 differential bus this is what prevents two
   drivers from fighting.
3. **Clean driver release** — each DE falls only after the last stop bit (TX drained before direction switch).
4. Measured bit time 8.68 µs → 115385 baud, +0.16 % of nominal (integer divisor DL=13 from PCLK = 24 MHz).

## CAN two-node bus: LPC1768 <-> RP2040-Zero, 125 kbps

Signal-level evidence of a real, physical two-node CAN bus (not chip internal loopback) working after fixing four stacked hardware faults found over the course of debugging this: a defective transceiver on the original module, an SI/SO swap in the RP2040's SPI wiring, a CANH/CANL polarity swap between the two modules, and a module connector (J1) that routes the 120R termination resistor in *series* with the signal path instead of across CANH/CANL — using it as the main bus connector desyncs reception without fully blocking it.

### Files

- `can_two_node_success.sal` — Saleae Logic 2 capture, both nodes’ SPI buses in one 15s window (8 channels)
- `can_two_node_success_decoded.csv` — decoded SPI byte stream

### Channel map

| Channel | Signal |
|---------|--------|
| CH0 | LPC1768 SPI CS (P0.23) |
| CH1 | LPC1768 SPI SO (P0.24, chip -> LPC) |
| CH2 | LPC1768 SPI SI (P0.25, LPC -> chip) |
| CH3 | LPC1768 SPI SCK (P0.26) |
| CH4 | RP2040 SPI CS (GP5) |
| CH5 | RP2040 SPI SO (GP4, chip -> RP2040) |
| CH6 | RP2040 SPI SI (GP3, RP2040 -> chip) |
| CH7 | RP2040 SPI SCK (GP2) |

### How to read it

Each node's own SPI bus shows it talking to *its own* MCP2515 chip — there's no direct CANH/CANL trace in this capture, only what each node's chip reports back over SPI when asked "did a real frame from the other node land in my receive buffer?" (SPI instruction `0x90`, READ RXB0).

**LPC analyzer track — 13/15 successful reads:**
Every ~1s the LPC sends its own frame (ID 0x200), then polls its chip. 13 out of 15 rounds, the poll returns a real frame with ID 0x100 (the RP2040's ID) sitting in the receive buffer — proof the RP2040's frame physically reached the LPC's chip and got accepted.

**RP2040 analyzer track — every poll returns fresh data:**
The RP2040 polls much faster (every 5ms, ~200x/round). In this capture, *every single poll* decoded a valid ID 0x200 frame (the LPC's ID) — but the meaningful proof isn't the count, it's the **payload byte**: it's the LPC's own send counter, and across the 15s window it climbs cleanly from 0x8D to 0x99 (141 to 153), one step per second. A stale, never-refreshed buffer would show the same number forever; seeing it climb in lockstep with the LPC's send rate is direct proof of live, repeated, successful reception — not a fluke or a leftover value.

To verify the ID field yourself on the RP2040 track: decode `SIDH=0x40, SIDL=0x00` from any of its READ RXB0 (`0x90`) responses -> `ID = (SIDH << 3) | (SIDL >> 5) = 0x200`, the LPC's own ID. Same math the LPC side uses to confirm `0x100` (see "How to read it” above).

### Exact timestamps (this capture, for screenshots)

LPC track — successful reads of the RP2040's frame (13 total):
0.796s, 1.823s, 2.999s, 4.180s, 5.355s, 6.893s, 7.910s, 8.926s, 10.071s, 11.252s, 12.428s, 14.007s, 15.023s

RP2040 track — each time the counter byte advances (13 total):
0.798s(0x8D/141), 1.819s(0x8E/142), 2.833s(0x8F/143), 3.849s(0x90/144), 4.864s(0x91/145), 5.879s(0x92/146), 6.894s(0x93/147), 7.915s(0x94/148), 8.930s(0x95/149), 9.945s(0x96/150), 10.960s(0x97/151), 11.975s(0x98/152), 12.995s(0x99/153)

### Send-to-poll latency (LPC side)

The LPC track timestamps above are when the *LPC polled and found* the RP2040's frame — not when the RP2040 actually sent it. The RP2040's real `LOAD_TXB0`+`RTS` send happens earlier in its own round; the gap to the LPC's next poll depends on where in the LPC’s 5ms polling window the frame landed:

| RP2040 sent (real) | LPC found it (polled) | gap |
|---|---|---|
| 0.638890s (D0=0x8D) | 0.796200s | ~157ms |
| 4.174665s (D0=0x90) | 4.179607s | ~5ms |

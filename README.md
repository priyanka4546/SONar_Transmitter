# Adaptive SDR Sonar Transmitter Payload

**SIH260580** : a software-defined sonar transmitter that adapts its transmission frequency, pulse duration, and amplitude to simulated environmental conditions (depth, turbidity, salinity), built on an STM32F411 Blackpill.

## Overview

This firmware generates a Hann-windowed Linear Frequency Modulated (LFM) chirp and streams it to an MCP4921 DAC. Rather than fixed transmission parameters, three potentiometers simulate environmental sensors, and an onboard propagation model - including a real sound-speed-in-seawater calculation - computes the frequency, pulse duration, and amplitude best suited to those conditions in real time.

**Current status:** functional prototype operating in the 2–18 kHz range. This is *not yet* the final 100–500 kHz sonar transmitter the problem statement targets.

## Hardware

| Component | Notes |
|---|---|
| STM32F411CEU6 "Blackpill" | The specific board used is a clone/non-genuine part |
| MCP4921 | 12-bit SPI DAC, unbuffered mode |
| LM358 | Op-amp stage after the DAC. Not rail-to-rail, limited slew rate  |
| 3× linear-taper potentiometer | Simulate depth / turbidity / salinity sensors |
| ST-Link V2 (clone) | Flashing via SWD |
| ESP32-C3 Super Mini | Companion board - reads the analog output via its own ADC for live visualization, since no oscilloscope is available yet |

## Pinout

| Signal | Pin |
|---|---|
| MCP4921 CS | PB12 |
| MCP4921 SCK | PB13 (SPI2_SCK) |
| MCP4921 SDI | PB15 (SPI2_MOSI) |
| MCP4921 LDAC | GND (tied low in hardware, not firmware-controlled) |
| Pot 1 - depth | PA1 (ADC1_IN1) |
| Pot 2 - turbidity | PA2 (ADC1_IN2) |
| Pot 3 - salinity | PA3 (ADC1_IN3) |
| ST-Link SWDIO | PA13 |
| ST-Link SWCLK | PA14 |

## Architecture & Key Decisions

A few choices in this codebase look non-obvious out of context. They're documented here because each one cost real debugging time to find.

**SPI2, not SPI1.** Isolated bisection testing found that initializing SPI1 at all - even a bare `SPI.begin()` with no transaction and no actual transfer - breaks USB CDC on this specific board. SPI2 on different pins (PB12/13/14/15) does not have this problem. This appears to be board-specific, not a general STM32F411 issue.

**ST-Link/SWD, not USB DFU.** DFU flashing was unreliable on this clone board (intermittent enumeration failures, consistent with documented crystal-calibration timing sensitivity on cheap Blackpill clones). SWD via a cheap ST-Link sidesteps the USB bootloader entirely and has been reliable since switching.

**LDAC tied to GND in hardware**, not toggled by firmware. MCP4921 latches its output on the rising edge of CS when LDAC is held low, so no separate LDAC control is needed - this removes GPIO toggles from the sample-rate-critical ISR.

**Direct BSRR register writes for CS**, not `digitalWrite()`. At a 200 kHz sample rate, `digitalWrite()`'s pin-lookup overhead was a meaningful fraction of the available 5 µs budget per sample. `GPIOB->BSRR` writes are a single instruction.

**`SPI.beginTransaction()` called once in `setup()`**, not per sample. This DAC is the only device on the bus, so the per-transfer overhead of repeatedly opening/closing a transaction was unnecessary and was cut from the ISR.

**Timer paused during buffer regeneration.** `sampleTimer->pause()` / `resume()` brackets the call to `generateChirp()` in `loop()`. Without this, the ISR could read `chirpBuffer` while it was mid-rewrite, producing a torn, glitched pulse.

**2–18 kHz operating range, not 100–500 kHz.** This is a deliberate prototyping choice, not a bug: it keeps the signal within reach of `analogRead()`-based measurement on the ESP32-C3 companion board (no dedicated ADC hardware needed to verify it's working), and it's audible, which makes bring-up easier. It is a known, explicit gap against the problem statement's target frequency range - see below.

## How the Adaptive Algorithm Works

Each potentiometer simulates an environmental sensor rather than directly setting a wave parameter:

1. **Depth** (0–100 m), **turbidity** (0–100%), and **salinity** (0–40 PSU) are read from the three pots.
2. **Speed of sound** is computed from temperature (currently a fixed assumed value), salinity, and depth using the Mackenzie (1981) approximation for sound speed in seawater.
3. A **propagation difficulty score** (0–1) is computed as a weighted combination: turbidity dominates (0.55), depth is secondary (0.35), and salinity contributes as its *deviation from 35 PSU* - the salinity of average seawater - rather than a straight linear scale, since both unusually fresh and unusually saline water are atypical conditions, not just "more salt is worse."
4. That difficulty score drives three outputs:
   - **Frequency** interpolates between an easy-conditions band (14–18 kHz, best resolution) and a hard-conditions band (2–6 kHz, best penetration).
   - **Duration** interpolates between 0.5 ms (easy) and 2.5 ms (hard) - more total transmitted energy for harder conditions.
   - **Amplitude** interpolates between 0.55 and 0.90 - more signal power to overcome attenuation.

This directly implements the problem statement's "Environmental Sensor Interface & Adaptation Logic" requirement.

## Build & Flash

**Arduino IDE settings:**
- Board: `Generic STM32F4 series`
- Board part number: `BlackPill F411CE`
- USB support: `CDC (generic Serial supersede U(S)ART)`
- Upload method: `STM32CubeProgrammer (SWD)`

**Wiring:** connect an ST-Link V2 to SWDIO (PA13), SWCLK (PA14), and GND. Leave the Blackpill's own USB cable connected too - it's needed separately for the USB CDC Serial connection, since SWD and USB are independent interfaces. Then flash normally via the IDE's Upload button.

## Testing / Verification

No oscilloscope or spectrum analyzer is available yet for direct signal validation, so an ESP32-C3 Super Mini is used as a stand-in: its own ADC samples the op-amp's output, and results stream to Arduino IDE's Serial Plotter for a live graph.

**Important caveat:** this setup can only reliably show the millisecond-scale amplitude *envelope* of each pulse (rise and fall shape, tracking duration and amplitude), not the actual frequency content, which oscillates far faster than simple ADC polling can resolve. It is not a substitute for a real oscilloscope, only a rough sanity check that pulses are firing with plausible timing and amplitude.

**Setup notes:**
- ESP32-C3 requires **Tools → USB CDC On Boot → Enabled** for Serial to work at all over its native USB connection - without it, Serial silently routes to unused UART pins instead.
- A shared ground between the STM32 board and the ESP32-C3 is required for any ADC reading to be meaningful.
- Depending on the op-amp's actual output swing, a voltage divider may be needed before the ESP32-C3's ADC input (max 3.3V, not 5V-tolerant) - verify the op-amp's real output range with a multimeter before connecting it directly.

## Known Limitations & Next Steps

- **Not yet DMA-driven.** The problem statement explicitly requires DMA + hardware timers "without stalling the CPU." The current implementation is CPU/interrupt-driven (a timer ISR calling `SPI.transfer16()` per sample), which works but doesn't meet this requirement as written. A DMA-based rewrite (timer-paced DMA into `SPI1->DR`, with the peripheral hardware - not firmware - generating chip-select framing) is the outstanding piece of work here.
- **2–18 kHz, not 100–500 kHz.** The current range is a deliberate prototyping choice (see above), not the problem statement's target sonar frequency range. Moving to the real range needs the DMA rewrite above, since CPU-driven SPI writes were found not to coexist with USB CDC at the sample rates the real frequency range would require.
- **LM358 has real limitations at higher frequencies.** Its slew rate (~0.3 V/µs) puts it right at its own performance limit near the top of even the current reduced frequency range - a faster op-amp (e.g. TL072) would remove this ceiling, though it needs a higher supply voltage (5V) to have adequate headroom on both rails.
- **No real oscilloscope/spectrum analyzer validation yet.** The problem statement's judging criteria call for a clean FFT spectrogram at the judging table - this hasn't been directly verified, only approximated via the ESP32-C3 envelope check described above.
- **Water temperature is currently a fixed assumed constant**, not sensor-driven - a fourth sensor input would complete the environmental model.

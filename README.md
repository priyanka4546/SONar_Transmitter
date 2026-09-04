# Adaptive SDR Sonar Transmitter Payload 🌊

**Smart India Hackathon 2026** | **Team:** team blub blub | **PS ID:** 26058

A low-power, real-time adaptive software-defined sonar transmitter payload for Autonomous Underwater Vehicles (AUVs). It generates mathematically precise, low-distortion acoustic chirps that dynamically adapt to environmental conditions (depth, turbidity, salinity) without requiring a firmware recompile

## Hardware Requirements
* **MCU:** STM32F411CEU6 (Black Pill)
* **DAC:** MCP4921 (12-bit SPI)
* **Filter/Buffer:** TL072 dual op-amp
* **Power:** LM2596 buck converter[s
* **Transducer:** Piezoelectric (40-100 kHz)

## Pin Configuration
| Component | STM32 Pin | Function |
| :--- | :--- | :--- |
| **Depth Sensor** (Pot 1) | `PA1` | ADC1_IN1 |
| **Turbidity Sensor** (Pot 2) | `PA2` | ADC1_IN2 |
| **Salinity Sensor** (Pot 3) | `PA3` | ADC1_IN3 |
| **MCP4921 CS** | `PB12` | SPI2_NSS |
| **MCP4921 SCK** | `PB13` | SPI2_SCK |
| **MCP4921 SDI** | `PB15` | SPI2_MOSI |

## Real-Time Processing Pipeline
1. **Sense & Read:** The STM32 ADC samples live inputs to map target frequency, duration, and amplitude
2. **Synthesize:** A phase-accumulator computes the LFM chirp, applying digital envelope windowing (soft start/stop) to suppress acoustic sidelobes and protect against transducer kickback
3. **Stream:** Hardware timer interrupts push the sample buffer to the DAC over SPI, ensuring a steady, CPU-efficient sample rate
4. **Condition:** A TL072 2-stage active filter smooths the stepped DAC output into a clean analog wave
5. **Dual Output:** The signal is split to a physical piezo for genuine acoustic transmission and an FFT probe tap for validation

## Firmware Setup
1. Open the project in the **Arduino IDE** using the **STM32 core**
2. Select **Generic STM32F4 series** > **BlackPill F411CE** in the Boards menu.
3. Wire the components according to the pin table above.
4. Upload via **ST-Link** using STM32CubeProgrammer (SWD).

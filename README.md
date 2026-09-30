# Low-Power Adaptive Sonar Transmitter Payload for AUVs

**Smart India Hackathon 2026 — Problem Statement ID: 26058**  
**Theme:** Robotics and Drones  
**Category:** Hardware  
**Team:** team blub blub  
**Team ID:** 182517

## 📌 Project Overview

This project is a prototype for a **low-power, real-time adaptive sonar payload for Autonomous Underwater Vehicles (AUVs)**.

The system uses an **nRF52840-DK** as the main embedded controller and collects underwater acoustic and power-related telemetry. The current prototype measures the **echo time-of-flight (ToF)** from a JSN-SR04T ultrasonic sensor, estimates underwater sound speed from temperature, calculates target range, and monitors bus voltage/current/power using an INA219.

The project is based on the Smart India Hackathon 2026 problem statement:

> **“Development of a Low power, Real-time Adaptive Software-Defined Sonar Transmitter Payload for Autonomous Underwater Vehicles (AUVs)”**

## 🎯 Objectives

- Measure ultrasonic echo time-of-flight in real time.
- Estimate underwater sound speed using water temperature.
- Calculate target range from acoustic ToF.
- Monitor electrical power parameters.
- Provide continuous telemetry through the Zephyr console.
- Develop a low-power embedded platform suitable for an AUV sonar payload.
- Provide a foundation for future adaptive sonar waveform generation and closed-loop environmental adaptation.

## 🧩 Hardware Used

| Component | Purpose |
|---|---|
| **Nordic Semiconductor nRF52840-DK v3.0.3** | Main embedded controller |
| **JSN-SR04T waterproof ultrasonic transducer** | Underwater ultrasonic sensing |
| **JSN-SR04T driver board** | Interface/driver for the ultrasonic transducer |
| **INA219 current/power sensor** | Bus voltage and power monitoring |
| **DS18B20 waterproof temperature sensor** | Water temperature measurement |
| **MP1584 buck converter** | DC-DC voltage regulation |
| **3S Li-ion/LiPo battery (11.1–12.6 V)** | Prototype power source |
| **Zero PCB / perfboard** | Hardware interconnection |

## 🔌 Current GPIO / Interface Mapping

The current Zephyr application uses:

| Signal | nRF52840 pin | Function |
|---|---:|---|
| TRIG | P0.13 | Trigger pulse for JSN-SR04T |
| ECHO | P0.14 | Echo input through a safe level/interface |
| I2C | `i2c0` | INA219 communication |
| INA219 address | `0x40` | Default INA219 I2C address |

> **Important:** Verify the actual nRF52840-DK header mapping and the JSN-SR04T electrical interface before connecting hardware. The ECHO signal must be within the nRF52840 GPIO voltage limits.

## ⚙️ Software

The firmware is written in **C using Zephyr RTOS**.

### Main software functions

1. Configure GPIO for the ultrasonic trigger and echo.
2. Generate a short trigger pulse.
3. Measure the duration of the ultrasonic ECHO pulse.
4. Calculate underwater sound speed from temperature.
5. Calculate distance from acoustic time-of-flight.
6. Read INA219 bus voltage over I2C.
7. Calculate/report power telemetry.
8. Print telemetry continuously through the Zephyr console.

## 📐 Working Principle

### 1. Ultrasonic Trigger

The nRF52840 generates a trigger pulse on **P0.13**.

The JSN-SR04T sends an ultrasonic pulse and produces an ECHO signal.

### 2. Echo Time-of-Flight

The firmware waits for ECHO to become HIGH and measures how long it remains HIGH.

The measured duration is stored as:

```text
ToF (µs)
```

A timeout of **60 ms** is currently used to prevent the firmware from waiting indefinitely.

### 3. Temperature-Based Sound Speed

The current code uses the following empirical relationship:

```text
c = 1449.2 + 4.6T - 0.055T²
```

where:

- `c` = estimated sound speed in m/s
- `T` = water temperature in °C

### 4. Range Calculation

For a round-trip acoustic measurement:

```text
Distance = (ToF × Sound Speed) / 2
```

The code converts the result to centimetres:

```text
distance_cm = (ToF × sound_speed) / 20000
```

### 5. Power Monitoring

The INA219 is accessed over I2C.

The firmware reads the bus-voltage register and calculates/report power telemetry.

The console output contains:

```text
Time(ms)
Temperature(°C)
Sound Speed(m/s)
ToF(µs)
Range(cm)
Bus Voltage(V)
Current(mA)
Power(mW)
```

## 🖥️ Example Telemetry Format

The firmware prints a table similar to:

```text
Time(ms)    Temp(C)    SoundSpd(m/s)    ToF(us)    Range(cm)    Bus(V)    Cur(mA)    Pwr(mW)
-----------------------------------------------------------------------------------------------
...
```

The telemetry loop runs approximately every **100 ms**, corresponding to a nominal **10 Hz** reporting rate.

## 🧠 Firmware Structure

```text
main()
│
├── Initialize GPIO0
│
├── Initialize I2C device
│
├── Configure TRIG and ECHO pins
│
├── Print telemetry header
│
└── Continuous loop
    │
    ├── Calculate sound speed
    │
    ├── Trigger JSN-SR04T
    │
    ├── Measure ECHO pulse width
    │
    ├── Calculate range
    │
    ├── Read INA219 telemetry
    │
    ├── Print telemetry
    │
    └── Wait 100 ms
```

## 🚀 Building the Zephyr Project

Make sure your Zephyr development environment is already installed and configured.

From the project directory, a typical Zephyr build command is:

```bash
west build -b nrf52840dk/nrf52840
```

To flash the board:

```bash
west flash
```

The exact board target can vary with the Zephyr version, so use the board name supported by your installed Zephyr version.

## 🔬 Current Prototype Status

### Implemented in the supplied firmware

- nRF52840 GPIO control
- JSN-SR04T trigger generation
- ECHO pulse-width measurement
- Temperature-based sound-speed calculation
- Acoustic range calculation
- INA219 I2C communication
- Continuous telemetry output
- 10 Hz telemetry loop

### Important implementation notes

The current source code contains a fixed temperature value:

```c
float water_temp = 24.5f;
```

Therefore, the **DS18B20 is not yet actually read by this particular source file**. The temperature value is currently being used as an environmental baseline.

Similarly, the current function uses a nominal current value rather than reading the INA219 current register:

```c
*current_ma = 28.0f;
```

Therefore, the current telemetry implementation should be considered a **prototype/demo implementation**, not a complete INA219 current measurement implementation.

The repository can be updated later when the DS18B20 and INA219 current-register handling are fully integrated.

## 🔮 Future Improvements

- Integrate real DS18B20 temperature readings.
- Read actual INA219 current and power registers.
- Add proper INA219 calibration for the selected shunt/configuration.
- Add adaptive waveform generation.
- Implement software-defined LFM/chirp generation.
- Add waveform parameter adaptation based on environmental telemetry.
- Add digital Hann envelope/windowing.
- Improve acoustic signal processing and filtering.
- Add more robust timeout and error handling.
- Add data logging for experimental analysis.
- Optimize power consumption for AUV operation.
- Add a closed-loop adaptive sonar processing pipeline.

## ⚠️ Potential Challenges

- Underwater acoustic propagation varies with temperature and environmental conditions.
- Ultrasonic sensor performance may differ from laboratory air measurements.
- Echo detection can be affected by noise and reflections.
- GPIO voltage compatibility must be checked carefully.
- Power consumption is important for battery-operated AUVs.
- Accurate current measurement requires proper INA219 configuration and calibration.
- Real-time waveform generation requires careful timing and resource management.

## 📚 Research Context

The project presentation identifies software-defined sonar on low-power embedded hardware, adaptive transmit waveform design, digital windowing, and frequency-dependent underwater sound absorption as relevant research areas.

The SIH project material references:

- Zhou et al., *Software-Defined Sonar for Unmanned Underwater System*, IET Radar, Sonar & Navigation, 2025.
- Naval Undersea Warfare Center, *Adaptive Transmit Waveform Design*, arXiv:2111.08746, 2021.
- F. J. Harris, *On the Use of Windows for Harmonic Analysis with the DFT*, IEEE, 1978.
- R. E. Francois and G. R. Garrison, *Sound Absorption Based on Ocean Measurements*, JASA, 1982.

## 👥 Team

**Team:** team blub blub  
**SIH Problem Statement:** 26058  
**SIH Team ID:** 182517


### Project Status

**Prototype / Development Stage**

This repository documents the current embedded prototype and can be expanded as the adaptive sonar transmitter, environmental sensing, and power-management features are integrated.

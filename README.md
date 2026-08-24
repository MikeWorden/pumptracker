# pumptracker

An acoustic edge monitoring sensor built to monitor my basement sump pump.   Uses a cheap 
 **ESP32-C5** RISC-V microcontroller and an **INMP441** digital I2S MEMS microphone.   The ESP32-C5 supports 2.4 and 5 GHz 
 
`pumptracker` continuously monitors ambient sound levels, isolates the acoustic signature of a running pump (such as my  basement sump pump), records cycle start times and run durations via NTP, and publishes structured event payloads over MQTT.  These MQTT messages  will be routed to HomeAssistant via the MQTT broker. 

---

## Features

- **Non-Invasive Monitoring:** Listens to pump cycle acoustics—no plumbing, current clamps, or fluid-contact sensors required.
- **ESP-IDF v5 Standard I2S Pipeline:** Phase-locked 24-bit PCM audio acquisition over a 32-bit standard Philips/MSB slot.
- **Dual-Band Connectivity:** Leverages ESP32-C5 Wi-Fi 6 support (2.4 GHz and 5 GHz).
- **Network Time Synchronization:** Automatically retrieves wall-clock epoch timestamps using NTP.
- **Debounced Cycle Detection:** Prevents false triggers from transient background noise (footsteps, HVAC, dropped objects).
- **MQTT Event Streaming:** Emits structured JSON events on start and stop, with Last Will and Testament (LWT) status tracking.

---

## Hardware Pinout

Connect the **INMP441** microphone module to the **ESP32-C5** as follows:

| INMP441 Pin | ESP32-C5 GPIO | Description |
| :--- | :--- | :--- |
| **VDD** | `3V3` | 3.3V DC Power (Do not use 5V) |
| **GND** | `GND` | Common Ground |
| **SD** | `GPIO 1` | Serial Data Out (DIN) |
| **WS** | `GPIO 2` | Word Select / Left-Right Clock (LRCK) |
| **SCK** | `GPIO 3` | Continuous Serial Clock (BCLK) |
| **L/R** | `GND` | Pull to GND for Left Channel Mono |

---

## Repository Structure

```text
pumptracker/
├── pumptracker.ino      # Main application logic, I2S DMA, and state machine
├── pumptracker.h        # Pin assignments, network credentials, and thresholds
├── 3dprint/		 # stl and 3mf files for a case for the processor and microphone
└── README.md

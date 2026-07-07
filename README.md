# VLC Digital Audio File Transfer System

## Project Overview
This project is an industrial-grade R&D prototype for transmitting digital files (like Audio) over a Visible Light Communication (VLC) channel. It utilizes a web browser for UI and BLE communication, and two ESP32 devices connected via a simplex optical link (Laser and Photodiode) acting as the transmission medium. 

The software architecture strictly adheres to a 4-layer model (Application, Transport, Data Link, and Physical) to guarantee bit-for-bit file integrity without relying on hardware flow control.

## Architecture Diagram
```
Browser (TX)
    ↓ (BLE - Nordic UART Service)
ESP32 TX (VLC_TX)
    ↓ (UART over Laser - Physical Layer)
    ↓ Visible Light 
    ↓ (Photodiode to UART)
ESP32 RX (VLC_RX)
    ↓ (BLE - Nordic UART Service)
Browser (RX)
```

## Communication Flow
1. **Selection:** User selects a file (e.g., audio) in the browser (TX).
2. **Packetization:** Browser chunks the file, framing it with headers, sequences, and a CRC16 checksum.
3. **Transmission:** Browser sends packets over BLE to the ESP32 TX.
4. **Optical Link:** ESP32 TX queues and transmits the raw packet bytes over UART via the laser.
5. **Reception:** ESP32 RX reads raw bytes from the photodiode via UART.
6. **Validation:** ESP32 RX parses the stream, validates the CRC16, and extracts valid packets.
7. **Forwarding:** Valid packets are sent over BLE to the browser (RX).
8. **Reconstruction:** Browser (RX) reassembles the file, validates the overall size and checksum, and presents it for playback or download.

## Packet Format
Every packet transmitted over the optical link is structured as follows:
* `SYNC1` (1 byte) - 0xAA
* `SYNC2` (1 byte) - 0x55
* `VERSION` (1 byte) - Protocol Version (0x01)
* `TYPE` (1 byte) - e.g., FILE_START (1), DATA (2), FILE_END (3)
* `FLAGS` (1 byte) - Bitmask for extended features
* `SEQUENCE` (2 bytes) - Big-Endian packet sequence number
* `LENGTH` (2 bytes) - Big-Endian payload length (N bytes)
* `PAYLOAD` (N bytes) - 0-240 bytes of data
* `CRC16` (2 bytes) - CCITT CRC16 covering VERSION through PAYLOAD

## BLE Services
* Uses the standard **Nordic UART Service (NUS)**:
  * Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
  * RX Characteristic (Browser writes to ESP32): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  * TX Characteristic (ESP32 notifies Browser): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
* ESP32 requests an MTU of 512 bytes.

## UART Configuration
* **Baud Rate:** 115200 (Configurable)
* **Configuration:** SERIAL_8N1
* **Inversion:** True (Inverted logic for laser safety and power savings)

## GPIO Connections
* **Transmitter (ESP32 TX):**
  * Laser connected to `GPIO2` (UART1 TX)
* **Receiver (ESP32 RX):**
  * Photodiode connected to `GPIO34` (UART1 RX)

## Libraries Required
* ESP32 core for Arduino
* `BLEDevice.h`, `BLEServer.h`, `BLEUtils.h`, `BLE2902.h` (Built into ESP32 core)

## Compilation Steps
1. Open `VLC_TX/VLC_TX.ino` in Arduino IDE or PlatformIO.
2. Compile and upload to the Transmitter ESP32.
3. Open `VLC_RX/VLC_RX.ino` in Arduino IDE or PlatformIO.
4. Compile and upload to the Receiver ESP32.

## Browser Requirements
* A modern browser supporting Web Bluetooth (e.g., Google Chrome, Microsoft Edge).
* Must be served over HTTPS (or `localhost` / `127.0.0.1` for development).

## Testing Procedure
1. Power up both ESP32s and align the laser with the photodiode.
2. Open `audio.html` in two separate browser tabs (or windows).
3. In Tab 1, select the `Transmitter (TX)` interface and connect to `VLC_TX_Pro`.
4. In Tab 2, select the `Receiver (RX)` interface and connect to `VLC_RX_Pro`.
5. Select a small audio file on the TX tab and click transfer.
6. Observe the progress, log output, and successful reception on the RX tab.

## Troubleshooting
* **BLE Connection Fails:** Ensure your OS Bluetooth is on and no other tabs are connected to the ESP32.
* **Corrupted Packets / Checksum Failures:** Align the laser perfectly. Shield the photodiode from ambient light (especially AC lighting).
* **Missing Packets:** If the RX browser reports missing sequence numbers, the UART baud rate might be too high for the optical hardware's slew rate. Consider reducing the baud rate in both firmwares.

## Known Limitations
* The physical hardware is simplex (one-way). Therefore, actual ACK/NAK flow control over the optical link is not possible. The system relies entirely on continuous transmission and robust data framing.
* Web Bluetooth MTU negotiation is OS-dependent.

## Future Improvements
* Bidirectional optical links (Transceivers on both ends) to enable true ACK/NAK flow control and sliding window mechanisms.
* Hardware optical front-end (Transimpedance Amplifiers and Comparators) for gigabit speeds.
* File resume capabilities.

## License
MIT License

## Version History
* v1.0.0 - Initial reliable framing architecture design.
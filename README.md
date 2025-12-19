# ESP32-S3 Dice Roller

A dice rolling application for the Waveshare ESP32-S3-Touch-LCD-1.28 development board. Shake the device to roll the dice!

## Features
- 🎲 Large numeric dice display (1-6) centered on screen
- 📱 Shake detection using integrated QMI8658 accelerometer
- ✨ Rolling animation with visual feedback
- 🎯 Smart shake detection with cooldown period
- 🔧 Serial command fallback if accelerometer unavailable

## Hardware
- **MCU**: ESP32-S3 (240MHz, 320KB RAM, 8MB Flash)
- **Display**: 1.28" Round LCD (240x240 pixels)
- **Display Driver**: GC9A01
- **Accelerometer**: QMI8658 6-axis IMU (integrated)
- **Interface**: SPI (display), I2C (accelerometer)

## Project Structure
```
Dice/
├── platformio.ini    # PlatformIO configuration
├── src/
│   └── main.cpp      # Dice rolling application
└── README.md         # This file
```

## Getting Started

1. Install PlatformIO if you haven't already
2. Connect your ESP32-S3 board via USB
3. Build and upload:
   ```bash
   pio run -t upload
   ```
4. Monitor serial output:
   ```bash
   pio device monitor
   ```

## Usage

- **Shake the device** to roll the dice
- The dice will animate for 1 second, then show a random value (1-6)
- If accelerometer is not detected, you can type "roll" or "r" in the serial monitor

## Libraries Used
- **TFT_eSPI** (v2.5.43): Display library optimized for ESP32 with GC9A01 support
- **NimBLE-Arduino** (v1.4.2): Bluetooth Low Energy library (included, not actively used)
- **Custom QMI8658 Driver**: Integrated I2C driver for onboard accelerometer

## Configuration

### Shake Sensitivity
You can adjust the shake sensitivity by modifying `SHAKE_THRESHOLD` in `main.cpp`:
- Lower values = more sensitive (detects lighter shakes)
- Higher values = less sensitive (requires harder shakes)

### Roll Animation Duration
Modify `ROLL_DURATION` to change how long the rolling animation lasts (in milliseconds).

## Troubleshooting

### Accelerometer Not Detected
If the QMI8658 is not found:
- Check I2C pin connections (GPIO6 = SDA, GPIO7 = SCL)
- Verify I2C address (should be 0x6B)
- Check serial monitor for WHO_AM_I register value
- The app will still work - you can roll dice via serial commands ("roll" or "r")

### Display Issues
The TFT_eSPI library may need pin configuration. Check your board's pinout and adjust if needed.

## Display Specifications
- Resolution: 240x240 pixels
- Interface: SPI (80MHz)
- Driver: GC9A01
- Backlight: GPIO2 (active HIGH)

## Documentation
For complete documentation, see [DOCUMENTATION.md](DOCUMENTATION.md)

## Quick Start
```bash
# Build project
pio run

# Upload to device
pio run -t upload

# Monitor serial output
pio device monitor
```


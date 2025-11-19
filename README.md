# ESP32-S3 Dice Roller

A dice rolling application for the Waveshare ESP32-S3-Touch-LCD-1.28 development board. Shake the device to roll the dice!

## Features
- 🎲 Visual dice display with dot patterns (1-6)
- 📱 Shake detection using built-in accelerometer
- ✨ Rolling animation
- 🎯 Automatic shake detection with cooldown period

## Hardware
- **MCU**: ESP32-S3
- **Display**: 1.28" Round LCD (240x240 pixels)
- **Display Driver**: ST7789V
- **Touch Controller**: CST816S
- **Accelerometer**: MPU6050 (if available on your board)

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
- **TFT_eSPI**: Display library optimized for ESP32
- **Adafruit GFX**: Graphics library
- **Adafruit ST7789**: ST7789 display driver
- **MPU6050**: Accelerometer library for shake detection

## Configuration

### Shake Sensitivity
You can adjust the shake sensitivity by modifying `SHAKE_THRESHOLD` in `main.cpp`:
- Lower values = more sensitive (detects lighter shakes)
- Higher values = less sensitive (requires harder shakes)

### Roll Animation Duration
Modify `ROLL_DURATION` to change how long the rolling animation lasts (in milliseconds).

## Troubleshooting

### Accelerometer Not Detected
If the MPU6050 is not found:
- Check I2C connections (SDA/SCL pins)
- Verify the accelerometer is present on your board variant
- The app will still work - you can roll dice via serial commands

### Display Issues
The TFT_eSPI library may need pin configuration. Check your board's pinout and adjust if needed.

## Display Specifications
- Resolution: 240x240 pixels
- Interface: SPI
- Touch: Capacitive touch (CST816S)


# ESP32-S3 Dice Roller - Complete Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [Hardware Specifications](#hardware-specifications)
3. [Software Architecture](#software-architecture)
4. [Code Structure](#code-structure)
5. [Configuration](#configuration)
6. [Usage Instructions](#usage-instructions)
7. [API Reference](#api-reference)
8. [Troubleshooting](#troubleshooting)
9. [Development Guide](#development-guide)

---

## Project Overview

The ESP32-S3 Dice Roller is an interactive dice application designed for the Waveshare ESP32-S3-Touch-LCD-1.28 development board. The application displays a dice value (1-6) on a round LCD screen and allows users to roll the dice by shaking the device, utilizing the integrated QMI8658 6-axis IMU sensor.

### Key Features
- 🎲 **Visual Dice Display**: Large numeric display (1-6) centered on screen
- 📱 **Shake-to-Roll**: Integrated accelerometer detects device shaking
- ✨ **Rolling Animation**: Visual feedback during dice roll
- 🎯 **Smart Detection**: Cooldown period prevents accidental multiple rolls
- 🔧 **Fallback Mode**: Serial commands available if accelerometer unavailable

---

## Hardware Specifications

### Development Board
- **Model**: Waveshare ESP32-S3-Touch-LCD-1.28
- **MCU**: ESP32-S3 (240MHz, 320KB RAM, 8MB Flash)
- **CPU Speed**: 240MHz
- **Flash Mode**: QIO (Quad I/O)

### Display
- **Type**: 1.28" Round LCD
- **Driver**: GC9A01
- **Resolution**: 240x240 pixels
- **Interface**: SPI (High-Speed SPI)
- **Backlight**: GPIO2 (active HIGH)

### Display Pin Configuration
| Signal | GPIO Pin | Description |
|--------|----------|-------------|
| MISO   | 12       | SPI Master In Slave Out |
| MOSI   | 11       | SPI Master Out Slave In |
| SCLK   | 10       | SPI Clock |
| CS     | 9        | Chip Select |
| DC     | 8        | Data/Command |
| RST    | 14       | Reset |
| BL     | 2        | Backlight Control |

### Accelerometer
- **Sensor**: QMI8658 6-axis IMU
- **Type**: 3-axis accelerometer + 3-axis gyroscope
- **Interface**: I2C
- **I2C Address**: 0x6B
- **I2C Pins**:
  - SDA: GPIO6
  - SCL: GPIO7
- **Range**: ±2g (configurable)
- **Output Data Rate**: 100Hz

---

## Software Architecture

### Framework
- **Platform**: PlatformIO
- **Framework**: Arduino
- **Platform**: Espressif 32

### Libraries
1. **TFT_eSPI** (v2.5.43)
   - Optimized display library for ESP32
   - Supports GC9A01 driver
   - Provides graphics primitives and text rendering

2. **NimBLE-Arduino** (v1.4.2)
   - Bluetooth Low Energy library
   - Included but not actively used in current implementation

### Build Configuration
- **SPI Frequency**: 80MHz
- **Font Support**: GLCD, Font2, Font4, Font6, Font7, Font8, GFXFF, Smooth Font
- **Display Rotation**: 0° (portrait)

---

## Code Structure

### File Organization
```
Dice/
├── platformio.ini          # PlatformIO project configuration
├── src/
│   └── main.cpp            # Main application code
├── README.md               # Quick start guide
└── DOCUMENTATION.md        # This file
```

### Main Components

#### 1. QMI8658 Class
Custom I2C driver for the integrated accelerometer.

**Location**: `src/main.cpp` (lines 24-108)

**Public Methods**:
- `bool begin()`: Initializes the sensor and configures registers
- `void getAcceleration(float* x, float* y, float* z)`: Reads acceleration values in g

**Private Methods**:
- `void writeRegister(uint8_t reg, uint8_t value)`: Writes to sensor register
- `uint8_t readRegister(uint8_t reg)`: Reads from sensor register
- `void readRegisters(uint8_t reg, uint8_t* data, uint8_t len)`: Reads multiple registers

**Configuration Registers**:
- `CTRL1` (0x02): Output Data Rate (100Hz) and accelerometer enable
- `CTRL2` (0x03): Accelerometer range (±2g)
- `CTRL3` (0x04): Additional control settings
- `CTRL7` (0x08): Sensor enable/disable

#### 2. Display Functions

**`drawDiceValue(int value)`** (lines 135-150)
- Displays large numeric dice value (1-6) in center of screen
- Uses text size 8 for maximum visibility
- Clears previous value before drawing

**`drawDice(int value, int x, int y, int size)`** (lines 152-179)
- Draws visual dice representation with dot patterns
- Creates white square with rounded corners
- Renders dots based on dice value (1-6 patterns)

**`drawRollingAnimation()`** (lines 181-185)
- Shows rapid random values during roll animation
- Updates display every loop iteration during roll

#### 3. Dice Logic

**`rollDice()`** (lines 187-198)
- Initiates dice roll sequence
- Generates random value (1-6)
- Sets rolling state and start time
- Prevents multiple simultaneous rolls

**`checkShake()`** (lines 200-228)
- Reads accelerometer data
- Calculates acceleration delta (change from previous reading)
- Detects shake when delta exceeds threshold
- Implements cooldown period to prevent multiple triggers

#### 4. Main Functions

**`setup()`** (lines 230-282)
- Initializes serial communication (115200 baud)
- Initializes QMI8658 accelerometer
- Initializes TFT display
- Enables backlight
- Draws initial screen (instructions + dice value)

**`loop()`** (lines 284-323)
- Continuously checks for shake detection
- Handles serial commands ("roll" or "r")
- Manages rolling animation state
- Updates display when roll completes

---

## Configuration

### Constants (in `main.cpp`)

#### Dice Configuration
```cpp
const unsigned long ROLL_DURATION = 1000;  // Roll animation duration (ms)
```

#### Shake Detection
```cpp
const float SHAKE_THRESHOLD = 15.0;        // Acceleration change threshold (g)
const unsigned long SHAKE_COOLDOWN = 500;  // Minimum time between shakes (ms)
```

#### QMI8658 Configuration
```cpp
#define QMI8658_ADDR 0x6B                  // I2C address
#define QMI8658_SDA 6                      // I2C SDA pin
#define QMI8658_SCL 7                      // I2C SCL pin
```

### Adjustable Parameters

#### Shake Sensitivity
Modify `SHAKE_THRESHOLD` in `main.cpp`:
- **Lower values** (e.g., 10.0): More sensitive, detects lighter shakes
- **Higher values** (e.g., 20.0): Less sensitive, requires harder shakes
- **Default**: 15.0

#### Roll Animation Speed
Modify `ROLL_DURATION` in `main.cpp`:
- **Shorter** (e.g., 500ms): Faster animation
- **Longer** (e.g., 2000ms): Slower animation
- **Default**: 1000ms

#### Cooldown Period
Modify `SHAKE_COOLDOWN` in `main.cpp`:
- **Shorter** (e.g., 250ms): More responsive, may trigger multiple times
- **Longer** (e.g., 1000ms): Prevents accidental multiple rolls
- **Default**: 500ms

### Display Configuration (in `platformio.ini`)

All display settings are configured via build flags:
- Driver: `GC9A01_DRIVER`
- SPI pins: Defined via `TFT_*` flags
- SPI frequency: `SPI_FREQUENCY=80000000` (80MHz)
- Fonts: Multiple font sets enabled

---

## Usage Instructions

### Initial Setup

1. **Install PlatformIO**
   ```bash
   # Install PlatformIO Core
   pip install platformio
   ```

2. **Connect Hardware**
   - Connect ESP32-S3 board via USB-C
   - Ensure proper drivers are installed

3. **Build Project**
   ```bash
   cd Dice
   pio run
   ```

4. **Upload Firmware**
   ```bash
   pio run -t upload
   ```

5. **Monitor Serial Output**
   ```bash
   pio device monitor
   ```

### Operating the Dice

#### Shake-to-Roll (Primary Method)
1. Hold the device in your hand
2. Shake the device firmly
3. The dice will animate for 1 second
4. Final value (1-6) will be displayed

#### Serial Command (Fallback Method)
If accelerometer is unavailable:
1. Open serial monitor (115200 baud)
2. Type `roll` or `r` and press Enter
3. Dice will roll and display result

### Expected Behavior

**On Startup**:
- Blue screen flash (display test)
- "Shake to Roll!" message at top
- Initial dice value "1" in center

**During Shake**:
- Serial output: "Shake detected! (delta: X.XX)"
- Rolling animation begins
- Random values flash on screen

**After Roll**:
- Final value displayed in large text
- Ready for next shake

---

## API Reference

### QMI8658 Class

#### `QMI8658(uint8_t addr = QMI8658_ADDR)`
Constructor for QMI8658 sensor driver.

**Parameters**:
- `addr`: I2C address (default: 0x6B)

#### `bool begin()`
Initializes the QMI8658 sensor.

**Returns**:
- `true`: Sensor initialized successfully
- `false`: Sensor not found or initialization failed

**Side Effects**:
- Configures I2C bus (GPIO6/GPIO7)
- Sets sensor registers for 100Hz output, ±2g range
- Enables accelerometer

#### `void getAcceleration(float* x, float* y, float* z)`
Reads current acceleration values.

**Parameters** (output):
- `x`: Pointer to X-axis acceleration (g)
- `y`: Pointer to Y-axis acceleration (g)
- `z`: Pointer to Z-axis acceleration (g)

**Note**: Values are in g-force units (±2g range)

### Display Functions

#### `void drawDiceValue(int value)`
Displays numeric dice value on screen.

**Parameters**:
- `value`: Dice value (1-6)

**Display**:
- Large text (size 8)
- Centered at (120, 120)
- White text on black background

#### `void drawDice(int value, int x, int y, int size)`
Draws visual dice representation.

**Parameters**:
- `value`: Dice value (1-6)
- `x`: X coordinate (top-left)
- `y`: Y coordinate (top-left)
- `size`: Dice square size in pixels

**Visual**:
- White square with black border
- Rounded corners
- Dot pattern matching dice value

### Dice Control Functions

#### `void rollDice()`
Initiates a dice roll.

**Behavior**:
- Generates random value (1-6)
- Sets rolling state
- Records start time
- Prevents multiple simultaneous rolls

#### `void checkShake()`
Checks accelerometer for shake gesture.

**Algorithm**:
1. Read current acceleration
2. Calculate delta from previous reading
3. If delta > threshold AND cooldown expired:
   - Trigger dice roll
   - Update last shake time
4. Update previous acceleration values

---

## Troubleshooting

### Display Issues

**Problem**: Screen is blank/dark
- **Solution 1**: Check backlight pin (GPIO2) is enabled
- **Solution 2**: Verify SPI pin connections in `platformio.ini`
- **Solution 3**: Check display initialization in serial monitor

**Problem**: Display shows garbled text
- **Solution**: Verify GC9A01 driver is selected in build flags
- **Solution**: Check SPI frequency (should be 80MHz)

**Problem**: Display orientation incorrect
- **Solution**: Modify `tft.setRotation()` in `setup()`
- **Options**: 0, 1, 2, or 3 (90° increments)

### Accelerometer Issues

**Problem**: "QMI8658 not found" message
- **Solution 1**: Check I2C pin connections (GPIO6/GPIO7)
- **Solution 2**: Verify I2C address (should be 0x6B)
- **Solution 3**: Check WHO_AM_I register value in serial output
  - If different from 0x05, update in code

**Problem**: Shake not detected
- **Solution 1**: Lower `SHAKE_THRESHOLD` value (make more sensitive)
- **Solution 2**: Check accelerometer readings in serial monitor
- **Solution 3**: Verify sensor is initialized (check serial output)

**Problem**: Too sensitive (triggers accidentally)
- **Solution**: Increase `SHAKE_THRESHOLD` value (make less sensitive)
- **Solution**: Increase `SHAKE_COOLDOWN` period

### Build/Upload Issues

**Problem**: Compilation errors
- **Solution**: Ensure all libraries are installed: `pio lib install`
- **Solution**: Check PlatformIO version compatibility

**Problem**: Upload fails
- **Solution 1**: Press BOOT button during upload
- **Solution 2**: Check USB cable and port
- **Solution 3**: Verify board selection in `platformio.ini`

**Problem**: Serial monitor not working
- **Solution**: Check baud rate (115200)
- **Solution**: Verify USB port permissions (Linux/Mac)
- **Solution**: Close other programs using the serial port

### Runtime Issues

**Problem**: Dice doesn't roll on shake
- **Solution**: Check serial monitor for "Shake detected!" messages
- **Solution**: Verify accelerometer is available (`accelerometerAvailable == true`)
- **Solution**: Use serial command "roll" to test dice functionality

**Problem**: Multiple rolls from single shake
- **Solution**: Increase `SHAKE_COOLDOWN` value
- **Solution**: Check shake detection logic isn't triggering multiple times

---

## Development Guide

### Adding Features

#### Custom Dice Patterns
Modify `dicePatterns` array (lines 126-133) to change dot layouts:
```cpp
const uint8_t dicePatterns[6][9] = {
  // Format: 3x3 grid, 1 = dot, 0 = empty
  {0, 0, 0, 0, 1, 0, 0, 0, 0}, // 1 (center)
  // ... add custom patterns
};
```

#### Additional Display Modes
Add new display functions:
```cpp
void drawDiceWithAnimation(int value) {
  // Custom animation implementation
}
```

#### Enhanced Shake Detection
Modify `checkShake()` to add:
- Direction detection
- Shake intensity measurement
- Gesture recognition

### Code Organization

**Best Practices**:
1. Keep QMI8658 class self-contained
2. Separate display logic from game logic
3. Use constants for configuration values
4. Add serial debug output for troubleshooting

### Testing

**Unit Testing**:
- Test dice value generation (should be 1-6)
- Test shake detection threshold
- Test cooldown period

**Integration Testing**:
- Test full shake-to-roll sequence
- Test serial command fallback
- Test display updates

### Performance Optimization

**Current Performance**:
- Loop delay: 50ms (20Hz update rate)
- Accelerometer: 100Hz sampling
- Display: Updated on-demand

**Optimization Opportunities**:
- Reduce loop delay for faster response
- Implement interrupt-based shake detection
- Optimize display update regions

---

## Technical Specifications

### Memory Usage
- **RAM**: ~19.6 KB (6.0% of 327KB)
- **Flash**: ~348 KB (10.4% of 3.3MB)

### Timing
- **Roll Animation**: 1000ms
- **Shake Cooldown**: 500ms
- **Loop Delay**: 50ms
- **Sensor Sampling**: 100Hz

### Sensor Specifications
- **Accelerometer Range**: ±2g
- **Resolution**: 16-bit (LSB = 16384 for ±2g)
- **Output Data Rate**: 100Hz
- **I2C Speed**: Standard (100kHz default)

---

## License & Credits

### Hardware
- **Board**: Waveshare ESP32-S3-Touch-LCD-1.28
- **Display Driver**: GC9A01
- **Accelerometer**: QMI8658

### Software Libraries
- **TFT_eSPI**: Bodmer (MIT License)
- **NimBLE-Arduino**: h2zero (Apache 2.0 License)
- **Arduino Framework**: LGPL 2.1

### Project
Developed for interactive dice rolling application on ESP32-S3 platform.

---

## Version History

### v1.0.0 (Current)
- Initial release
- QMI8658 accelerometer integration
- Shake-to-roll functionality
- Large numeric display
- Serial command fallback
- Rolling animation

---

## Support & Resources

### Official Documentation
- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)
- [QMI8658 Datasheet](https://www.waveshare.com)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)

### Community
- PlatformIO Community Forums
- ESP32 Arduino Forums
- Waveshare Support

---

*Last Updated: 2024*
*Documentation Version: 1.0*


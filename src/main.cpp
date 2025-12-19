#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();

// QMI8658 I2C address
#define QMI8658_ADDR 0x6B
#define QMI8658_WHO_AM_I 0x00
#define QMI8658_CTRL1 0x02
#define QMI8658_CTRL2 0x03
#define QMI8658_CTRL3 0x04
#define QMI8658_CTRL7 0x08
#define QMI8658_ACCEL_XOUT_L 0x35

// I2C pins for QMI8658 on Waveshare board
#define QMI8658_SDA 6
#define QMI8658_SCL 7

bool accelerometerAvailable = false;

// QMI8658 sensor class
class QMI8658 {
private:
  uint8_t address;
  TwoWire* wire;
  
public:
  QMI8658(uint8_t addr = QMI8658_ADDR) : address(addr), wire(&Wire) {}
  
  bool begin() {
    // Initialize I2C with custom pins
    wire->begin(QMI8658_SDA, QMI8658_SCL);
    delay(10);
    
    // Check WHO_AM_I register
    uint8_t whoami = readRegister(QMI8658_WHO_AM_I);
    if (whoami != 0x05) {  // QMI8658 WHO_AM_I value
      Serial.print("QMI8658 WHO_AM_I mismatch: 0x");
      Serial.println(whoami, HEX);
      return false;
    }
    
    // Configure accelerometer
    // CTRL1: Set ODR (Output Data Rate) to 100Hz
    writeRegister(QMI8658_CTRL1, 0x60);  // 100Hz, enable accel
    
    // CTRL2: Set accelerometer range to ±2g
    writeRegister(QMI8658_CTRL2, 0x00);  // ±2g range
    
    // CTRL3: Enable accelerometer
    writeRegister(QMI8658_CTRL3, 0x00);
    
    // CTRL7: Enable accelerometer
    writeRegister(QMI8658_CTRL7, 0x20);  // Enable accel
    
    delay(50);
    return true;
  }
  
  void getAcceleration(float* x, float* y, float* z) {
    uint8_t data[6];
    readRegisters(QMI8658_ACCEL_XOUT_L, data, 6);
    
    // Convert to 16-bit signed values
    int16_t ax = (int16_t)(data[1] << 8 | data[0]);
    int16_t ay = (int16_t)(data[3] << 8 | data[2]);
    int16_t az = (int16_t)(data[5] << 8 | data[4]);
    
    // Convert to g (for ±2g range, LSB = 16384)
    *x = ax / 16384.0;
    *y = ay / 16384.0;
    *z = az / 16384.0;
  }
  
private:
  void writeRegister(uint8_t reg, uint8_t value) {
    wire->beginTransmission(address);
    wire->write(reg);
    wire->write(value);
    wire->endTransmission();
  }
  
  uint8_t readRegister(uint8_t reg) {
    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission(false);
    wire->requestFrom(address, (uint8_t)1);
    if (wire->available()) {
      return wire->read();
    }
    return 0;
  }
  
  void readRegisters(uint8_t reg, uint8_t* data, uint8_t len) {
    wire->beginTransmission(address);
    wire->write(reg);
    wire->endTransmission(false);
    wire->requestFrom(address, len);
    for (uint8_t i = 0; i < len; i++) {
      if (wire->available()) {
        data[i] = wire->read();
      }
    }
  }
};

// Initialize QMI8658 accelerometer
QMI8658 qmi8658;

// Dice state
int currentDiceValue = 1;
bool isRolling = false;
unsigned long rollStartTime = 0;
const unsigned long ROLL_DURATION = 1000; // Roll animation duration in ms
const float SHAKE_THRESHOLD = 15.0; // Acceleration threshold for shake detection

// Previous accelerometer values for shake detection
float lastX = 0, lastY = 0, lastZ = 0;
unsigned long lastShakeTime = 0;
const unsigned long SHAKE_COOLDOWN = 500; // Minimum time between shakes (ms)

// Dice face patterns (1-6 dots)
const uint8_t dicePatterns[6][9] = {
  {0, 0, 0, 0, 1, 0, 0, 0, 0}, // 1
  {1, 0, 0, 0, 0, 0, 0, 0, 1}, // 2
  {1, 0, 0, 0, 1, 0, 0, 0, 1}, // 3
  {1, 0, 1, 0, 0, 0, 1, 0, 1}, // 4
  {1, 0, 1, 0, 1, 0, 1, 0, 1}, // 5
  {1, 0, 1, 1, 0, 1, 1, 0, 1}  // 6
};

void drawDiceValue(int value) {
  // Clear the center area for text
  tft.fillRect(0, 80, 240, 80, TFT_BLACK);
  
  // Draw large dice value text in the center
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(8);
  tft.setTextDatum(MC_DATUM); // Middle center alignment
  
  char valueStr[2];
  sprintf(valueStr, "%d", value);
  tft.drawString(valueStr, 120, 120, 1);
  
  Serial.print("Dice value displayed: ");
  Serial.println(value);
}

void drawDice(int value, int x, int y, int size) {
  // Draw dice square
  tft.fillRect(x, y, size, size, TFT_WHITE);
  tft.drawRect(x, y, size, size, TFT_BLACK);
  
  // Draw rounded corners effect
  tft.fillCircle(x + 5, y + 5, 3, TFT_BLACK);
  tft.fillCircle(x + size - 5, y + 5, 3, TFT_BLACK);
  tft.fillCircle(x + 5, y + size - 5, 3, TFT_BLACK);
  tft.fillCircle(x + size - 5, y + size - 5, 3, TFT_BLACK);
  
  // Draw dots based on value (1-6)
  if (value >= 1 && value <= 6) {
    int dotSize = size / 8;
    int spacing = size / 3;
    int offset = size / 6;
    
    for (int row = 0; row < 3; row++) {
      for (int col = 0; col < 3; col++) {
        if (dicePatterns[value - 1][row * 3 + col]) {
          int dotX = x + offset + col * spacing;
          int dotY = y + offset + row * spacing;
          tft.fillCircle(dotX, dotY, dotSize, TFT_BLACK);
        }
      }
    }
  }
}

void drawRollingAnimation() {
  // Show random dice values rapidly during roll
  int animValue = random(1, 7);
  drawDiceValue(animValue);
}

void rollDice() {
  if (isRolling) return; // Already rolling
  
  isRolling = true;
  rollStartTime = millis();
  
  // Generate final random value
  currentDiceValue = random(1, 7);
  
  Serial.print("Rolling dice... Final value: ");
  Serial.println(currentDiceValue);
}

void checkShake() {
  if (!accelerometerAvailable) return;
  
  // Read accelerometer from QMI8658
  float accelX, accelY, accelZ;
  qmi8658.getAcceleration(&accelX, &accelY, &accelZ);
  
  // Calculate total acceleration change
  float deltaX = abs(accelX - lastX);
  float deltaY = abs(accelY - lastY);
  float deltaZ = abs(accelZ - lastZ);
  float totalDelta = deltaX + deltaY + deltaZ;
  
  // Check if shake detected
  unsigned long currentTime = millis();
  if (totalDelta > SHAKE_THRESHOLD && 
      (currentTime - lastShakeTime) > SHAKE_COOLDOWN) {
    lastShakeTime = currentTime;
    rollDice();
    Serial.print("Shake detected! (delta: ");
    Serial.print(totalDelta);
    Serial.println(")");
  }
  
  // Update last values
  lastX = accelX;
  lastY = accelY;
  lastZ = accelZ;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 Dice Roller");
  
  // Initialize QMI8658 integrated accelerometer
  Serial.println("Initializing QMI8658 accelerometer...");
  if (qmi8658.begin()) {
    accelerometerAvailable = true;
    Serial.println("QMI8658 found and initialized!");
    delay(100); // Give sensor time to stabilize
  } else {
    accelerometerAvailable = false;
    Serial.println("Warning: QMI8658 not found. Shake detection disabled.");
    Serial.println("You can still roll dice by using serial commands (type 'roll' or 'r').");
  }
  
  // Initialize display
  Serial.println("Initializing display...");
  tft.init();
  tft.setRotation(0);
  
  // Enable backlight (if pin is defined)
  #ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Backlight enabled");
  #endif
  
  // Clear screen with a test pattern first
  tft.fillScreen(TFT_BLUE);
  delay(100);
  tft.fillScreen(TFT_BLACK);
  
  // Set text wrapping
  tft.setTextWrap(false, false);
  
  // Draw instructions at top
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM); // Top center alignment
  if (accelerometerAvailable) {
    tft.drawString("Shake to Roll!", 120, 10, 1);
  } else {
    tft.drawString("Type 'roll'", 120, 10, 1);
  }
  
  // Draw initial dice value in center
  drawDiceValue(currentDiceValue);
  
  Serial.println("Setup complete!");
  Serial.println("Display initialized - dice value should be visible");
}

void loop() {
  // Check for shake (if accelerometer available)
  if (accelerometerAvailable) {
    checkShake();
  }
  
  // Alternative: Roll on serial command (for testing without accelerometer)
  if (Serial.available()) {
    String cmd = Serial.readString();
    cmd.trim();
    if (cmd == "roll" || cmd == "r") {
      rollDice();
    }
  }
  
  if (isRolling) {
    // Show rolling animation
    drawRollingAnimation();
    
    // Check if roll animation is complete
    if (millis() - rollStartTime >= ROLL_DURATION) {
      isRolling = false;
      // Draw final dice value in center
      drawDiceValue(currentDiceValue);
      
      // Update instruction text
      tft.fillRect(0, 0, 240, 30, TFT_BLACK);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.setTextSize(2);
      tft.setTextDatum(TC_DATUM);
      if (accelerometerAvailable) {
        tft.drawString("Shake to Roll!", 120, 10, 1);
      } else {
        tft.drawString("Type 'roll'", 120, 10, 1);
      }
    }
  }
  
  delay(50); // Small delay for stability
}

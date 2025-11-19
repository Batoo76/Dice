#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <MPU6050.h>

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();

// Initialize MPU6050 accelerometer
MPU6050 mpu;
bool accelerometerAvailable = false;

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
  drawDice(animValue, 70, 70, 100);
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
  
  // Read accelerometer
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  
  // Convert to g-force (MPU6050 default is ±2g, so divide by 16384)
  float accelX = ax / 16384.0;
  float accelY = ay / 16384.0;
  float accelZ = az / 16384.0;
  
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
    Serial.println("Shake detected!");
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
  
  // Initialize I2C for MPU6050
  Wire.begin();
  
  // Initialize MPU6050
  Serial.println("Initializing MPU6050...");
  if (mpu.begin()) {
    accelerometerAvailable = true;
    Serial.println("MPU6050 found!");
    
    // Calibrate MPU6050
    mpu.calcGyroOffsets(true);
    Serial.println("MPU6050 calibrated!");
  } else {
    accelerometerAvailable = false;
    Serial.println("Warning: MPU6050 not found. Shake detection disabled.");
    Serial.println("You can still roll dice by touching the screen or using serial commands.");
  }
  
  // Initialize display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  // Draw initial dice
  drawDice(currentDiceValue, 70, 70, 100);
  
  // Draw instructions
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(50, 10);
  if (accelerometerAvailable) {
    tft.println("Shake to Roll!");
  } else {
    tft.println("Touch to Roll!");
  }
  
  Serial.println("Setup complete!");
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
      // Draw final dice value
      drawDice(currentDiceValue, 70, 70, 100);
      
      // Update instruction text
      tft.fillRect(0, 0, 240, 20, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      tft.setCursor(50, 10);
      if (accelerometerAvailable) {
        tft.println("Shake to Roll!");
      } else {
        tft.println("Touch to Roll!");
      }
    }
  }
  
  delay(50); // Small delay for stability
}

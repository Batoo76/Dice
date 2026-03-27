#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <math.h>
#include "QMI8658.h"
#include "DEV_Config.h"

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();

// CST816S Touch controller I2C address and pins (shares I2C bus)
#define CST816S_ADDR 0x15
#define CST816S_TOUCH_NUM 0x02
#define CST816S_XPOS_H 0x03
#define CST816S_XPOS_L 0x04
#define CST816S_YPOS_H 0x05
#define CST816S_YPOS_L 0x06

bool accelerometerAvailable = false;

// Accelerometer data buffer
float accelData[3] = {0, 0, 0};

// Conversion constant: 1g = 9.807 m/s²
const float ONE_G_MS2 = 9.807f;

// Dice types
const int DICE_TYPES[] = {4, 6, 8, 10, 20, 100};
const int NUM_DICE_TYPES = 6;
int currentDiceTypeIndex = 1; // Start with D6 (index 1)
int currentDiceFaces = 6; // Current number of faces

// Application states
enum AppState {
  STATE_SELECT_DICE,  // Selecting dice type
  STATE_ROLLING,      // Rolling animation
  STATE_SHOW_RESULT   // Showing final result
};
AppState currentState = STATE_SELECT_DICE;

// Dice state
int currentDiceValue = 1;
bool isRolling = false;
unsigned long rollStartTime = 0;
const unsigned long ROLL_DURATION = 1000; // Roll animation duration in ms
const float SHAKE_THRESHOLD = 2.0; // Acceleration threshold for shake detection (in g)

// Touch detection
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_COOLDOWN = 300; // Minimum time between touch events (ms)
unsigned long touchStartTime = 0;
bool touchActive = false;
const unsigned long LONG_PRESS_DURATION = 500; // Long press duration in ms

// Previous accelerometer values for shake detection
float lastX = 0, lastY = 0, lastZ = 0;
unsigned long lastShakeTime = 0;
const unsigned long SHAKE_COOLDOWN = 500; // Minimum time between shakes (ms)

// Debug and monitoring
unsigned long lastDebugPrint = 0;
const unsigned long DEBUG_INTERVAL = 500; // Print debug info every 500ms
bool debugMode = true; // Enable detailed serial output

// Dice face patterns (1-6 dots)
const uint8_t dicePatterns[6][9] = {
  {0, 0, 0, 0, 1, 0, 0, 0, 0}, // 1
  {1, 0, 0, 0, 0, 0, 0, 0, 1}, // 2
  {1, 0, 0, 0, 1, 0, 0, 0, 1}, // 3
  {1, 0, 1, 0, 0, 0, 1, 0, 1}, // 4
  {1, 0, 1, 0, 1, 0, 1, 0, 1}, // 5
  {1, 0, 1, 1, 0, 1, 1, 0, 1}  // 6
};

// Muted wireframe color (readable behind bright text)
static const uint16_t DICE_WIREFRAME_COLOR = 0x2965; // dark gray-blue

// Isometric-style 2D projection (same idea as D10: explicit screen coords, then drawLine)
static void isoTo2D(float x, float y, float z, int cx, int cy, float sc, int* ox, int* oy) {
  *ox = cx + (int)((x - z) * sc);
  *oy = cy + (int)((x + z) * sc * 0.55f - y * sc);
}

// Wireframe: 3D unit-scale vertices, edges = shortest nonzero inter-vertex distance (platonic solids)
static void drawWireframeIso(const float* vx, const float* vy, const float* vz, int nVert,
                             int cx, int cy, int r, uint16_t color) {
  if (nVert < 2 || nVert > 16) return;
  const float sc = (float)r * 0.45f;
  int px[16], py[16];
  for (int i = 0; i < nVert; i++) {
    isoTo2D(vx[i], vy[i], vz[i], cx, cy, sc, &px[i], &py[i]);
  }
  float minD = 1e9f;
  for (int i = 0; i < nVert; i++) {
    for (int j = i + 1; j < nVert; j++) {
      float dx = vx[i] - vx[j];
      float dy = vy[i] - vy[j];
      float dz = vz[i] - vz[j];
      float d = sqrtf(dx * dx + dy * dy + dz * dz);
      if (d > 1e-5f && d < minD) minD = d;
    }
  }
  if (minD >= 1e8f) return;
  const float tol = minD * 0.12f;
  for (int i = 0; i < nVert; i++) {
    for (int j = i + 1; j < nVert; j++) {
      float dx = vx[i] - vx[j];
      float dy = vy[i] - vy[j];
      float dz = vz[i] - vz[j];
      float d = sqrtf(dx * dx + dy * dy + dz * dz);
      if (fabsf(d - minD) <= tol) {
        tft.drawLine(px[i], py[i], px[j], py[j], color);
      }
    }
  }
}

// D4: same visual language as D10 — 2D trig, no 3D pipeline
static void drawD4Wireframe2D(int cx, int cy, int r, uint16_t color) {
  const int n = 3;
  const float ang0 = -M_PI / 2.0f;
  int tx, ty, bx[3], by[3];
  for (int k = 0; k < n; k++) {
    float a = ang0 + k * (2.0f * (float)M_PI / n);
    bx[k] = cx + (int)(cosf(a) * r);
    by[k] = cy + (int)(sinf(a) * r * 0.92f) + (int)(r * 0.12f);
  }
  tx = cx;
  ty = cy - (int)(r * 0.95f);
  for (int k = 0; k < n; k++) {
    int nk = (k + 1) % n;
    tft.drawLine(bx[k], by[k], bx[nk], by[nk], color);
    tft.drawLine(tx, ty, bx[k], by[k], color);
  }
}

// D20: outer hex + inner rotated hex + radials (same nested-polygon pattern as D10)
static void drawD20Wireframe2D(int cx, int cy, int r, uint16_t color) {
  const int n = 6;
  const float ang0 = -M_PI / 2.0f;
  int ox[n], oy[n], ix[n], iy[n];
  int ir = r * 58 / 100;
  for (int k = 0; k < n; k++) {
    float a = ang0 + k * (2.0f * (float)M_PI / n);
    ox[k] = cx + (int)(cosf(a) * r);
    oy[k] = cy + (int)(sinf(a) * r);
    float a2 = ang0 + (k + 0.5f) * (2.0f * (float)M_PI / n);
    ix[k] = cx + (int)(cosf(a2) * ir);
    iy[k] = cy + (int)(sinf(a2) * ir);
  }
  for (int k = 0; k < n; k++) {
    int nk = (k + 1) % n;
    tft.drawLine(ox[k], oy[k], ox[nk], oy[nk], color);
    tft.drawLine(ix[k], iy[k], ix[nk], iy[nk], color);
    tft.drawLine(ox[k], oy[k], ix[k], iy[k], color);
    tft.drawLine(ox[nk], oy[nk], ix[k], iy[k], color);
  }
}

// Stylized D10 (pentagonal trapezohedron hint): nested pentagons + zigzag
static void drawD10Wireframe2D(int cx, int cy, int r, uint16_t color) {
  const int n = 5;
  float ang0 = -M_PI / 2.0f;
  int ox[n], oy[n], ix[n], iy[n];
  int ir = r * 55 / 100;
  for (int k = 0; k < n; k++) {
    float a = ang0 + k * (2.0f * (float)M_PI / n);
    ox[k] = cx + (int)(cosf(a) * r);
    oy[k] = cy + (int)(sinf(a) * r);
    float a2 = ang0 + (k + 0.5f) * (2.0f * (float)M_PI / n);
    ix[k] = cx + (int)(cosf(a2) * ir);
    iy[k] = cy + (int)(sinf(a2) * ir);
  }
  for (int k = 0; k < n; k++) {
    int nk = (k + 1) % n;
    tft.drawLine(ox[k], oy[k], ox[nk], oy[nk], color);
    tft.drawLine(ix[k], iy[k], ix[nk], iy[nk], color);
    tft.drawLine(ox[k], oy[k], ix[k], iy[k], color);
    tft.drawLine(ox[nk], oy[nk], ix[k], iy[k], color);
  }
}

// Percentile dice: two D10-style gems side by side
static void drawD100Wireframe2D(int cx, int cy, int r, uint16_t color) {
  int gap = r * 8 / 10;
  drawD10Wireframe2D(cx - gap, cy, r, color);
  drawD10Wireframe2D(cx + gap, cy, r, color);
}

// Background wireframe for the currently highlighted dice type (selection screen)
static void drawDiceTypeWireframeBackground(int faces) {
  const int cx = 120;
  const int cy = 102;
  const int r = 60; // match D10 outer radius for consistent size

  switch (faces) {
    case 4:
      drawD4Wireframe2D(cx, cy, r, DICE_WIREFRAME_COLOR);
      break;
    case 6: {
      const float c[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
      };
      float vx[8], vy[8], vz[8];
      for (int i = 0; i < 8; i++) {
        vx[i] = c[i][0] * 0.48f;
        vy[i] = c[i][1] * 0.48f;
        vz[i] = c[i][2] * 0.48f;
      }
      drawWireframeIso(vx, vy, vz, 8, cx, cy, r * 2, DICE_WIREFRAME_COLOR);
      break;
    }
    case 8: {
      const float o[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
      };
      float vx[6], vy[6], vz[6];
      for (int i = 0; i < 6; i++) {
        vx[i] = o[i][0] * 0.65f;
        vy[i] = o[i][1] * 0.65f;
        vz[i] = o[i][2] * 0.65f;
      }
      drawWireframeIso(vx, vy, vz, 6, cx, cy, r * 3, DICE_WIREFRAME_COLOR);
      break;
    }
    case 10:
      drawD10Wireframe2D(cx, cy, r, DICE_WIREFRAME_COLOR);
      break;
    case 20:
      drawD20Wireframe2D(cx, cy, r, DICE_WIREFRAME_COLOR);
      break;
    case 100:
      // Two D10-style gems (doubled from r*36/100)
      drawD100Wireframe2D(cx, cy, r * 72 / 100, DICE_WIREFRAME_COLOR);
      break;
    default:
      break;
  }
}

void drawDiceValue(int value) {
  // Clear the center area for text (adjusted to avoid overlap with "Shake to Roll!")
  tft.fillRect(0, 60, 240, 60, TFT_BLACK);

  // Same die wireframe as on the selection screen, behind the digits
  drawDiceTypeWireframeBackground(currentDiceFaces);
  
  // Draw large dice value text in the center (slightly higher)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(8);
  tft.setTextDatum(MC_DATUM); // Middle center alignment
  
  char valueStr[4]; // Support up to 3 digits (100)
  sprintf(valueStr, "%d", value);
  tft.drawString(valueStr, 120, 90, 1); // Moved up to y=90
  
  Serial.print("Dice value displayed: ");
  Serial.println(value);
}

void drawDiceSelectionScreen() {
  // Clear screen
  tft.fillScreen(TFT_BLACK);

  // Wireframe of the currently selected dice type (behind labels)
  drawDiceTypeWireframeBackground(currentDiceFaces);
  
  // Draw title
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Dice Type", 120, 20, 1);
  
  // Draw current dice type (large)
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(6);
  tft.setTextDatum(MC_DATUM);
  char diceTypeStr[5];
  sprintf(diceTypeStr, "D%d", currentDiceFaces);
  tft.drawString(diceTypeStr, 120, 100, 1);
  
  // Draw instructions
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Tap to cycle", 120, 150, 1);
  tft.drawString("Long press to select", 120, 165, 1);
  tft.drawString("Shake to valid", 120, 180, 1);
  
  // Draw all available dice types at bottom
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  String diceList = "4  6  8  10  20  100";
  tft.drawString(diceList, 120, 210, 1);
  
  // Highlight current selection
  int xPos = 20; // Starting X position
  int spacing = 35; // Spacing between dice types
  for (int i = 0; i < NUM_DICE_TYPES; i++) {
    if (i == currentDiceTypeIndex) {
      // Draw highlight box
      tft.drawRect(xPos + i * spacing - 8, 200, 16, 20, TFT_YELLOW);
    }
  }
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
  int animValue = random(1, currentDiceFaces + 1);
  drawDiceValue(animValue);
}

void rollDice() {
  if (isRolling) return; // Already rolling
  
  isRolling = true;
  rollStartTime = millis();
  currentState = STATE_ROLLING;
  
  // Generate final random value based on current dice type
  currentDiceValue = random(1, currentDiceFaces + 1);
  
  Serial.print("Rolling D");
  Serial.print(currentDiceFaces);
  Serial.print("... Final value: ");
  Serial.println(currentDiceValue);
  
  // Clear screen and show rolling
  tft.fillScreen(TFT_BLACK);
  drawDiceTypeWireframeBackground(currentDiceFaces);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("Rolling...", 120, 20, 1);
}

void checkShake() {
  if (!accelerometerAvailable) return;
  
  // Read accelerometer from QMI8658 library (returns values in m/s²)
  QMI8658_read_acc_xyz(accelData);
  // Convert from m/s² to g (divide by 9.807 m/s² per g)
  float accelX = accelData[0] / ONE_G_MS2;
  float accelY = accelData[1] / ONE_G_MS2;
  float accelZ = accelData[2] / ONE_G_MS2;
  
  // Calculate total acceleration change
  float deltaX = abs(accelX - lastX);
  float deltaY = abs(accelY - lastY);
  float deltaZ = abs(accelZ - lastZ);
  float totalDelta = deltaX + deltaY + deltaZ;
  
  // Calculate magnitude of current acceleration
  float magnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
  
  // Debug output
  unsigned long currentTime = millis();
  if (debugMode && (currentTime - lastDebugPrint) >= DEBUG_INTERVAL) {
    lastDebugPrint = currentTime;
    
    Serial.println("=== Accelerometer Status ===");
    Serial.print("Acceleration (m/s²) - X: ");
    Serial.print(accelData[0], 2);
    Serial.print(", Y: ");
    Serial.print(accelData[1], 2);
    Serial.print(", Z: ");
    Serial.println(accelData[2], 2);
    
    // Convert to mg for display
    float accelX_mg = accelData[0] / ONE_G_MS2 * 1000.0f;
    float accelY_mg = accelData[1] / ONE_G_MS2 * 1000.0f;
    float accelZ_mg = accelData[2] / ONE_G_MS2 * 1000.0f;
    Serial.print("Acceleration (mg) - X: ");
    Serial.print(accelX_mg, 1);
    Serial.print(", Y: ");
    Serial.print(accelY_mg, 1);
    Serial.print(", Z: ");
    Serial.println(accelZ_mg, 1);
    
    Serial.print("Acceleration (g) - X: ");
    Serial.print(accelX, 3);
    Serial.print(", Y: ");
    Serial.print(accelY, 3);
    Serial.print(", Z: ");
    Serial.print(accelZ, 3);
    Serial.print(", Magnitude: ");
    Serial.println(magnitude, 3);
    
    Serial.print("Delta - X: ");
    Serial.print(deltaX, 3);
    Serial.print(", Y: ");
    Serial.print(deltaY, 3);
    Serial.print(", Z: ");
    Serial.print(deltaZ, 3);
    Serial.print(", Total: ");
    Serial.print(totalDelta, 3);
    Serial.print(" (Threshold: ");
    Serial.print(SHAKE_THRESHOLD);
    Serial.println(")");
    
    unsigned long timeSinceLastShake = currentTime - lastShakeTime;
    Serial.print("Time since last shake: ");
    Serial.print(timeSinceLastShake);
    Serial.print("ms (Cooldown: ");
    Serial.print(SHAKE_COOLDOWN);
    Serial.println("ms)");
    
    if (totalDelta > SHAKE_THRESHOLD) {
      Serial.print(">>> SHAKE DETECTED! (delta: ");
      Serial.print(totalDelta, 3);
      Serial.print(" > threshold: ");
      Serial.print(SHAKE_THRESHOLD);
      if (timeSinceLastShake < SHAKE_COOLDOWN) {
        Serial.print(") - BUT IN COOLDOWN (");
        Serial.print(SHAKE_COOLDOWN - timeSinceLastShake);
        Serial.println("ms remaining)");
      } else {
        Serial.println(") - READY TO TRIGGER!");
      }
    } else {
      Serial.print("No shake (delta: ");
      Serial.print(totalDelta, 3);
      Serial.print(" < threshold: ");
      Serial.print(SHAKE_THRESHOLD);
      Serial.println(")");
    }
    Serial.println("============================");
  }
  
  // Check if shake detected
  if (totalDelta > SHAKE_THRESHOLD && 
      (currentTime - lastShakeTime) > SHAKE_COOLDOWN) {
    lastShakeTime = currentTime;
    
    if (currentState == STATE_SELECT_DICE) {
      // Shake to confirm dice selection
      currentState = STATE_SHOW_RESULT;
      Serial.println(">>> SHAKE DETECTED - CONFIRMING DICE SELECTION! <<<");
      Serial.print("Selected dice: D");
      Serial.println(currentDiceFaces);
      
      // Clear screen and show result screen (value + wireframe first, then chrome on top)
      tft.fillScreen(TFT_BLACK);
      drawDiceValue(1);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setTextSize(2);
      tft.setTextDatum(TC_DATUM);
      char confirmStr[20];
      sprintf(confirmStr, "D%d Selected", currentDiceFaces);
      tft.drawString(confirmStr, 120, 50, 1);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString("Shake to Roll!", 120, 160, 1);
      
      delay(1000); // Show confirmation briefly
    } else {
      // Shake to roll dice
      rollDice();
      Serial.println(">>> SHAKE DETECTED - ROLLING DICE! <<<");
      Serial.print("  Delta: ");
      Serial.print(totalDelta, 3);
      Serial.print("g, Magnitude: ");
      Serial.print(magnitude, 3);
      Serial.println("g");
    }
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
  
  // Initialize I2C bus for QMI8658
  Serial.println("\n========================================");
  Serial.println("Initializing QMI8658 accelerometer...");
  Serial.println("========================================");
  Serial.print("I2C Pins - SDA: GPIO");
  Serial.print(QMI8658_SDA);
  Serial.print(", SCL: GPIO");
  Serial.println(QMI8658_SCL);
  
  // Initialize I2C
  DEV_I2C_Init();
  delay(10);
  
  // Initialize QMI8658 using library
  if (QMI8658_init()) {
    accelerometerAvailable = true;
    Serial.println("\n>>> QMI8658 SUCCESSFULLY INITIALIZED! <<<");
    Serial.print("Shake threshold: ");
    Serial.print(SHAKE_THRESHOLD);
    Serial.println("g");
    Serial.print("Cooldown period: ");
    Serial.print(SHAKE_COOLDOWN);
    Serial.println("ms");
    delay(100); // Give sensor time to stabilize
    
    // Read initial values (library returns m/s²)
    QMI8658_read_acc_xyz(accelData);
    float initX = accelData[0] / ONE_G_MS2; // Convert from m/s² to g
    float initY = accelData[1] / ONE_G_MS2;
    float initZ = accelData[2] / ONE_G_MS2;
    Serial.println("\nInitial accelerometer reading:");
    Serial.print("  X: ");
    Serial.print(initX, 3);
    Serial.print("g, Y: ");
    Serial.print(initY, 3);
    Serial.print("g, Z: ");
    Serial.print(initZ, 3);
    Serial.println("g");
    lastX = initX;
    lastY = initY;
    lastZ = initZ;
  } else {
    accelerometerAvailable = false;
    Serial.println("\n>>> WARNING: QMI8658 INITIALIZATION FAILED! <<<");
    Serial.println("Shake detection disabled.");
    Serial.println("You can still roll dice by using serial commands (type 'roll' or 'r').");
    Serial.println("\nTroubleshooting:");
    Serial.println("  1. Check I2C pin connections (SDA=GPIO6, SCL=GPIO7)");
    Serial.println("  2. Verify I2C address (should be 0x6A)");
    Serial.println("  3. Check WHO_AM_I register value in output above");
  }
  Serial.println("========================================\n");
  
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
  
  // Start in dice selection state
  currentState = STATE_SELECT_DICE;
  currentDiceFaces = DICE_TYPES[currentDiceTypeIndex];
  
  // Draw dice selection screen
  drawDiceSelectionScreen();
  
  Serial.println("Setup complete!");
  Serial.println("Dice selection screen displayed");
  Serial.println("Available dice types: D4, D6, D8, D10, D20, D100");
  Serial.println("Serial commands: 'next' or 'n' to cycle, 'confirm' or 'c' to select");
}

bool readTouch(uint16_t* x, uint16_t* y) {
  // Read touch from CST816S via I2C
  Wire.beginTransmission(CST816S_ADDR);
  uint8_t error = Wire.endTransmission();
  if (error != 0) {
    return false; // Touch controller not responding
  }
  
  // Read touch number register
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(CST816S_TOUCH_NUM);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)1);
  
  if (!Wire.available()) return false;
  uint8_t touchNum = Wire.read();
  
  if (touchNum == 0) return false; // No touch
  
  // Read X and Y coordinates
  uint8_t data[4];
  Wire.beginTransmission(CST816S_ADDR);
  Wire.write(CST816S_XPOS_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)4);
  
  if (Wire.available() < 4) return false;
  
  data[0] = Wire.read(); // X high
  data[1] = Wire.read(); // X low
  data[2] = Wire.read(); // Y high
  data[3] = Wire.read(); // Y low
  
  // Convert to coordinates (240x240 display)
  *x = ((data[0] & 0x0F) << 8) | data[1];
  *y = ((data[2] & 0x0F) << 8) | data[3];
  
  return true;
}

void checkTouch() {
  uint16_t x = 0, y = 0;
  bool touched = readTouch(&x, &y);
  unsigned long currentTime = millis();
  
  if (touched) {
    // Touch is active
    if (!touchActive) {
      // Touch just started
      touchActive = true;
      touchStartTime = currentTime;
      Serial.print("Touch started at X: ");
      Serial.print(x);
      Serial.print(", Y: ");
      Serial.println(y);
    } else {
      // Touch is continuing - check for long press
      unsigned long touchDuration = currentTime - touchStartTime;
      
      if (touchDuration >= LONG_PRESS_DURATION && (currentTime - lastTouchTime) > TOUCH_COOLDOWN) {
        // Long press detected
        lastTouchTime = currentTime;
        touchActive = false; // Reset touch state
        
        Serial.print("Long press detected! (");
        Serial.print(touchDuration);
        Serial.println("ms)");
        
        // Long press: Go back to dice selection mode
        if (currentState != STATE_SELECT_DICE) {
          currentState = STATE_SELECT_DICE;
          Serial.println("Returning to dice selection mode");
          drawDiceSelectionScreen();
        } else {
          // Already in selection mode - confirm selection
          currentState = STATE_SHOW_RESULT;
          Serial.print("Confirming dice selection: D");
          Serial.println(currentDiceFaces);
          
          tft.fillScreen(TFT_BLACK);
          drawDiceValue(1);
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
          tft.setTextSize(2);
          tft.setTextDatum(TC_DATUM);
          char confirmStr[20];
          sprintf(confirmStr, "D%d Selected", currentDiceFaces);
          tft.drawString(confirmStr, 120, 50, 1);
          tft.setTextColor(TFT_CYAN, TFT_BLACK);
          tft.drawString("Shake to Roll!", 120, 160, 1);
        }
      }
    }
  } else {
    // No touch detected
    if (touchActive) {
      // Touch just ended - check if it was a short press
      unsigned long touchDuration = currentTime - touchStartTime;
      touchActive = false;
      
      if (touchDuration < LONG_PRESS_DURATION && (currentTime - lastTouchTime) > TOUCH_COOLDOWN) {
        // Short press - cycle dice type (only in selection mode)
        lastTouchTime = currentTime;
        
        Serial.print("Short press detected (");
        Serial.print(touchDuration);
        Serial.println("ms)");
        
        if (currentState == STATE_SELECT_DICE) {
          // Cycle to next dice type
          currentDiceTypeIndex = (currentDiceTypeIndex + 1) % NUM_DICE_TYPES;
          currentDiceFaces = DICE_TYPES[currentDiceTypeIndex];
          
          Serial.print("Dice type changed to D");
          Serial.println(currentDiceFaces);
          
          // Redraw selection screen
          drawDiceSelectionScreen();
        }
      }
    }
  }
}

void loop() {
  // Handle dice selection state
  if (currentState == STATE_SELECT_DICE) {
    // Check for touch to cycle dice types
    checkTouch();
    
    // Check for shake to confirm selection (handled in checkShake)
    if (accelerometerAvailable) {
      checkShake();
    }
    
    // Alternative: Serial commands for testing
    if (Serial.available()) {
      String cmd = Serial.readString();
      cmd.trim();
      
      if (cmd == "next" || cmd == "n") {
        // Cycle to next dice type
        currentDiceTypeIndex = (currentDiceTypeIndex + 1) % NUM_DICE_TYPES;
        currentDiceFaces = DICE_TYPES[currentDiceTypeIndex];
        Serial.print("Dice type changed to D");
        Serial.println(currentDiceFaces);
        drawDiceSelectionScreen();
      } else if (cmd == "confirm" || cmd == "c") {
        // Confirm selection
        currentState = STATE_SHOW_RESULT;
        tft.fillScreen(TFT_BLACK);
        drawDiceValue(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(TC_DATUM);
        char confirmStr[20];
        sprintf(confirmStr, "D%d Selected", currentDiceFaces);
        tft.drawString(confirmStr, 120, 50, 1);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("Shake to Roll!", 120, 160, 1);
      }
    }
  } else {
    // In rolling or result state
    // Check for touch (long press to return to dice selection)
    checkTouch();
    
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
        currentState = STATE_SHOW_RESULT;
        
        // Draw final dice value in center
        drawDiceValue(currentDiceValue);
        
        // Clear instruction band below wireframe (past D10 bottom ~cy+r = 162)
        tft.fillRect(0, 165, 240, 50, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM); // Middle center alignment
        if (accelerometerAvailable) {
          tft.drawString("Shake to Roll!", 120, 160, 1); // Centered below dice value
        } else {
          tft.drawString("Type 'roll'", 120, 160, 1);
        }
      }
    }
  }
  
  delay(50); // Small delay for stability
}

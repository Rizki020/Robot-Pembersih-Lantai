/**
 * Kode Final Robot Pembersih Lantai
 * Versi Definitif Final - Perbaikan Real-time Speed Control
 */

#include <NewPing.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define FORWARD 'F'
#define BACKWARD 'B'
#define LEFT 'L'
#define RIGHT 'R'
#define SQUARE 'S'
#define START 'A'
#define PAUSE 'P'
#define TRIANGLE 'T'
#define CROSS 'X'

// --- Deklarasi Pin Hardware ---
const int echo_L = 13, trig_L = 3;
const int echo_M = 4, trig_M = 5;
const int echo_R = A3, trig_R = A2;
const int ENA = 6, IN1 = 7, IN2 = 8;
const int IN3 = 9, IN4 = 10, ENB = 11;
const int buttonPin = 12;
const int pumpPin = 1;
const int BT_RX_PIN = A0;
const int BT_TX_PIN = A1;

// --- Pengaturan Kecepatan ---
const int AUTO_FORWARD_SPEED = 210;
const int MIN_MANUAL_SPEED = 50;
const int TURN_SPEED_AUTO = 200;
const int REVERSE_SPEED_MANUAL = 200;
const int REVERSE_SPEED_AUTO = 220;

// --- Variabel Global ---
int motor_speed = 170;
const int max_distance = 200;
int distance_L, distance_M, distance_R;

// --- Variabel untuk Manual Assist---
const int DANGER_ZONE_START = 25;
const int DANGER_ZONE_END = 10;
const int MIN_ASSIST_SPEED = 80;

// --- Variabel Logika Mode & Lainnya---
bool isAutomaticMode = false;
bool isAutoRunning = false;
bool lastButtonState = HIGH;
enum DisplayState { NEEDS_UPDATE, MANUAL_DISPLAYED, AUTO_STOPPED_DISPLAYED, AUTO_RUNNING_DISPLAYED };
DisplayState currentDisplayState = NEEDS_UPDATE;
unsigned long pumpTimer = 0;
const long pumpOnDuration = 5000;
const long pumpOffDuration = 10000;
bool isPumpOn = false;
unsigned long lastManualActivityTime = 0;
const long backlightTimeoutDuration = 60000;

NewPing sonar_L(trig_L, echo_L, max_distance);
NewPing sonar_M(trig_M, echo_M, max_distance);
NewPing sonar_R(trig_R, echo_R, max_distance);
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial bluetooth(BT_RX_PIN, BT_TX_PIN);

// --- Deklarasi Prototipe Fungsi ---
void printLine(int line, String text);
void moveForward(int speed);
void moveBackward(int speed);
void moveLeft();
void moveRight();
void moveStop();
void turnWithSpeed(char direction, int duration);
void automaticMode();

// ===================================================================
// ========================== FUNGSI UTAMA ===========================
// ===================================================================

void setup() {
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  moveStop();
  digitalWrite(pumpPin, HIGH);
  lcd.init();
  lcd.backlight();
  printLine(0, "Robot Siap!");
  delay(2000);
  bluetooth.begin(9600);
  lastButtonState = digitalRead(buttonPin);
  isAutomaticMode = (lastButtonState == LOW);
  lastManualActivityTime = millis();
  randomSeed(analogRead(A5));
}

void loop() {
  handleButtonPress();
  handleBluetoothCommands();
  updateDisplayIfNeeded();
  handleBacklightTimeout();

  if (isAutomaticMode && isAutoRunning) {
    automaticMode();
  }
}

// ===================================================================
// ======================= FUNGSI-FUNGSI LOGIKA ======================
// ===================================================================

void handleBluetoothCommands() {
  if (bluetooth.available() > 0) {
    char command = bluetooth.read();
    if (isAutomaticMode) {
      if (!isAutoRunning && command == START) {
        isAutoRunning = true;
        pumpTimer = millis();
        currentDisplayState = NEEDS_UPDATE;
      } else if (isAutoRunning && command == SQUARE) {
        isAutoRunning = false;
        moveStop();
        digitalWrite(pumpPin, HIGH);
        isPumpOn = false;
        pumpTimer = millis();
        currentDisplayState = NEEDS_UPDATE;
      }
    } else {
      lcd.backlight();
      lastManualActivityTime = millis();
      String msg = "";
      bool displayMsg = true;
      
      switch (command) {
        case FORWARD: {
          distance_M = sonar_M.ping_cm(); if (distance_M == 0) distance_M = max_distance;
          int safe_speed = motor_speed;
          if (distance_M < DANGER_ZONE_START) {
              safe_speed = map(distance_M, DANGER_ZONE_END, DANGER_ZONE_START, MIN_ASSIST_SPEED, motor_speed);
              safe_speed = constrain(safe_speed, MIN_ASSIST_SPEED, motor_speed);
              msg = "AWAS! Speed: " + String(safe_speed);
          } else {
              msg = "Maju...";
          }
          moveForward(safe_speed);
          break;
        }
        case BACKWARD:
          moveBackward(REVERSE_SPEED_MANUAL);
          msg = "Mundur...";
          break;
        case LEFT:     moveLeft();     msg = "Belok Kiri..."; break;
        case RIGHT:    moveRight();    msg = "Belok Kanan..."; break;
        case SQUARE:   moveStop();     msg = "Berhenti."; break;
        case START:    digitalWrite(pumpPin, LOW);  msg = "Pompa ON"; break;
        case PAUSE:    digitalWrite(pumpPin, HIGH); msg = "Pompa OFF"; break;
        
        // ### PERBAIKAN DI SINI ###
        case TRIANGLE: 
          motor_speed = min(255, motor_speed + 10);
          analogWrite(ENA, motor_speed); // Langsung terapkan kecepatan baru
          analogWrite(ENB, motor_speed);
          msg = "Speed: " + String(motor_speed);
          break;
        case CROSS:    
          motor_speed = max(MIN_MANUAL_SPEED, motor_speed - 10);
          analogWrite(ENA, motor_speed); // Langsung terapkan kecepatan baru
          analogWrite(ENB, motor_speed);
          msg = "Speed: " + String(motor_speed);
          break;
          
        default:
          displayMsg = false;
          break;
      }
      if (displayMsg) {
          printLine(1, msg);
      }
    }
  }
}

void automaticMode() {
  handlePeriodicPump();

  // --- Pembacaan sensor yang lebih stabil ---
  distance_L = sonar_L.ping_median(5); delay(50);
  if (distance_L == 0) distance_L = max_distance;

  distance_M = sonar_M.ping_median(5); delay(50);
  if (distance_M == 0) distance_M = max_distance;

  distance_R = sonar_R.ping_median(5); delay(50);
  if (distance_R == 0) distance_R = max_distance;

  String sensorData = "L" + String(distance_L) + " M" + String(distance_M) + " R" + String(distance_R);
  printLine(1, sensorData);

  // Threshold jarak aman
  const int safe_threshold_M = 25;
  const int safe_threshold_LR = 20;

  if (distance_M < safe_threshold_M || distance_L < safe_threshold_LR || distance_R < safe_threshold_LR) {
    printLine(0, "Halangan Detek!");
    moveStop();
    delay(200);

    // Fungsi hitung durasi belok adaptif
    auto calculateTurnDuration = [](int distance_side) {
      if (distance_side < 10) {
        return 1500;
      } else if (distance_side < 20) {
        return 1000;
      } else {
        return 700;
      }
    };

    if (distance_M < safe_threshold_M && distance_L >= safe_threshold_LR && distance_R >= safe_threshold_LR) {
      // Depan sempit, kiri-kanan aman
      moveBackward(REVERSE_SPEED_AUTO);
      delay(800);
      moveStop();
      delay(200);

      if (distance_L > distance_R) {
        int durasiBelok = calculateTurnDuration(distance_R);
        printLine(0, "Belok Kiri Adaptif");
        turnWithSpeed(LEFT, durasiBelok);
      } else {
        int durasiBelok = calculateTurnDuration(distance_L);
        printLine(0, "Belok Kanan Adaptif");
        turnWithSpeed(RIGHT, durasiBelok);
      }
    }
    else if (distance_L < safe_threshold_LR && distance_M >= safe_threshold_M) {
      // Kiri sempit, belok ke kanan
      int durasiBelok = calculateTurnDuration(distance_L);
      printLine(0, "Belok Kanan Adaptif");
      turnWithSpeed(RIGHT, durasiBelok);
    }
    else if (distance_R < safe_threshold_LR && distance_M >= safe_threshold_M) {
      // Kanan sempit, belok ke kiri
      int durasiBelok = calculateTurnDuration(distance_R);
      printLine(0, "Belok Kiri Adaptif");
      turnWithSpeed(LEFT, durasiBelok);
    }
    else {
      // Semua sempit
      printLine(0, "Mundur Jauh");
      moveBackward(REVERSE_SPEED_AUTO);
      delay(1200);
      moveStop();
      delay(200);

      if (random(0, 2) == 0) {
        int durasiBelok = calculateTurnDuration(distance_R);
        printLine(0, "Belok Kiri Acak");
        turnWithSpeed(LEFT, durasiBelok);
      } else {
        int durasiBelok = calculateTurnDuration(distance_L);
        printLine(0, "Belok Kanan Acak");
        turnWithSpeed(RIGHT, durasiBelok);
      }
    }

    currentDisplayState = NEEDS_UPDATE;
  } else {
    printLine(0, "Mode: Otomatis");
    moveForward(AUTO_FORWARD_SPEED);
  }
}



void handleButtonPress() {
  bool currentButtonState = digitalRead(buttonPin);
  if (currentButtonState != lastButtonState) {
    delay(50);
    if (digitalRead(buttonPin) == currentButtonState) {
      isAutomaticMode = (currentButtonState == LOW);
      moveStop();
      digitalWrite(pumpPin, HIGH);
      isPumpOn = false;
      isAutoRunning = false;
      pumpTimer = millis();
      lastManualActivityTime = millis();
      lcd.backlight();
      currentDisplayState = NEEDS_UPDATE;
      lastButtonState = currentButtonState;
    }
  }
}

void handleBacklightTimeout() {
  if (!isAutomaticMode) {
    if (millis() - lastManualActivityTime > backlightTimeoutDuration) {
      lcd.noBacklight();
    }
  } else {
    lcd.backlight();
  }
}

void updateDisplayIfNeeded() {
  DisplayState targetState;
  if (!isAutomaticMode) {
    targetState = MANUAL_DISPLAYED;
  } else {
    targetState = isAutoRunning ? AUTO_RUNNING_DISPLAYED : AUTO_STOPPED_DISPLAYED;
  }
  if (currentDisplayState != targetState) {
    lcd.clear();
    if (targetState == MANUAL_DISPLAYED) {
      printLine(0, "Mode: Manual");
      printLine(1, "Siap Dikontrol");
    } else if (targetState == AUTO_STOPPED_DISPLAYED) {
      printLine(0, "Mode: Otomatis");
      printLine(1, "Tekan Start");
    } else if (targetState == AUTO_RUNNING_DISPLAYED) {
      printLine(0, "Mode: Otomatis");
    }
    currentDisplayState = targetState;
  }
}

void handlePeriodicPump() {
  if (isPumpOn) {
    if (millis() - pumpTimer > pumpOnDuration) {
      digitalWrite(pumpPin, HIGH); isPumpOn = false; pumpTimer = millis();
    }
  } else {
    if (millis() - pumpTimer > pumpOffDuration) {
      digitalWrite(pumpPin, LOW); isPumpOn = true; pumpTimer = millis();
    }
  }
}

// ===================================================================
// ====================== FUNGSI-FUNGSI LEVEL BAWAH ==================
// ===================================================================

void printLine(int line, String text) {
  if (text.length() > 16) { text = text.substring(0, 16); }
  lcd.setCursor(0, line);
  lcd.print(text);
  for (int i = text.length(); i < 16; i++) {
    lcd.print(" ");
  }
}

void moveForward(int speed) { 
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  analogWrite(ENA, speed); 
  analogWrite(ENB, speed); 
}

void moveBackward(int speed) {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  analogWrite(ENA, speed); 
  analogWrite(ENB, speed);
}

void moveLeft() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  analogWrite(ENA, motor_speed); 
  analogWrite(ENB, motor_speed);
}

void moveRight() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  analogWrite(ENA, motor_speed);
  analogWrite(ENB, motor_speed);
}

void moveStop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
  analogWrite(ENA, 0); 
  analogWrite(ENB, 0);
}

void turnWithSpeed(char direction, int duration) {
  if (direction == LEFT) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  } else { // RIGHT
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  }
  analogWrite(ENA, TURN_SPEED_AUTO); 
  analogWrite(ENB, TURN_SPEED_AUTO);
  delay(duration);
  moveStop();
}

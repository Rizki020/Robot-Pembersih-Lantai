/**
 * Kode Final Robot Pembersih Lantai
 */

#include <NewPing.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// --- Download Aplikasi Via Google Play Store -> https://play.google.com/store/apps/details?id=com.giristudio.hc05.bluetooth.arduino.control
// --- Definisi Perintah Untuk Mode Manual ---
#define FORWARD 'F'   // Perintah untuk Motor bergerak ke Depan
#define BACKWARD 'B'  // Perintah untuk Motor bergerak ke Belakang
#define LEFT 'L'      // Perintah untuk Motor bergerak ke Kiri
#define RIGHT 'R'     // Perintah untuk Motor bergerak ke Kanan
#define START 'A'     // Perintah untuk MENYALAKAN Pompa
#define SQUARE 'S'    // Perintah untuk Memberhentikan Motor
#define CROSS 'X'     // Perintah untuk MEMATIKAN Pompa

// --- Deklarasi Pin ---
const int echo_L = 2, trig_L = 3; // Sensor Kiri
const int echo_M = 4, trig_M = 5; // Sensor Tengah
const int echo_R = A3, trig_R = A2; // Sensor Kanan
const int ENA = 6, IN1 = 7, IN2 = 8; // Motor Kanan
const int IN3 = 9, IN4 = 10, ENB = 11; // Motor Kiri
const int buttonPin = 12; // Tombol ganti mode
const int pumpPin = 1; // Kontrol Pompa Air 
const int BT_RX_PIN = A0; // pin BLuetooth
const int BT_TX_PIN = A1; // pin Bluetooth

// --- Variabel Global ---
int motor_speed = 150; 
const int max_distance = 200;
int distance_L, distance_M, distance_R;

// --- Variabel untuk Logika Tombol & Tampilan LCD ---
bool isAutomaticMode = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
int displayedMode = -1;

// --- Variabel untuk Timer Pompa Air ---
unsigned long pumpTimer = 0;          // Menyimpan waktu terakhir status pompa berubah
const long pumpOnDuration = 5000;   // Durasi pompa menyala: 5000 milidetik = 5 detik
const long pumpOffDuration = 10000;  // Durasi pompa mati: 3000 milidetik = 3 detik
bool isPumpOn = false;                // Menyimpan status pompa saat ini

// --- Inisialisasi Objek ---
NewPing sonar_L(trig_L, echo_L, max_distance);
NewPing sonar_M(trig_M, echo_M, max_distance);
NewPing sonar_R(trig_R, echo_R, max_distance);
LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial bluetooth(BT_RX_PIN, BT_TX_PIN);

void setup() {
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  moveStop();
  digitalWrite(pumpPin, HIGH); // HIGH = OFF untuk relay Active LOW

  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Welcome! :)");
  lcd.setCursor(0, 1); lcd.print("Robot Siap!");
  
  bluetooth.begin(9600);
  delay(2000);
}

void loop() {
  // Logika pembacaan tombol untuk ganti mode
  if (digitalRead(buttonPin) == LOW) {
    isAutomaticMode = true;
  } else {
    isAutomaticMode = false;
  }

  int currentMode = isAutomaticMode ? 1 : 0;
  if (currentMode != displayedMode) {
    moveStop(); // Hentikan Motor saat ganti mode manual
    digitalWrite(pumpPin, HIGH); // Pastikan pompa mati saat ganti mode manual
    isPumpOn = false; // Reset status pompa
    pumpTimer = millis(); // Reset timer pompa
    
    lcd.clear();
    lcd.setCursor(0, 0);
    if (isAutomaticMode) {
      lcd.print("Mode: Otomatis");
    } else {
      lcd.print("Mode: Manual");
    }
    displayedMode = currentMode;
  }
  
  if (isAutomaticMode) {
    automaticMode();
  } else {
    manualMode();
  }
}

// --- Logika Mode Manual ---
// --- Dikendalikan Melalui Aplikasi ---

void manualMode() {
  if (bluetooth.available() > 0) {
    char command = bluetooth.read();
    switch (command) {
      case FORWARD: moveForward(); lcd.setCursor(0, 1); lcd.print("Maju...         "); break;
      case BACKWARD: moveBackward(); lcd.setCursor(0, 1); lcd.print("Mundur...       "); break;
      case LEFT: moveLeft(); lcd.setCursor(0, 1); lcd.print("Belok Kiri...   "); break;
      case RIGHT: moveRight(); lcd.setCursor(0, 1); lcd.print("Belok Kanan...  "); break;
      case SQUARE: moveStop(); lcd.setCursor(0, 1); lcd.print("Berhenti.       "); break;
      case START: digitalWrite(pumpPin, LOW); lcd.setCursor(0, 1); lcd.print("Pompa ON        "); break;
      case CROSS: digitalWrite(pumpPin, HIGH); lcd.setCursor(0, 1); lcd.print("Pompa OFF       "); break;
    }
  }
}

// --- FUNGSI KONTROL POMPA BERKALA ---
void handlePeriodicPump() {
  if (isPumpOn) {
    // Jika pompa sedang ON, cek apakah sudah waktunya untuk MATI
    if (millis() - pumpTimer > pumpOnDuration) {
      digitalWrite(pumpPin, HIGH); // Matikan pompa (HIGH = OFF)
      isPumpOn = false;
      pumpTimer = millis(); // Reset timer untuk durasi OFF
    }
  } else {
    // Jika pompa sedang OFF, cek apakah sudah waktunya untuk NYALA
    if (millis() - pumpTimer > pumpOffDuration) {
      digitalWrite(pumpPin, LOW); // Nyalakan pompa (LOW = ON)
      isPumpOn = true;
      pumpTimer = millis(); // Reset timer untuk durasi ON
    }
  }
}

// --- FUNGSI MODE OTOMATIS ---
void automaticMode() {

  handlePeriodicPump(); // <-- PANGGIL FUNGSI KONTROL POMPA DI SINI

  // Logika sensor dan gerakan motor 
  distance_L = sonar_L.ping_cm(); if(distance_L == 0) distance_L = max_distance; delay(50);
  distance_M = sonar_M.ping_cm(); if(distance_M == 0) distance_M = max_distance; delay(50);
  distance_R = sonar_R.ping_cm(); if(distance_R == 0) distance_R = max_distance; delay(50);
  
  lcd.setCursor(0, 1);
  lcd.print("L"); lcd.print(distance_L); lcd.print(" M"); lcd.print(distance_M);
  lcd.print(" R"); lcd.print(distance_R); lcd.print("    ");

  if (distance_M > 20) {
    moveForward();
  } else {
    moveStop(); delay(300);
    if (distance_L > distance_R) {
      moveBackward(); delay(400); moveLeft(); delay(500);
    } else {
      moveBackward(); delay(400); moveRight(); delay(500);
    }
  }
}

// --- FUNGSI-FUNGSI KONTROL GERAKAN MOTOR ---

void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}
void moveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}
void moveLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}
void moveRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}
void moveStop() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
}
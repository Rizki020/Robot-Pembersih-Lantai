/**
 * Kode Final Robot Pembersih Lantai
 * Versi 8 Kali Revisi- CAPEK BANG !!!
 * - Alat seperti robot ini memiliki dua mode utama: Manual dan Otomatis.
 * - Mode diganti menggunakan tombol fisik (latching switch).
 * - Mode Manual: Dikendalikan penuh via aplikasi Bluetooth.
 * - Mode Otomatis: Robot bergerak sendiri menghindari rintangan.
 * - Membutuhkan perintah 'Start' dari aplikasi untuk mulai bergerak.
 * - Dapat dihentikan dengan perintah 'Stop' dari aplikasi.
 * - Meletakkan komentar pada setiap kode agar mudah dipahami.
 */

// ===================================================================
// ======================== BAGIAN PENGATURAN AWAL ===================
// ===================================================================

// --- Library yang Digunakan ---
#include <NewPing.h>          // Library untuk mempermudah penggunaan sensor ultrasonic HC-SR04.
#include <LiquidCrystal_I2C.h> // Library untuk mengontrol LCD melalui koneksi I2C.
#include <SoftwareSerial.h>   // Library untuk membuat port serial tambahan (untuk Bluetooth).

// --- Definisi Perintah Aplikasi Bluetooth ---
// --- Download Aplikasi Via Google Play Store -> https://play.google.com/store/apps/details?id=com.giristudio.hc05.bluetooth.arduino.control
// Menetapkan karakter-karakter yang dikirim oleh aplikasi ke fungsi-fungsi robot.
#define FORWARD 'F'     // Perintah untuk Motor bergerak ke Depan
#define BACKWARD 'B'    // Perintah untuk Motor bergerak ke Belakang
#define LEFT 'L'        // Perintah untuk Motor bergerak ke Kiri
#define RIGHT 'R'       // Perintah untuk Motor bergerak ke Kanan
#define SQUARE 'S'      // Perintah untuk Memberhentikan Motor (di Manual & Otomatis)
#define START 'A'       // Perintah untuk Menyalakan Pompa (di Manual) & Mulai (di Otomatis)
#define PAUSE 'P'       // Perintah untuk Mematikan Pompa
#define TRIANGLE 'T'    // Perintah untuk Menambah Kecepatan Motor
#define CROSS 'X'       // Perintah untuk Mengurangi Kecepatan Motor

// --- Deklarasi Pin Hardware ---
// Sensor Jarak Ultrasonic
const int echo_L = 2, trig_L = 3;   // Sensor Kiri
const int echo_M = 4, trig_M = 5;   // Sensor Tengah
const int echo_R = A3, trig_R = A2; // Sensor Kanan

// Driver Motor L298N
const int ENA = 6, IN1 = 7, IN2 = 8;    // Pin-pin untuk mengontrol Motor Kanan (Enable A, Input 1, Input 2)
const int IN3 = 9, IN4 = 10, ENB = 11;  // Pin-pin untuk mengontrol Motor Kiri (Input 3, Input 4, Enable B)

// Komponen Lain
const int buttonPin = 12;               // Pin untuk tombol fisik pengganti mode (latching switch)
const int pumpPin = 1;                  // Pin untuk mengontrol relay pompa air (Relay Active LOW)
const int BT_RX_PIN = A0;               // Pin RX Bluetooth (menerima data), terhubung ke TX modul HC-05
const int BT_TX_PIN = A1;               // Pin TX Bluetooth (mengirim data), terhubung ke RX modul HC-05

// --- Variabel Global ---
// Variabel yang bisa diakses dari seluruh bagian kode.
int motor_speed = 100;              // Kecepatan awal dan default motor (nilai 0-255).
const int max_distance = 200;       // Jarak maksimum yang akan dideteksi sensor (dalam cm).
int distance_L, distance_M, distance_R; // Variabel untuk menyimpan hasil bacaan jarak sensor.

// --- Variabel Logika Mode ---
// Variabel yang mengatur state atau kondisi robot saat ini.
bool isAutomaticMode = false;       // Penanda mode utama. 'true' jika Otomatis, 'false' jika Manual.
bool isAutoRunning = false;         // Penanda sub-mode Otomatis. 'true' jika sedang berjalan, 'false' jika sedang diam.
bool lastButtonState = HIGH;        // Menyimpan status terakhir tombol untuk mendeteksi perubahan.

// --- Variabel Kontrol Tampilan & Pompa ---
enum DisplayState { NEEDS_UPDATE, MANUAL_DISPLAYED, AUTO_STOPPED_DISPLAYED, AUTO_RUNNING_DISPLAYED };
DisplayState currentDisplayState = NEEDS_UPDATE; // Melacak apa yang ditampilkan di LCD untuk mencegah flicker (kedipan).

unsigned long pumpTimer = 0;        // Menyimpan waktu terakhir pompa berganti status (untuk timer).
const long pumpOnDuration = 5000;   // Durasi pompa menyala dalam milidetik (5 detik).
const long pumpOffDuration = 10000; // Durasi pompa mati dalam milidetik (10 detik).
bool isPumpOn = false;              // Menyimpan status pompa saat ini (ON atau OFF).
unsigned long lastManualActivityTime = 0; // Menyimpan waktu aktivitas manual terakhir
const long backlightTimeoutDuration = 60000; // Durasi timeout 1 menit (60000 milidetik)

// --- Inisialisasi Objek Library ---
// Membuat objek dari library untuk digunakan dalam kode.
NewPing sonar_L(trig_L, echo_L, max_distance);
NewPing sonar_M(trig_M, echo_M, max_distance);
NewPing sonar_R(trig_R, echo_R, max_distance);
LiquidCrystal_I2C lcd(0x27, 16, 2);      // LCD dengan alamat I2C 0x27, ukuran 16x2.
SoftwareSerial bluetooth(BT_RX_PIN, BT_TX_PIN); // Membuat port serial virtual untuk Bluetooth.


// ===================================================================
// ========================== FUNGSI UTAMA ===========================
// ===================================================================

/**
 * @brief Fungsi setup() yang hanya berjalan satu kali saat Arduino pertama kali dinyalakan.
 * Digunakan untuk inisialisasi awal.
 */
void setup() {
  // Mengatur mode pin (apakah sebagai INPUT atau OUTPUT).
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // Tombol menggunakan resistor pull-up internal Arduino.

  // Kondisi awal robot.
  moveStop();                       // Pastikan motor berhenti saat pertama kali nyala.
  digitalWrite(pumpPin, HIGH);      // Matikan pompa (Relay Active LOW, jadi HIGH = Mati).

  // Inisialisasi komponen eksternal.
  lcd.init();                       // Mulai komunikasi dengan LCD.
  lcd.backlight();                  // Nyalakan lampu latar LCD.
  printLine(0, "Robot Siap!");      // Tampilkan pesan selamat datang.
  delay(2000);                      // Jeda 2 detik.

  bluetooth.begin(9600);            // Mulai komunikasi serial dengan Bluetooth pada baud rate 9600.
  
  // Baca kondisi awal tombol untuk menentukan mode awal.
  lastButtonState = digitalRead(buttonPin);
  isAutomaticMode = (lastButtonState == LOW);

  lastManualActivityTime = millis(); //Inisialisasi timer backlight LCD
}

/**
 * @brief Fungsi loop() yang berjalan terus-menerus setelah setup() selesai.
 * Ini adalah jantung dari program.
 */
void loop() {
  // 1. Cek apakah ada penekanan tombol fisik untuk ganti mode.
  handleButtonPress();

  // 2. Cek apakah ada perintah masuk dari Bluetooth dan proses sesuai mode saat ini.
  handleBluetoothCommands();

  // 3. Update tampilan LCD jika ada perubahan status.
  updateDisplayIfNeeded();
  handleBacklightTimeout();

  // 4. Jalankan logika utama berdasarkan mode.
  if (isAutomaticMode && isAutoRunning) {
    // Hanya jalankan logika otomatis jika mode otomatis DAN sedang berjalan.
    automaticMode();
  }
}


// ===================================================================
// ======================= FUNGSI-FUNGSI LOGIKA ======================
// ===================================================================

/**
 * Mengontrol backlight agar mati otomatis saat tidak ada aktivitas.
 */
void handleBacklightTimeout() {
  // Fitur ini hanya aktif di mode manual.
  if (!isAutomaticMode) {
    // Cek apakah sudah 10 detik sejak aktivitas manual terakhir.
    if (millis() - lastManualActivityTime > backlightTimeoutDuration) {
      lcd.noBacklight(); // Jika ya, matikan lampu latar.
    }
  } else {
    // Jika dalam mode otomatis, pastikan backlight selalu menyala.
    lcd.backlight();
  }
}

/**
 * @brief Fungsi untuk menangani penekanan tombol fisik (latching).
 * Saat tombol ditekan, semua state di-reset dan mode diganti.
 */
void handleButtonPress() {
  bool currentButtonState = digitalRead(buttonPin);
  if (currentButtonState != lastButtonState) { // Cek jika ada perubahan dari status sebelumnya.
    delay(50); // Jeda singkat untuk anti-bouncing (getaran tombol).
    if (digitalRead(buttonPin) == currentButtonState) { // Konfirmasi lagi setelah jeda.
      isAutomaticMode = (currentButtonState == LOW); // Jika tombol ditekan (LOW), mode jadi Otomatis.
      // Reset semua kondisi ke awal.
      moveStop();
      digitalWrite(pumpPin, HIGH);
      isPumpOn = false;
      isAutoRunning = false;
      pumpTimer = millis();
      lastManualActivityTime = millis();
      lcd.backlight(); // Pastikan backlight nyala saat ganti mode
      currentDisplayState = NEEDS_UPDATE; // Beri tanda agar LCD di-refresh.
      lastButtonState = currentButtonState; // Simpan status tombol yang baru.
    }
  }
}

/**
 * @brief Pusat komando untuk semua perintah Bluetooth.
 * Fungsi ini menjadi satu-satunya yang membaca dari Bluetooth untuk mencegah 'rebutan' data.
 */
void handleBluetoothCommands() {
  if (bluetooth.available() > 0) { // Cek apakah ada data yang masuk.
    char command = bluetooth.read(); // Baca data tersebut.

    if (isAutomaticMode) {
      // --- Logika untuk Mode Otomatis ---
      if (!isAutoRunning && command == START) {
        // Jika robot sedang DIAM dan menerima perintah START ('A')...
        isAutoRunning = true; // ...ubah status menjadi BERJALAN.
        pumpTimer = millis(); // ...mulai timer pompa dari awal.
        currentDisplayState = NEEDS_UPDATE; // ...minta refresh LCD.
      } else if (isAutoRunning && command == SQUARE) {
        // Jika robot sedang BERJALAN dan menerima perintah STOP ('S')...
        isAutoRunning = false; // ...ubah status menjadi DIAM.
        moveStop(); // ...hentikan motor.
        digitalWrite(pumpPin, HIGH); // ...matikan pompa.
        isPumpOn = false;
        pumpTimer = millis();
        currentDisplayState = NEEDS_UPDATE;
      }
    } else {
      // --- Logika untuk Mode Manual ---
      lcd.backlight(); // Nyalakan kembali backlight setiap ada aktivitas.
      lastManualActivityTime = millis(); // Reset timer setiap ada aktivitas
      String msg = ""; // Variabel untuk menyimpan pesan yang akan ditampilkan di LCD.
      switch (command) {
        case FORWARD:  moveForward();  msg = "Maju..."; break;
        case BACKWARD: moveBackward(); msg = "Mundur..."; break;
        case LEFT:     moveLeft();     msg = "Belok Kiri..."; break;
        case RIGHT:    moveRight();    msg = "Belok Kanan..."; break;
        case SQUARE:   moveStop();     msg = "Berhenti."; break;
        case START:    digitalWrite(pumpPin, LOW);  msg = "Pompa ON"; break;
        case PAUSE:    digitalWrite(pumpPin, HIGH); msg = "Pompa OFF"; break;
        case TRIANGLE: 
          motor_speed = min(255, motor_speed + 10); // Tambah kecepatan, maks 255.
          analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
          msg = "Speed: " + String(motor_speed);
          break;
        case CROSS:    
          motor_speed = max(50, motor_speed - 10); // Kurangi kecepatan, min 50.
          analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
          msg = "Speed: " + String(motor_speed);
          break;
      }
      if (msg != "") {
        printLine(1, msg); // Tampilkan pesan status di baris kedua LCD.
      }
    }
  }
}

/**
 * @brief Mengupdate tampilan LCD hanya jika ada perubahan state robot.
 * Ini adalah kunci untuk mencegah layar berkedip (flicker).
 */
void updateDisplayIfNeeded() {
  DisplayState targetState; // Tentukan state tampilan yang seharusnya.
  if (!isAutomaticMode) {
    targetState = MANUAL_DISPLAYED;
  } else {
    targetState = isAutoRunning ? AUTO_RUNNING_DISPLAYED : AUTO_STOPPED_DISPLAYED;
  }

  // Bandingkan state yang seharusnya dengan yang sedang ditampilkan.
  if (currentDisplayState != targetState) {
    lcd.clear(); // HANYA bersihkan layar jika ada perubahan besar.
    if (targetState == MANUAL_DISPLAYED) {
      printLine(0, "Mode: Manual");
      printLine(1, "Siap Dikontrol");
    } else if (targetState == AUTO_STOPPED_DISPLAYED) {
      printLine(0, "Mode: Otomatis");
      printLine(1, "Tekan Start");
    } else if (targetState == AUTO_RUNNING_DISPLAYED) {
      printLine(0, "Mode: Otomatis");
    }
    currentDisplayState = targetState; // Catat state yang baru saja ditampilkan.
  }
}

/**
 * @brief Fungsi yang berisi logika utama robot saat berjalan di mode otomatis.
 * Menghindari rintangan berdasarkan data sensor.
 */
void automaticMode() {
  // 1. Jalankan logika pompa periodik.
  handlePeriodicPump();

  // 2. Baca jarak dari ketiga sensor.
  distance_L = sonar_L.ping_cm(); if (distance_L == 0) distance_L = max_distance;
  delay(30);
  distance_M = sonar_M.ping_cm(); if (distance_M == 0) distance_M = max_distance;
  delay(30);
  distance_R = sonar_R.ping_cm(); if (distance_R == 0) distance_R = max_distance;
  
  // 3. Tampilkan data sensor ke LCD (hanya jika sedang berjalan).
  if(currentDisplayState == AUTO_RUNNING_DISPLAYED){
    String sensorData = "L" + String(distance_L) + " M" + String(distance_M) + " R" + String(distance_R);
    printLine(1, sensorData);
  }

  // 4. Logika pengambilan keputusan gerak.
  if (distance_M > 20) {
    // Jika jalan di depan lapang (lebih dari 20cm), maju terus.
    moveForward();
  } else {
    // Jika ada halangan di depan...
    moveStop(); delay(200);     // Berhenti sejenak.
    moveBackward(); delay(400); // Mundur sedikit.
    // Cek sensor kiri dan kanan untuk menentukan arah belok.
    if (distance_L > distance_R) {
      moveLeft(); delay(500);   // Belok ke kiri jika lebih lapang.
    } else {
      moveRight(); delay(500);  // Belok ke kanan jika lebih lapang.
    }
  }
}

/**
 * @brief Fungsi untuk menyalakan dan mematikan pompa secara berkala menggunakan timer non-blocking.
 */
void handlePeriodicPump() {
  if (isPumpOn) {
    // Jika pompa sedang NYALA, cek apakah sudah waktunya untuk MATI.
    if (millis() - pumpTimer > pumpOnDuration) {
      digitalWrite(pumpPin, HIGH); isPumpOn = false; pumpTimer = millis();
    }
  } else {
    // Jika pompa sedang MATI, cek apakah sudah waktunya untuk NYALA.
    if (millis() - pumpTimer > pumpOffDuration) {
      digitalWrite(pumpPin, LOW); isPumpOn = true; pumpTimer = millis();
    }
  }
}

// ===================================================================
// ====================== FUNGSI-FUNGSI LEVEL BAWAH ==================
// ===================================================================

/**
 * @brief Fungsi cerdas untuk mencetak teks ke satu baris LCD.
 * Secara otomatis membersihkan sisa baris untuk mencegah karakter sampah.
 * @param line Baris tujuan (0 untuk baris pertama, 1 untuk baris kedua).
 * @param text Teks yang ingin ditampilkan.
 */
void printLine(int line, String text) {
  if (text.length() > 16) { text = text.substring(0, 16); } // Potong teks jika lebih dari 16 karakter.
  lcd.setCursor(0, line);
  lcd.print(text);
  // Cetak spasi untuk mengisi sisa baris.
  for (int i = text.length(); i < 16; i++) {
    lcd.print(" ");
  }
}

// --- FUNGSI-FUNGSI KONTROL GERAKAN MOTOR ---
// Mengatur kombinasi HIGH/LOW pada pin IN1-IN4 untuk mengarahkan motor.

void moveForward() { 
  // Kanan Maju, Kiri Maju
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed); 
}

void moveBackward() {
  // Kanan Mundur, Kiri Mundur
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}

void moveLeft() {
  // Kanan Maju, Kiri Mundur (Berputar ke kiri)
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}

void moveRight() {
  // Kanan Mundur, Kiri Maju (Berputar ke kanan)
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
  analogWrite(ENA, motor_speed); analogWrite(ENB, motor_speed);
}

void moveStop() {
  // Matikan kedua motor.
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
  analogWrite(ENA, 0); analogWrite(ENB, 0);
}

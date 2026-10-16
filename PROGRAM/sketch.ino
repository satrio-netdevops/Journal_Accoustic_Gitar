#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define MODE_UJI 0   // 1 = 17 titik uji otomatis (untuk dibandingkan dengan MATLAB)
                     // 0 = baca sensor DHT22 (geser slider di Wokwi)

// ---------------- Pin ----------------
#define PIN_DHT 4
const int PIN_LED[3] = {25, 26, 27};            // Humidifier, Dehumidifier, Kipas
const char* NAMA_AKT[3] = {"Hum", "Deh", "Kpi"};

// ---------------- Konstanta desain ----------------
const float DEADBAND = 10.0f;    // % (asumsi desain)
const float ALARM_C  = 40.6f;    // ambang alarm suhu (Taylor), di luar aturan fuzzy

// ==== INTI FUZZY (mulai) ====
float yMin = 0, yMax = 100;
float muOut[3][101];             // himpunan keluaran, sampel x = 0..100

float trapmf(float x, float a, float b, float c, float d) {
  float l = (x >= b) ? 1.0f : ((b > a) ? (x - a) / (b - a) : 0.0f);
  float r = (x <= c) ? 1.0f : ((d > c) ? (d - x) / (d - c) : 0.0f);
  float m = fminf(l, r);
  return (m < 0.0f) ? 0.0f : m;
}
float trimf(float x, float a, float b, float c) { return trapmf(x, a, b, b, c); }

// Rule base: {suhu, RH, humidifier, dehumidifier, kipas}, indeks mulai 0
// Suhu: 0=Rendah 1=Normal 2=Tinggi | RH: 0=Kering 1=Ideal 2=Lembab
// Output: 0=Rendah 1=Sedang 2=Tinggi
const uint8_t RULE[9][5] = {
  {0,0, 1,0,0},  // R1
  {0,1, 0,0,0},  // R2
  {0,2, 0,2,1},  // R3
  {1,0, 2,0,0},  // R4
  {1,1, 0,0,0},  // R5
  {1,2, 0,2,1},  // R6
  {2,0, 2,0,1},  // R7
  {2,1, 0,0,2},  // R8
  {2,2, 0,2,2}   // R9
};

void inisialisasiOutput() {
  for (int i = 0; i <= 100; i++) {
    muOut[0][i] = trapmf(i, 0, 0, 25, 50);
    muOut[1][i] = trimf(i, 25, 50, 75);
    muOut[2][i] = trapmf(i, 50, 75, 100, 100);
  }
}

// y[0..2] = centroid mentah Humidifier, Dehumidifier, Kipas
void inferensi(float suhu, float rh, float y[3]) {
  suhu = fminf(fmaxf(suhu, 0.0f), 50.0f);   // jepit ke semesta input
  rh   = fminf(fmaxf(rh,   0.0f), 100.0f);
  float ms[3] = { trapmf(suhu, 0, 0, 18, 22), trapmf(suhu, 18, 22, 25, 35), trapmf(suhu, 25, 35, 50, 50) };
  float mr[3] = { trapmf(rh, 0, 0, 40, 45), trapmf(rh, 40, 45, 55, 60), trapmf(rh, 55, 60, 100, 100) };
  float w[9];
  for (int r = 0; r < 9; r++) w[r] = fminf(ms[RULE[r][0]], mr[RULE[r][1]]);   // AND = min
  for (int k = 0; k < 3; k++) {
    float num = 0, den = 0;
    for (int i = 0; i <= 100; i++) {
      float agg = 0;
      for (int r = 0; r < 9; r++)                                              // implikasi = min, agregasi = max
        agg = fmaxf(agg, fminf(w[r], muOut[RULE[r][2 + k]][i]));
      num += i * agg;  den += agg;
    }
    y[k] = (den > 0) ? num / den : 0.0f;                                       // centroid
  }
}

float kePWM(float y) {
  float p = (y - yMin) / (yMax - yMin) * 100.0f;
  p = fminf(fmaxf(p, 0.0f), 100.0f);
  if (p < DEADBAND) p = 0;
  return p;
}

void kalibrasi() {
  float y[3];
  inferensi(24, 50, y);  yMin = y[0];   // centroid himpunan Rendah
  inferensi(40, 80, y);  yMax = y[1];   // centroid himpunan Tinggi
}
// ==== INTI FUZZY (selesai) ====

// ---------------- Perangkat ----------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(PIN_DHT, DHT22);

void pwmInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  for (int k = 0; k < 3; k++) ledcAttach(PIN_LED[k], 5000, 8);
#else
  for (int k = 0; k < 3; k++) { ledcSetup(k, 5000, 8); ledcAttachPin(PIN_LED[k], k); }
#endif
}
void pwmTulis(int k, float persen) {
  int duty = (int)roundf(persen * 2.55f);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_LED[k], duty);
#else
  ledcWrite(k, duty);
#endif
}

void tampilOLED(float t, float rh, const float pwm[3], const char* info, bool alarm) {
  char buf[24];
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  snprintf(buf, sizeof buf, "T %.1fC  RH %.1f%%", t, rh);
  display.setCursor(0, 0);  display.print(buf);
  for (int k = 0; k < 3; k++) {
    int y0 = 14 + k * 12;
    display.setCursor(0, y0);  display.print(NAMA_AKT[k]);
    display.drawRect(24, y0, 70, 8, SSD1306_WHITE);
    display.fillRect(24, y0, (int)(pwm[k] * 0.70f), 8, SSD1306_WHITE);
    snprintf(buf, sizeof buf, "%3d%%", (int)roundf(pwm[k]));
    display.setCursor(98, y0);  display.print(buf);
  }
}

void proses(float t, float rh, const char* label, const char* info) {
  float y[3], pwm[3];
  inferensi(t, rh, y);
  for (int k = 0; k < 3; k++) { pwm[k] = kePWM(y[k]); pwmTulis(k, pwm[k]); }
  bool alarm = (t >= ALARM_C);
  Serial.printf("%s,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                label, t, rh, y[0], y[1], y[2], pwm[0], pwm[1], pwm[2]);
  if (alarm) Serial.println("# ALARM: suhu >= 40.6 C");
  tampilOLED(t, rh, pwm, info, alarm);
}

// ---------------- Titik uji (sama dengan Tabel_uji.csv MATLAB) ----------------
const int N_UJI = 17;
const float UJI[N_UJI][2] = {
  {15,30},{15,50},{15,80},{23.5,30},{23.5,50},{23.5,80},{40,30},{40,50},{40,80},
  {24,42.5},{24,57.5},{20,50},{30,50},{27,52},{20,42},{30,35},{30,65}
};
const char* LABEL[N_UJI] = {
  "Inti-R1","Inti-R2","Inti-R3","Inti-R4","Inti-R5","Inti-R6","Inti-R7","Inti-R8","Inti-R9",
  "Transisi-1","Transisi-2","Transisi-3","Transisi-4","Transisi-5","Transisi-6","Transisi-7","Transisi-8"
};

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("# OLED tidak ditemukan");
  pwmInit();
  dht.begin();
  inisialisasiOutput();
  kalibrasi();
  Serial.printf("# y_min = %.3f | y_max = %.3f | dead-band = %.0f %%\n", yMin, yMax, DEADBAND);
  Serial.println("Kasus,Suhu,RH,y_Hum,y_Deh,y_Kipas,PWM_Hum,PWM_Deh,PWM_Kipas");
}

void loop() {
#if MODE_UJI
  static int idx = 0;
  if (idx < N_UJI) {
    char info[24];
    snprintf(info, sizeof info, "UJI %d/%d %s", idx + 1, N_UJI, LABEL[idx]);
    proses(UJI[idx][0], UJI[idx][1], LABEL[idx], info);
    idx++;
    delay(3000);
  } else {
    if (idx == N_UJI) { Serial.println("# SELESAI"); idx++; }
    delay(1000);
  }
#else
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("# Gagal membaca DHT22");
  } else {
    proses(t, h, "Sensor", "MODE SENSOR DHT22");
  }
  delay(2000);   // DHT22 maks. ~0,5 Hz
#endif
}

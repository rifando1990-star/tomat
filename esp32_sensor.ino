#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <Wire.h>
#include <BH1750.h>

const char* WIFI_SSID     = "nama wifi";
const char* WIFI_PASSWORD = "kata sandi wifi";

// --- PERUBAHAN 1: Menyesuaikan dengan IP Raspberry Pi dan file PHP ---
// --- cara cek IP raspberry dengan "hostname -I" ---
const char* SERVER_URL = "http://IPraspberry/kirim_data.php";

// Jeda pengiriman ke database (milidetik).
const unsigned long KIRIM_INTERVAL_MS = 10000;

unsigned long terakhirKirim = 0;

// ==================================================
// PIN ESP32
// ==================================================

#define SOIL_PIN 34       // AO Soil Moisture (GPIO 34 = ADC1, aman dipakai saat WiFi menyala)
#define DHTPIN 15         // DATA DHT22
#define DHTTYPE DHT22

#define I2C_SDA 21        // SDA BH1750
#define I2C_SCL 22        // SCL BH1750

// ==================================================
// SENSOR
// ==================================================

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

// ==================================================
// KONFIGURASI SOIL
// ==================================================

const int AVG_SAMPLES = 8;
const int SAMPLE_DELAY_MS = 60;

// ==================================================
// BACA SOIL MOISTURE
// ==================================================

int readSoilAverage() {
  long sum = 0;
  for (int i = 0; i < AVG_SAMPLES; i++) {
    sum += analogRead(SOIL_PIN);
    delay(SAMPLE_DELAY_MS);
  }
  return sum / AVG_SAMPLES;
}

// ==================================================
// KONVERSI ADC KE PERSENTASE
// ==================================================

float soilToPercent(int adc) {
  if (adc <= 900)  return 100;
  if (adc <= 1499) return 85;
  if (adc <= 1799) return 70;
  if (adc <= 1999) return 55;
  if (adc <= 2299) return 45;
  if (adc <= 2400) return 20;
  return 0;
}

// ==================================================
// KATEGORI TANAH
// ==================================================

String categorizeSoil(float p) {
  if (p == 100) return "SANGAT BASAH / BANJIR";
  if (p == 85)  return "BASAH";
  if (p == 70)  return "LEMBAB";
  if (p == 55)  return "NORMAL";
  if (p == 45)  return "NORMAL-MENGERING";
  if (p == 20)  return "KERING";
  return "SANGAT KERING";
}

// ==================================================
// KATEGORI CAHAYA
// ==================================================

String categorizeLight(float lux) {
  if (lux < 3000)  return "GELAP / REDUP";
  if (lux <= 6000) return "OPTIMAL";
  return "TERLALU TERANG";
}

// ==================================================
// SAMBUNG WIFI
// ==================================================

void connectWiFi() {
  Serial.print("Menyambung WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int percobaan = 0;
  while (WiFi.status() != WL_CONNECTED && percobaan < 30) {
    delay(500);
    Serial.print(".");
    percobaan++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi tersambung, IP ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi GAGAL tersambung (cek nama, password, dan pastikan WiFi 2.4 GHz)");
  }
}

// ==================================================
// KIRIM DATA KE DATABASE (LEWAT PHP)
// ==================================================

void kirimData(float suhu, float kelembapanUdara, float kelembapanTanah, float cahaya) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("KIRIM: WiFi terputus, mencoba menyambung ulang...");
    WiFi.disconnect();
    connectWiFi();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("KIRIM: dibatalkan, WiFi belum tersambung");
      return;
    }
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // --- PERUBAHAN 2: Menyesuaikan parameter cahaya agar terbaca di PHP ---
  String data = "suhu=" + String(suhu, 2) +
                "&kelembapan_udara=" + String(kelembapanUdara, 2) +
                "&kelembapan_tanah=" + String(kelembapanTanah, 0) +
                "&cahaya=" + String(cahaya, 2);

  int kode = http.POST(data);

  Serial.print("KIRIM: kode balasan = ");
  Serial.println(kode);   // 200 = berhasil

  if (kode > 0) {
    Serial.print("KIRIM: jawaban server = ");
    Serial.println(http.getString());   // Harus menampilkan pesan sukses dari PHP
  }

  http.end();
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("         TEST SENSOR ESP32");
  Serial.println(" Soil Moisture + DHT22 + BH1750");
  Serial.println("==========================================");

  analogReadResolution(12);

  pinMode(SOIL_PIN, INPUT);
  Serial.print("SOIL AO  : GPIO ");
  Serial.println(SOIL_PIN);

  dht.begin();
  Serial.println("DHT22    : OK");

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.print("I2C SDA  : GPIO ");
  Serial.println(I2C_SDA);
  Serial.print("I2C SCL  : GPIO ");
  Serial.println(I2C_SCL);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750   : OK");
  } else {
    Serial.println("BH1750   : GAGAL TERDETEKSI");
  }

  connectWiFi();

  Serial.println("------------------------------------------");
  Serial.println("Sensor mulai membaca...");
  Serial.println("------------------------------------------");
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  int soilAdc = readSoilAverage();
  float soilVoltage = soilAdc * (3.3 / 4095.0);
  float soilPercent = soilToPercent(soilAdc);
  String soilCondition = categorizeSoil(soilPercent);

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float lux = lightMeter.readLightLevel();

  Serial.println();
  Serial.println("============== DATA SENSOR ==============");

  Serial.println("\n[ SOIL MOISTURE ]");
  Serial.print("ADC        : "); Serial.println(soilAdc);
  Serial.print("Voltage    : "); Serial.print(soilVoltage, 3); Serial.println(" V");
  Serial.print("Persentase : "); Serial.print(soilPercent, 0); Serial.println(" %");
  Serial.print("Kondisi    : "); Serial.println(soilCondition);

  Serial.println("\n[ DHT22 ]");
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("ERROR: DHT22 tidak terbaca!");
  } else {
    Serial.print("Suhu       : "); Serial.print(temperature, 2); Serial.println(" °C");
    Serial.print("Kelembapan : "); Serial.print(humidity, 2); Serial.println(" %");
  }

  Serial.println("\n[ BH1750 ]");
  if (lux < 0) {
    Serial.println("ERROR: BH1750 tidak terbaca!");
  } else {
    Serial.print("Cahaya     : "); Serial.print(lux, 2); Serial.println(" lux");
    Serial.print("Kondisi    : "); Serial.println(categorizeLight(lux));
  }

  Serial.println();
  bool dataValid = !(isnan(temperature) || isnan(humidity)) && (lux >= 0);

  if (millis() - terakhirKirim >= KIRIM_INTERVAL_MS) {
    if (dataValid) {
      kirimData(temperature, humidity, soilPercent, lux);
      terakhirKirim = millis();
    } else {
      Serial.println("KIRIM: dilewati, ada sensor yang gagal terbaca");
    }
  }

  Serial.println("\n==========================================");
  delay(2000);
}ini merupakan codingan espnya

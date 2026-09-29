# TomatoCare IoT: Monitoring Sensor Tanaman Tomat

Sistem pemantauan kondisi tanaman tomat. ESP32 membaca sensor, mengirim data lewat WiFi ke Raspberry Pi, disimpan di database MariaDB, lalu ditampilkan real-time di sidebar.

## Alur sistem

ESP32 membaca sensor, lalu mengirim HTTP POST ke Apache di Raspberry Pi (kirim_data.php), yang menyimpan ke MariaDB (tomat_db), dan Streamlit membaca database lalu menampilkannya di sidebar.

## Daftar file

| File | Fungsi | Diletakkan di |
|---|---|---|
| esp32/esp32_sensor.ino | Membaca 3 sensor dan mengirim data ke Raspberry | Diunggah ke ESP32 |
| kirim_data.php | Menerima data dari ESP32 dan menyimpan ke database | /var/www/html/ di Raspberry Pi |
| sidebar_sensor_realtime.py | Menampilkan data sensor real-time di sidebar Streamlit | Potongan yang ditambahkan ke webapp/app.py |

Catatan: password WiFi dan database di repo ini sudah diganti placeholder. Isi dengan nilai asli sebelum dipakai.

## 1. Kode ESP32 (esp32_sensor.ino)

Membaca tiga sensor dan mengirim hasilnya ke Raspberry.

- Soil moisture (GPIO 34): dibaca 8 kali lalu dirata-rata agar stabil, kemudian dikonversi ke persen bertingkat (100, 85, 70, 55, 45, 20, 0).
- DHT22 (GPIO 15): suhu dan kelembapan udara.
- BH1750 (SDA 21, SCL 22): intensitas cahaya dalam lux.
- Data dikirim tiap 10 detik (KIRIM_INTERVAL_MS) lewat HTTP POST dengan field suhu, kelembapan_udara, kelembapan_tanah, dan cahaya.
- Jika ada sensor yang gagal terbaca, pengiriman dilewati agar data rusak tidak masuk database.
- Jika WiFi terputus, ESP32 mencoba menyambung ulang sebelum mengirim.
- ESP32 hanya mendukung WiFi 2.4 GHz.

## 2. Penerima data (kirim_data.php)

Berjalan di Apache pada Raspberry Pi. Setiap ESP32 mengirim data, skrip ini bekerja dalam lima tahap:

1. Membuka koneksi ke database tomat_db dan memastikan metode request adalah POST.
2. Mengambil empat nilai sensor dan memvalidasi bahwa semuanya angka.
3. Menimpa tabel sensor_terkini (satu baris) untuk data real-time.
4. Menambahkan nilai ke tabel sensor_akumulator (jumlah data dan total tiap sensor).
5. Jika sudah 300 detik (5 menit), menghitung rata-rata (total dibagi jumlah data), menyimpannya ke tabel data_sensor, lalu mereset akumulator.

Kode balasan: 200 berhasil, 400 data tidak lengkap atau bukan angka, 405 bukan POST, 500 koneksi database gagal.

## 3. Tampilan sidebar (sidebar_sensor_realtime.py)

Potongan kode yang ditambahkan ke webapp/app.py (aplikasi Streamlit). Tidak bisa dijalankan sendirian. Butuh SENSORS dan record_sensor_only() dari app.py.

Bagian 1, penarik data (get_data_sensor_asli): membuka koneksi ke MariaDB, mengambil satu baris dari sensor_terkini, dan mengembalikan nol semua jika tabel kosong atau terjadi error. Koneksi selalu ditutup di blok finally.

Bagian 2, tampilan sidebar (panel_sensor_realtime):
- Memakai st.fragment(run_every=2) sehingga panel diperbarui otomatis tiap 2 detik tanpa me-reload seluruh aplikasi.
- URUTAN_DB memetakan nama sensor aplikasi ke urutan kolom hasil query.
- Nilai ditampilkan dengan st.metric dan disimpan ke session_state agar bagian lain aplikasi bisa memakainya.
- Jika semua nilai 0, muncul peringatan dan tombol Catat pembacaan sensor dinonaktifkan.

## Struktur database (tomat_db)

| Tabel | Isi |
|---|---|
| sensor_terkini | Satu baris berisi pembacaan terbaru, ditimpa setiap data masuk |
| sensor_akumulator | Jumlah data dan total tiap sensor untuk perhitungan rata-rata 5 menit |
| data_sensor | Riwayat rata-rata tiap 5 menit |

## Perangkat

- ESP32, sensor DHT22, soil moisture, BH1750
- Raspberry Pi 4B (Apache, PHP, MariaDB, Streamlit)

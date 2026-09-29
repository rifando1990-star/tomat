<?php
// lokasi server (localhost), username (tomat_user), password (tomato), dan nama database (tomat_db).
$koneksi = new mysqli("localhost", "tomat_user", "tomato", "tomat_db");
if ($koneksi->connect_error) {
    http_response_code(500);
    die("Koneksi database gagal");
}

if ($_SERVER["REQUEST_METHOD"] !== "POST") {
    http_response_code(405);
    die("Gunakan metode POST");
}

$suhu             = $_POST["suhu"] ?? null;
$kelembapan_udara = $_POST["kelembapan_udara"] ?? null;
$kelembapan_tanah = $_POST["kelembapan_tanah"] ?? null;
$cahaya           = $_POST["cahaya"] ?? null;

if (!is_numeric($suhu) || !is_numeric($kelembapan_udara) ||
    !is_numeric($kelembapan_tanah) || !is_numeric($cahaya)) {
    http_response_code(400);
    die("Data tidak lengkap atau bukan angka");
}

// 1. Timpa nilai real-time (baris yang sama terus, tabel tidak membesar)
$stmt = $koneksi->prepare(
    "UPDATE sensor_terkini SET suhu=?, kelembapan_udara=?, kelembapan_tanah=?, intensitas_cahaya=? WHERE id=1"
);
$stmt->bind_param("dddd", $suhu, $kelembapan_udara, $kelembapan_tanah, $cahaya);
$stmt->execute();
$stmt->close();

// 2. Tambahkan ke akumulator (jumlah dan total, belum dirata-rata)
$stmt = $koneksi->prepare(
    "UPDATE sensor_akumulator SET
        jumlah_data = jumlah_data + 1,
        total_suhu = total_suhu + ?,
        total_kelembapan_udara = total_kelembapan_udara + ?,
        total_kelembapan_tanah = total_kelembapan_tanah + ?,
        total_cahaya = total_cahaya + ?
     WHERE id = 1"
);
$stmt->bind_param("dddd", $suhu, $kelembapan_udara, $kelembapan_tanah, $cahaya);
$stmt->execute();
$stmt->close();

// 3. Cek: sudah 5 menit (300 detik) sejak akumulasi mulai?
$cek = $koneksi->query(
    "SELECT jumlah_data, total_suhu, total_kelembapan_udara, total_kelembapan_tanah, total_cahaya,
            TIMESTAMPDIFF(SECOND, waktu_mulai, NOW()) AS detik_berlalu
     FROM sensor_akumulator WHERE id = 1"
);
$a = $cek->fetch_assoc();

if ($a["detik_berlalu"] >= 300 && $a["jumlah_data"] > 0) {
    $n = $a["jumlah_data"];

    // Rumus rata-rata: total dibagi jumlah data
    $rata_suhu   = $a["total_suhu"] / $n;
    $rata_udara  = $a["total_kelembapan_udara"] / $n;
    $rata_tanah  = $a["total_kelembapan_tanah"] / $n;
    $rata_cahaya = $a["total_cahaya"] / $n;

    $stmt = $koneksi->prepare(
        "INSERT INTO data_sensor (suhu, kelembapan_udara, kelembapan_tanah, intensitas_cahaya) VALUES (?, ?, ?, ?)"
    );
    $stmt->bind_param("dddd", $rata_suhu, $rata_udara, $rata_tanah, $rata_cahaya);
    $stmt->execute();
    $stmt->close();

    // Reset akumulator untuk 5 menit berikutnya
    $koneksi->query(
        "UPDATE sensor_akumulator SET
            jumlah_data=0, total_suhu=0, total_kelembapan_udara=0,
            total_kelembapan_tanah=0, total_cahaya=0, waktu_mulai=NOW()
         WHERE id=1"
    );
}

echo "Data berhasil disimpan";
$koneksi->close();

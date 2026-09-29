# Potongan kode dari webapp/app.py (Streamlit) untuk menampilkan data sensor
# ESP32 secara real-time di sidebar. Bukan file lengkap: bagian ini ditempel
# ke app.py. Butuh dari app.py: SENSORS, record_sensor_only(), dan
# import mysql.connector serta streamlit as st.

import streamlit as st
import mysql.connector


# --- Fungsi Penarik Data dari Database ---
def get_data_sensor_asli():
    conn = None
    try:
        # 1. Membuka koneksi ke MariaDB
        conn = mysql.connector.connect(
            host="localhost",
            user="tomat_user",
            password="ISI_PASSWORD_DB",  # <-- password MariaDB
            database="tomat_db"
        )
        cursor = conn.cursor()

        # 2. Mengambil 1 baris data terbaru dari tabel sensor_terkini
        cursor.execute(
            "SELECT suhu, kelembapan_udara, kelembapan_tanah, intensitas_cahaya "
            "FROM sensor_terkini WHERE id = 1"
        )
        hasil = cursor.fetchone()
        cursor.close()

        # 3. Jika data ada, kembalikan nilainya. Jika kosong, kembalikan angka 0.0
        return hasil if hasil else (0.0, 0.0, 0.0, 0.0)

    except Exception as e:
        # 4. Jika error, tampilkan pesannya lalu kembalikan angka 0.0
        st.error(f"Gagal membaca database: {e}")
        return (0.0, 0.0, 0.0, 0.0)

    finally:
        # 5. Koneksi selalu ditutup, baik berhasil maupun error
        if conn is not None and conn.is_connected():
            conn.close()


@st.fragment(run_every=2)
def panel_sensor_realtime():
    # Urutan hasil: (suhu, kelembapan_udara, kelembapan_tanah, intensitas_cahaya)
    data_db = get_data_sensor_asli()

    # Memasangkan key sensor dengan urutan kolom database
    URUTAN_DB = {
        "suhu": 0,
        "kelembaban_udara": 1,
        "kelembaban_tanah": 2,
        "cahaya": 3,
    }

    sensor_values: dict[str, float] = {}
    for spec in SENSORS:
        idx = URUTAN_DB.get(spec.key)
        nilai_asli = float(data_db[idx]) if idx is not None and idx < len(data_db) else 0.0

        sensor_values[spec.key] = nilai_asli

        # Disimpan juga ke session_state agar bagian lain aplikasi tetap bisa membacanya
        st.session_state[f"sensor_{spec.key}"] = nilai_asli

        st.metric(label=f"{spec.nama} ({spec.satuan})", value=f"{nilai_asli:.2f}")

    # Kalau semua nilai 0, berarti database kosong atau gagal dibaca
    belum_ada_data = all(v == 0.0 for v in sensor_values.values())
    if belum_ada_data:
        st.warning("Data sensor belum terbaca (semua nilai 0). Cek database atau ESP32.")

    if st.button(
        "Catat pembacaan sensor",
        use_container_width=True,
        disabled=belum_ada_data,
    ):
        record_sensor_only(sensor_values)
        st.toast("Pembacaan sensor tersimpan ke riwayat tren.")


with st.sidebar:
    st.header("Sensor IoT (Real-time)")
    st.caption("Data diambil dari pembacaan ESP32 dan diperbarui otomatis.")
    panel_sensor_realtime()

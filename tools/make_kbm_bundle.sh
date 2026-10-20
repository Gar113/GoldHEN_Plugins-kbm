#!/bin/sh
# membuat bundle siap pakai (zip) untuk dibagikan
#
# Hasil: dist/kbm-bundle.zip
#   isi:
#     kbm.prx                     -> copy ke /data/GOLDHEN/plugins/
#     kbm.cfg                     -> copy ke /data/GOLDHEN/
#     kbm_keys.cfg                -> contoh mapping tombol (opsional)
#     KBM-Config.pkg              -> install lewat Package Installer
#     BACA-INI.txt                -> petunjuk singkat
#
# Pakai:  ./tools/make_kbm_bundle.sh
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PRX="$ROOT/bin/plugins/prx_final/kbm.prx"
CFG="$ROOT/plugin_src/kbm/kbm.cfg"
PKG=$(ls "$ROOT"/app_src/kbm_config/*.pkg 2>/dev/null | head -1)
OUT="$ROOT/dist"
STAGE="$OUT/kbm-bundle"

if [ ! -f "$PRX" ]; then
    echo "ERROR: $PRX belum dibuild."
    echo "  jalankan: make -C plugin_src/kbm"
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE"

cp "$PRX" "$STAGE/kbm.prx"
cp "$CFG" "$STAGE/kbm.cfg"

if [ -n "$PKG" ] && [ -f "$PKG" ]; then
    cp "$PKG" "$STAGE/KBM-Config.pkg"
else
    echo "PERINGATAN: .pkg app belum dibuild, bundle tanpa app."
    echo "  jalankan: make -C app_src/kbm_config"
fi

# contoh mapping tombol (format sama seperti yang ditulis app)
cat > "$STAGE/kbm_keys.cfg" <<'EOF'
; Contoh mapping tombol keyboard untuk plugin kbm.
; Nilai = HID usage ID (hex). Kosongkan/samakan seperti di bawah
; kalau tidak ingin mengubah mapping.
;
; Paling mudah: pakai app "KBM Config" di PS4 — file ini akan
; ditulis otomatis. File ini hanya contoh / cadangan manual.
;
; HID umum: Space=2C  Enter=28  Esc=29  Tab=2B
;           A..Z = 04..1D   1..0 = 1E..27
;           Panah: Atas=52 Bawah=51 Kiri=50 Kanan=4F
;           Ctrl kiri=E0  Shift kiri=E1  Alt kiri=E2

cross=2C
square=08
circle=09
triangle=14
l1=15
r1=1C
l3=E1
r3=E0
options=28
touchpad=2B
up=52
down=51
left=50
right=4F
; stick_l=  (kosongkan = pakai WASD bawaan)
; stick_r=  (kosongkan = pakai gerakan mouse)
; l2=       (kosongkan = klik kanan mouse)
; r2=       (kosongkan = klik kiri mouse)
EOF

cat > "$STAGE/BACA-INI.txt" <<'EOF'
===========================================================
 KBM — Keyboard & Mouse untuk semua game PS4
===========================================================

ISI FOLDER INI
--------------
  kbm.prx          plugin
  kbm.cfg          setelan sens/smooth/dll
  kbm_keys.cfg     contoh mapping tombol
  KBM-Config.pkg   aplikasi pengatur tombol (opsional)


CARA PASANG (butuh GoldHEN)
---------------------------
1. Copy kbm.prx ke PS4:
     /data/GOLDHEN/plugins/kbm.prx

   Copy kbm.cfg ke PS4:
     /data/GOLDHEN/kbm.cfg

   (lewat FTP, mis. FileZilla; atau lewat USB)

2. Pastikan plugin aktif. Di /data/GOLDHEN/plugins.ini
   tambahkan baris:
     /data/GOLDHEN/plugins/kbm.prx
   di bagian [default].

3. Jalankan game. Keyboard & mouse langsung bekerja.


ATUR ULANG TOMBOL (OPSIONAL)
----------------------------
Cara mudah — pakai aplikasi:
  1. Install KBM-Config.pkg:
     Settings > Debug Settings > Package Installer
  2. Buka app "KBM Config"
  3. Pilih tombol, tekan tombol keyboard yang diinginkan
  4. Tekan OPTIONS untuk simpan
  5. Jalankan ulang game

Cara manual — edit /data/GOLDHEN/kbm_keys.cfg
  (lihat kbm_keys.cfg di folder ini untuk contoh)


ATUR SENSITIVITAS
-----------------
Edit /data/GOLDHEN/kbm.cfg:
  sens=14      kecepatan kamera
  smooth=1     kehalusan (0 = paling responsif)
  curve=20     percepatan
  deadzone=1
  max=60       batas dorongan per frame
  invert_y=0   balik sumbu vertikal


MAPPING BAWAAN
--------------
  W A S D      = stick kiri (gerak)
  mouse        = stick kanan (kamera)
  klik kiri    = R2   (tembak)
  klik kanan   = L2   (aim)
  klik tengah  = R1
  Space        = X        Q = Segitiga
  E            = Kotak    F = Bulat
  R            = L1       T = R1
  Shift        = L3       Ctrl = R3
  Enter / Esc  = OPTIONS  Tab = touchpad
  Panah        = D-pad


MASALAH?
--------
  Mouse tidak jalan di beberapa game (mis. RE2/RE4):
    -> sedang diselidiki, lihat klog.

  Kamera terasa delay:
    -> di kbm.cfg set smooth=0, curve=10

  Lihat log plugin:
    nc <IP-PS4> 3232
    (cari baris [kbm])
EOF

# buat zip
cd "$OUT"
rm -f kbm-bundle.zip
if command -v zip >/dev/null 2>&1; then
    zip -r -q kbm-bundle.zip kbm-bundle
else
    # fallback kalau zip tidak ada
    tar -czf kbm-bundle.zip kbm-bundle
    mv kbm-bundle.zip kbm-bundle.tar.gz
    echo "zip tidak ada, dibuat kbm-bundle.tar.gz"
fi

echo "Selesai: $OUT/kbm-bundle.zip"
ls -la "$OUT"/kbm-bundle.zip 2>/dev/null || ls -la "$OUT"

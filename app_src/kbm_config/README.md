# KBM Config — aplikasi homebrew PS4

Aplikasi PKG untuk mengubah mapping tombol keyboard plugin `kbm.prx`
langsung dari PS4, tanpa perlu edit file manual.

## Cara pakai

1. Install `IV0000-KBMCFG001_00-KBMCONFIG0000000.pkg` lewat
   **Settings → Debug Settings → Package Installer** (butuh GoldHEN).
2. Buka app **KBM Config**.
3. Navigasi:

   | Tombol pad | Fungsi |
   |---|---|
   | Panah atas/bawah | pilih tombol controller |
   | Cross | ubah mapping (lalu tekan tombol keyboard) |
   | Circle | batal saat menunggu input |
   | Options | simpan ke `/data/GOLDHEN/kbm_keys.cfg` |

4. Jalankan game. Plugin akan membaca mapping baru saat load.

## File yang dihasilkan

`/data/GOLDHEN/kbm_keys.cfg`, format baris per baris:

```
cross=2C
square=08
...
```

Nilai adalah **HID usage ID** keyboard (hex). Contoh: `2C` = Space,
`04` = A, `52` = panah atas.

## Build

```sh
export OO_PS4_TOOLCHAIN=$HOME/OpenOrbis/PS4Toolchain
make
```

Menghasilkan satu file `.pkg`.

### Catatan OpenSSL

`PkgTool.Core` ditulis dengan .NET Core 3.0 dan memerlukan **OpenSSL 1.1**.
Kalau sistem hanya menyediakan OpenSSL 3 (mis. Arch saat ini), Makefile
akan otomatis memakai OpenSSL 1.1 lokal dari `/tmp/opencode/ossl11/usr/lib`
atau `/opt/openssl-1.1/lib` bila ada. Untuk menyiapkannya:

```sh
mkdir -p /tmp/opencode/ossl11 && cd /tmp/opencode/ossl11
curl -sL -o ossl.pkg.tar.zst \
  "https://archive.archlinux.org/packages/o/openssl-1.1/openssl-1.1-1.1.1.w-2-x86_64.pkg.tar.zst"
tar --use-compress-program=unzstd -xf ossl.pkg.tar.zst usr/lib/
```

## Keterbatasan saat ini

- Aplikasi memakai renderer 2D sederhana (`Scene2D`) + FreeType — ringan,
  tanpa OpenGL.
- Perubahan mapping berlaku setelah game dijalankan ulang.
- Fitur yang belum ada: pengaturan sensitivitas/curve (masih lewat
  `kbm.cfg`), profil per-game.

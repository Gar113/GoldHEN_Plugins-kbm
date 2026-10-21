/*
 * template.h — sistem template setelan untuk KBM Config
 *
 * Gagasan:
 *   Supaya tidak perlu setel ulang tiap ganti game, setelan bisa
 *   disimpan sebagai template bernama. Contoh:
 *       /data/GOLDHEN/templates/DEFAULT.cfg
 *       /data/GOLDHEN/templates/TLOU2.cfg
 *       /data/GOLDHEN/templates/Spider-Man.cfg
 *
 *   Pilih template -> nilainya ditulis ke file aktif
 *   (/data/GOLDHEN/kbm.cfg dan kbm_keys.cfg), lalu plugin memakainya
 *   saat game dijalankan ulang.
 *
 * "DEFAULT" selalu ada dan tidak bisa dihapus, jadi selalu ada jalan
 * kembali ke setelan awal.
 */
#ifndef KBM_TEMPLATE_H
#define KBM_TEMPLATE_H

#define TPL_DIR       "/data/GOLDHEN/templates"
#define TPL_MAX       32          /* maksimum template yang dibaca  */
#define TPL_NAMELEN   40

/* Satu file template: isi kamera + mapping tombol */
typedef struct {
    char name[TPL_NAMELEN];       /* nama tampil (tanpa .cfg)       */

    /* setelan kamera */
    int  sens;
    int  curve;
    int  deadzone;
    int  max;
    int  invert_y;

    /* mapping tombol; 0 = tidak diisi di template ini */
    struct { char key[20]; int val; } map[24];
    int  mapcount;
} TplData;

/* Template bawaan (tidak bisa dihapus) */
void   tpl_default(TplData *t);

/* Baca semua template dari disk. return jumlah yang ditemukan. */
int    tpl_scan(TplData *out, int maxout);

/* Simpan TplData ke /data/GOLDHEN/templates/<name>.cfg
   return 1 kalau berhasil. */
int    tpl_save(const TplData *t);

/* Hapus template. "DEFAULT" tidak boleh dihapus.
   return 1 kalau berhasil. */
int    tpl_delete(const char *name);

/* Ambil setelan kamera dari template (dipakai app & ditulis ke cfg) */
int    tpl_load_camera(const char *name, TplData *out);

/* Nama file untuk template ke-‘i’ saat memuat daftar (dipakai app) */
void   tpl_name_from_index(int idx, char *out, int outlen);

#endif

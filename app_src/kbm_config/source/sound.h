/*
 * sound.h — efek suara untuk KBM Config
 *
 * Nada dibangkitkan langsung di kode (tidak pakai file WAV), jadi:
 *   - tidak ada aset tambahan yang bisa gagal dimuat
 *   - ukuran PKG tidak bertambah
 *   - suara tetap berbunyi walau folder assets bermasalah
 */
#ifndef KBM_SOUND_H
#define KBM_SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

/* buka port audio. aman dipanggil berkali-kali.
   return 1 = berhasil, 0 = audio tidak tersedia (app tetap jalan) */
int sound_init(void);

/* efek suara */
void sound_move(void);      /* pindah baris / arahkan     */
void sound_confirm(void);   /* tekan Cross / Enter        */
void sound_cancel(void);    /* tekan Circle / ESC         */
void sound_save(void);      /* simpan config              */
void sound_listen(void);    /* mulai menunggu tombol baru */
void sound_error(void);

/* dipanggil tiap frame: kalau tidak ada suara, tidak melakukan apa pun */
void sound_update(void);

/* 1 = audio siap dipakai, 0 = tidak ada audio */
int sound_ok(void);

#ifdef __cplusplus
}
#endif

#endif

/*
 * sound.c — efek suara KBM Config
 *
 * Cara kerja:
 *   PS4 menerima audio sebagai potongan 16-bit stereo. Kita mengisi satu
 *   potongan (SOUND_FRAMES frame) dengan gelombang sinus, lalu mengirimnya
 *   dengan sceAudioOutOutput(h, NULL) supaya menunggu slot kosong.
 *
 * Penting: memanggil sceAudioOutOutput dengan NULL akan MENUNGGU sampai
 * potongan sebelumnya selesai diputar. Kalau itu dilakukan di dalam loop
 * gambar, app bisa terasa lambat saat berbunyi. Karena itu:
 *   - potongan dibuat pendek (sekitar 50 ms)
 *   - hanya dipanggil saat ada suara, tidak setiap frame
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

#include "sound.h"
#include "log.h"

#define SOUND_RATE      48000
#define SOUND_CHANNELS  2
#define SOUND_FRAMES    1024          /* ~21 ms per potongan */

/* batas jumlah potongan per efek supaya tidak pernah menggantung lama */
#define MAX_SEG         12

typedef struct {
    float freq;        /* frekuensi nada (Hz)        */
    int   ms;          /* durasi (milidetik)         */
    float vol;         /* 0.0 - 1.0                  */
} Note;

static int   g_audio = -1;

/* antrian nada yang masih harus dibunyikan */
static Note  g_queue[MAX_SEG];
static int   g_qcount = 0;
static int   g_qpos   = 0;

/* fase sinus supaya antar nada tidak "klik" */
static double g_phase = 0.0;

static short g_buf[SOUND_FRAMES * SOUND_CHANNELS];

/* ---------------------------------------------------------------- */

int sound_init(void)
{
    if (g_audio >= 0)
        return 1;

    OrbisUserServiceUserId uid = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
    int id = -1;

    /* pakai user yang sedang aktif kalau ada, supaya suara ikut keluar
       di headphone user tersebut; kalau gagal pakai SYSTEM */
    if (sceUserServiceGetInitialUser(&id) == 0 && id >= 0)
        uid = (OrbisUserServiceUserId)id;

    if (sceAudioOutInit() < 0) {
        DEBUGLOG << "[kbmcfg] audio: init gagal";
        return 0;
    }

    g_audio = sceAudioOutOpen(uid, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                              SOUND_FRAMES, SOUND_RATE,
                              ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (g_audio < 0) {
        /* coba sekali lagi dengan user SYSTEM */
        g_audio = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM,
                                  ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                  SOUND_FRAMES, SOUND_RATE,
                                  ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    }

    if (g_audio < 0) {
        DEBUGLOG << "[kbmcfg] audio: open gagal = " << g_audio;
        g_audio = -1;
        return 0;
    }

    DEBUGLOG << "[kbmcfg] audio: siap, handle = " << g_audio;
    return 1;
}

/* ---------------------------------------------------------------- */
/* tambahkan satu nada ke antrian                                    */
/* ---------------------------------------------------------------- */
static void push(float freq, int ms, float vol)
{
    if (g_qcount >= MAX_SEG)
        return;
    g_queue[g_qcount].freq = freq;
    g_queue[g_qcount].ms   = ms;
    g_queue[g_qcount].vol  = vol;
    g_qcount++;
}

/* ---------------------------------------------------------------- */
/* bangkitkan satu potongan untuk nada tertentu                      */
/* ---------------------------------------------------------------- */
static void render_note(const Note *n)
{
    double step = 2.0 * M_PI * n->freq / (double)SOUND_RATE;
    int    frames = SOUND_FRAMES;
    int    total  = (n->ms * SOUND_RATE) / 1000;
    if (total < frames)
        frames = total;

    /* fade in/out pendek supaya tidak "klik" di awal/akhir nada */
    int fade = SOUND_RATE / 200;      /* 5 ms */
    if (fade > frames / 2)
        fade = frames / 2;

    for (int i = 0; i < SOUND_FRAMES; i++) {
        float v = 0.0f;
        if (i < frames) {
            float env = 1.0f;
            if (i < fade)
                env = (float)i / (float)fade;
            else if (i > frames - fade)
                env = (float)(frames - i) / (float)fade;
            v = (float)sin(g_phase) * n->vol * env;
            g_phase += step;
            if (g_phase > 2.0 * M_PI)
                g_phase -= 2.0 * M_PI;
        } else {
            v = 0.0f;
        }
        short s = (short)(v * 32767.0f * 0.35f);   /* jangan terlalu keras */
        g_buf[i * SOUND_CHANNELS + 0] = s;
        g_buf[i * SOUND_CHANNELS + 1] = s;
    }
}

/* ---------------------------------------------------------------- */
/* dipanggil tiap frame dari loop utama                              */
/* ---------------------------------------------------------------- */
void sound_update(void)
{
    if (g_audio < 0)
        return;

    if (g_qpos >= g_qcount) {
        g_qcount = 0;
        g_qpos   = 0;
        return;
    }

    render_note(&g_queue[g_qpos]);
    g_qpos++;

    /* NULL dulu = tunggu potongan sebelumnya selesai, jadi tidak
       menumpuk di buffer audio */
    sceAudioOutOutput(g_audio, NULL);
    sceAudioOutOutput(g_audio, g_buf);
}

/* ---------------------------------------------------------------- */
/* efek suara — pola sederhana, jelas, dan tidak mengganggu         */
/* ---------------------------------------------------------------- */

/* pindah baris: blip pendek tinggi */
void sound_move(void)
{
    push(1200.0f, 25, 0.35f);
}

/* konfirmasi: dua nada naik (seperti "ok") */
void sound_confirm(void)
{
    push(880.0f,  40, 0.45f);
    push(1320.0f, 55, 0.45f);
}

/* batal: dua nada turun */
void sound_cancel(void)
{
    push(700.0f, 40, 0.45f);
    push(420.0f, 70, 0.45f);
}

/* simpan berhasil: tiga nada naik */
void sound_save(void)
{
    push(700.0f,  45, 0.45f);
    push(1000.0f, 45, 0.45f);
    push(1400.0f, 90, 0.45f);
}

/* mulai menunggu tombol: nada tunggal rendah */
void sound_listen(void)
{
    push(600.0f, 60, 0.40f);
}

/* gagal: nada rendah dua kali */
void sound_error(void)
{
    push(220.0f, 90, 0.50f);
    push(160.0f, 130, 0.50f);
}

int sound_ok(void)
{
    return g_audio >= 0;
}

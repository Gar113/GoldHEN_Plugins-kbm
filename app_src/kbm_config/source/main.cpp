/*
 * KBM Config — konfigurasi mapping keyboard untuk plugin kbm.prx
 *
 * Cara pakai:
 *   1. Buka app ini di PS4 (keyboard USB harus tercolok)
 *   2. Pilih tombol controller yang mau diubah (panah atas/bawah + Cross)
 *   3. Tekan tombol keyboard yang diinginkan
 *   4. Ulangi, lalu pilih "SIMPAN"
 *   5. Jalankan game — plugin kbm membaca /data/GOLDHEN/kbm_keys.cfg
 *
 * Warna: biru = terpilih, hijau = sudah di-mapping, abu = belum di-mapping
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>

#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/Keyboard.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>

#include "graphics.h"
#include "log.h"
#include "sound.h"
#include "template.h"

#define FRAME_WIDTH   1920
#define FRAME_HEIGHT  1080
#define FRAME_DEPTH      4

#define FONT_TITLE     56
#define FONT_ITEM      34
#define FONT_HINT      26

#define CFG_PATH  "/data/GOLDHEN/kbm_keys.cfg"

/* file setelan kamera yang dibaca plugin kbm.prx */
#define CAM_PATH  "/data/GOLDHEN/kbm.cfg"

std::stringstream debugLogStream;

/* ---------------------------------------------------------------- */
/* daftar tombol controller yang bisa di-mapping                    */
/* ---------------------------------------------------------------- */
typedef struct {
    const char *name;      /* nama tombol controller */
    const char *cfgkey;    /* kunci di file cfg      */
    uint8_t     defkey;    /* HID usage default      */
    uint8_t     key;       /* mapping aktif          */
} KbmEntry;

static KbmEntry g_items[] = {
    { "CROSS      (X)   ", "cross",    0x2C, 0 },  /* Space  */
    { "SQUARE     ([])  ", "square",   0x08, 0 },  /* E      */
    { "CIRCLE     (O)   ", "circle",   0x09, 0 },  /* F      */
    { "TRIANGLE   (D)   ", "triangle", 0x14, 0 },  /* Q      */
    { "L1                ", "l1",       0x15, 0 },  /* R      */
    { "R1                ", "r1",       0x1C, 0 },  /* T      */
    { "L2 (klik kanan)   ", "l2",       0x00, 0 },
    { "R2 (klik kiri)    ", "r2",       0x00, 0 },
    { "L3                ", "l3",       0xE1, 0 },  /* LShift */
    { "R3                ", "r3",       0xE0, 0 },  /* LCtrl  */
    { "OPTIONS           ", "options",  0x28, 0 },  /* Enter  */
    { "TOUCHPAD          ", "touchpad", 0x2B, 0 },  /* Tab    */
    { "DPAD UP           ", "up",       0x52, 0 },
    { "DPAD DOWN         ", "down",     0x51, 0 },
    { "DPAD LEFT         ", "left",     0x50, 0 },
    { "DPAD RIGHT        ", "right",    0x4F, 0 },
    { "STICK KIRI        ", "stick_l",  0x00, 0 },
    { "STICK KANAN       ", "stick_r",  0x00, 0 },
};
static const int ITEM_COUNT = sizeof(g_items) / sizeof(g_items[0]);

/* ---------------------------------------------------------------- */
/* nama tombol keyboard dari HID usage ID                           */
/* ---------------------------------------------------------------- */
static const char *hid_name(uint8_t k) {
    switch (k) {
        case 0x00: return "(kosong)";
        case 0x04: return "A";       case 0x05: return "B";
        case 0x06: return "C";       case 0x07: return "D";
        case 0x08: return "E";       case 0x09: return "F";
        case 0x0A: return "G";       case 0x0B: return "H";
        case 0x0C: return "I";       case 0x0D: return "J";
        case 0x0E: return "K";       case 0x0F: return "L";
        case 0x10: return "M";       case 0x11: return "N";
        case 0x12: return "O";       case 0x13: return "P";
        case 0x14: return "Q";       case 0x15: return "R";
        case 0x16: return "S";       case 0x17: return "T";
        case 0x18: return "U";       case 0x19: return "V";
        case 0x1A: return "W";       case 0x1B: return "X";
        case 0x1C: return "Y";       case 0x1D: return "Z";
        case 0x1E: return "1";       case 0x1F: return "2";
        case 0x20: return "3";       case 0x21: return "4";
        case 0x22: return "5";       case 0x23: return "6";
        case 0x24: return "7";       case 0x25: return "8";
        case 0x26: return "9";       case 0x27: return "0";
        case 0x28: return "ENTER";   case 0x29: return "ESC";
        case 0x2A: return "BACKSP";  case 0x2B: return "TAB";
        case 0x2C: return "SPACE";
        case 0x3A: return "F1";      case 0x3B: return "F2";
        case 0x3C: return "F3";      case 0x3D: return "F4";
        case 0x3E: return "F5";      case 0x3F: return "F6";
        case 0x40: return "F7";      case 0x41: return "F8";
        case 0x42: return "F9";      case 0x43: return "F10";
        case 0x44: return "F11";     case 0x45: return "F12";
        case 0x4F: return "PANAH KANAN"; case 0x50: return "PANAH KIRI";
        case 0x51: return "PANAH BAWAH"; case 0x52: return "PANAH ATAS";
        case 0xE0: return "CTRL KIRI";   case 0xE1: return "SHIFT KIRI";
        case 0xE2: return "ALT KIRI";    case 0xE4: return "CTRL KANAN";
        case 0xE5: return "SHIFT KANAN";
        default:   return "?";
    }
}

/* ---------------------------------------------------------------- */
/* keyboard                                                          */
/* ---------------------------------------------------------------- */
static int  g_kbh = -1;
static OrbisKeyboardData g_kbd;
static uint8_t g_prev[256];     /* tombol yang ditekan frame sebelumnya */

static int kb_open(void) {
    int uid = -1;
    OrbisUserServiceInitializeParams up;
    memset(&up, 0, sizeof(up));
    up.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    int r = sceUserServiceInitialize(&up);
    DEBUGLOG << "[kbmcfg] userServiceInit = " << r;

    if (sceUserServiceGetInitialUser(&uid) < 0) uid = -1;
    DEBUGLOG << "[kbmcfg] uid = " << uid;

    /* coba semua user id yang wajar kalau initial user gagal */
    int ids[3];
    ids[0] = uid; ids[1] = 1; ids[2] = 0;
    for (int i = 0; i < 3; i++) {
        if (ids[i] < 0) continue;
        g_kbh = sceKeyboardOpen(ids[i], 0, 0, NULL);
        if (g_kbh >= 0) break;
    }
    DEBUGLOG << "[kbmcfg] keyboard handle = " << g_kbh;
    return g_kbh;
}

/* kembalikan HID usage tombol yang BARU ditekan, atau 0 kalau tidak ada */
static uint8_t kb_poll_new_press(void) {
    if (g_kbh < 0) return 0;

    memset(&g_kbd, 0, sizeof(g_kbd));
    if (sceKeyboardReadState(g_kbh, &g_kbd) < 0) return 0;

    uint8_t cur[256];
    memset(cur, 0, sizeof(cur));
    int n = g_kbd.nkeys;
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) {
        uint16_t kc = g_kbd.keycodes[i];
        if (kc < 256) cur[kc] = 1;
    }
    if (g_kbd.mods & 0x01) cur[0xE0] = 1;
    if (g_kbd.mods & 0x02) cur[0xE1] = 1;
    if (g_kbd.mods & 0x10) cur[0xE4] = 1;
    if (g_kbd.mods & 0x20) cur[0xE5] = 1;

    uint8_t pressed = 0;
    for (int i = 4; i < 256; i++) {
        if (cur[i] && !g_prev[i]) { pressed = (uint8_t)i; break; }
    }
    memcpy(g_prev, cur, sizeof(cur));
    return pressed;
}

/* ---------------------------------------------------------------- */
/* tombol arah yang bisa dipakai app                                 */
/*                                                                   */
/* Dipakai bersama oleh keyboard dan pad, supaya satu tekanan = satu  */
/* langkah, dari sumber mana pun.                                     */
/* ---------------------------------------------------------------- */
#define NAV_LEFT   0
#define NAV_RIGHT  1
#define NAV_UP     2
#define NAV_DOWN   3
#define NAV_TAB    4
#define NAV_COUNT  5

/* Terjemahkan tombol keyboard yang baru ditekan menjadi tombol arah.
   Panah = atas/bawah/kiri/kanan, TAB = tab berikutnya. */
static void kb_nav_single(uint8_t k, int out[NAV_COUNT]) {
    for (int i = 0; i < NAV_COUNT; i++) out[i] = 0;
    if (!k) return;
    switch (k) {
        case 0x52: out[NAV_UP]    = 1; break;   /* panah atas  */
        case 0x51: out[NAV_DOWN]  = 1; break;   /* panah bawah */
        case 0x50: out[NAV_LEFT]  = 1; break;   /* panah kiri  */
        case 0x4F: out[NAV_RIGHT] = 1; break;   /* panah kanan */
        case 0x2B: out[NAV_TAB]   = 1; break;   /* TAB         */
        default: break;
    }
}

/* ---------------------------------------------------------------- */
/* pad (navigasi menu)                                               */
/* ---------------------------------------------------------------- */
static int32_t g_pad = -1;

static int pad_open(void) {
    int uid = -1;
    if (sceUserServiceGetInitialUser(&uid) < 0) uid = -1;
    int ids[3];
    ids[0] = uid; ids[1] = 1; ids[2] = 0;
    for (int i = 0; i < 3; i++) {
        if (ids[i] < 0) continue;
        g_pad = scePadOpen(ids[i], 0, 0, NULL);
        if (g_pad >= 0) break;
    }
    DEBUGLOG << "[kbmcfg] pad handle = " << g_pad;
    return g_pad;
}

#define PAD_UP        0x00000010
#define PAD_DOWN      0x00000040
#define PAD_LEFT      0x00000080
#define PAD_RIGHT     0x00000020
#define PAD_L2        0x00000100
#define PAD_R2        0x00000200
#define PAD_L1        0x00000400
#define PAD_R1        0x00000800
#define PAD_TRIANGLE  0x00001000
#define PAD_CIRCLE    0x00002000
#define PAD_CROSS     0x00004000
#define PAD_SQUARE    0x00008000
#define PAD_OPTIONS   0x00000008
#define PAD_TOUCH     0x00100000

/* ---------------------------------------------------------------- */
/* pad (navigasi menu)                                               */
/*                                                                   */
/* Kenapa tidak sekadar "newly" (tekan baru):                        */
/*                                                                   */
/*   D-pad DS4 tidak jarang terbaca sebagai getar - satu tekanan      */
/*   terlihat sebagai tekan/lepas/tekan lagi dalam beberapa frame.    */
/*   Dulu app hanya bereaksi ke "newly", jadi separuh tekanan hilang  */
/*   dan terasa seperti tombol tidak jalan.                           */
/*                                                                   */
/*   Sekarang tombol D-pad ditahan (hold) dan diulang setelah jeda    */
/*   awal, sama seperti perilaku menu pada umumnya: satu tekanan       */
/*   selalu menghasilkan satu langkah, dan menahan tombol menggulir.  */
/*                                                                   */
/*   Stick kiri juga dipakai sebagai tombol arah lewat deadzone       */
/*   sendiri, jadi stick analog pun bisa menggerakkan pilihan.        */
/* ---------------------------------------------------------------- */
#define REPEAT_DELAY  18      /* frame sebelum mulai menggulir (~0.3 s) */
#define REPEAT_RATE    6      /* jarak antar gulir (~0.1 s)             */

static uint32_t g_pad_prev = 0;       /* tombol pad mentah frame lalu   */
static int      g_pad_diag = 0;       /* diagnostik: cetak tombol mentah */

static int      g_nav_held[NAV_COUNT]   = {0, 0, 0, 0, 0};
static int      g_nav_timer[NAV_COUNT]  = {0, 0, 0, 0, 0};

/* ---------------------------------------------------------------- */
/* tombol aksi pad                                                   */
/*                                                                   */
/* Tombol aksi (Cross, Square, ...) tetap memakai "tekan baru":       */
/* satu tekanan = satu aksi, tidak ada pengulangan. Getarnya D-pad     */
/* tidak jadi masalah di sini karena satu tekanan hanya perlu         */
/* terlihat sekali - dan kalau terlihat dua kali, putaran keduanya    */
/* sama-sama tidak berbahaya (mis. "X" dua kali pada layar TOMBOL     */
/* hanya membuka mode tunggu tombol, tidak mengubah apa pun).         */
/* ---------------------------------------------------------------- */
static uint32_t g_btn_prev  = 0;
static int      g_btn_diag  = 0;

static uint32_t pad_buttons_now(void) {
    if (g_pad < 0) return g_pad_prev;
    OrbisPadData pd;
    memset(&pd, 0, sizeof(pd));
    if (scePadReadState(g_pad, &pd) < 0) return g_pad_prev;
    g_pad_prev = pd.buttons;
    return pd.buttons;
}

static uint32_t pad_poll_new(void) {
    uint32_t now = pad_buttons_now();
    uint32_t newly = now & ~g_btn_prev;
    g_btn_prev = now;

    if (newly && g_btn_diag < 40) {
        DEBUGLOG << "[kbmcfg] pad baru=0x" << std::hex << newly;
        g_btn_diag++;
    }
    return newly;
}

/* ambil tombol arah dari pad: D-pad + stick kiri (dengan deadzone) */
static void pad_read_nav(int out[NAV_COUNT]) {
    for (int i = 0; i < NAV_COUNT; i++) out[i] = 0;
    if (g_pad < 0) return;

    OrbisPadData pd;
    memset(&pd, 0, sizeof(pd));
    if (scePadReadState(g_pad, &pd) < 0) return;

    uint32_t now = pd.buttons;
    g_pad_prev = now;

    if (now && g_pad_diag < 40) {
        DEBUGLOG << "[kbmcfg] pad buttons=0x" << std::hex << now;
        g_pad_diag++;
    }

    out[NAV_LEFT]  = (now & PAD_LEFT)  ? 1 : 0;
    out[NAV_RIGHT] = (now & PAD_RIGHT) ? 1 : 0;
    out[NAV_UP]    = (now & PAD_UP)    ? 1 : 0;
    out[NAV_DOWN]  = (now & PAD_DOWN)  ? 1 : 0;
    out[NAV_TAB]   = (now & PAD_TOUCH) ? 1 : 0;

    /* Stick kiri jadi tombol arah.
       Deadzone-nya dibuat kecil (30 dari 128) supaya tidak perlu
       didorong penuh, tapi cukup besar supaya stick yang sedikit
       miring tidak membuat pilihan jalan sendiri.

       PENTING: leftStick.x dan leftStick.y bertipe uint8_t, bukan
       int8_t. Jangkauannya 0..255 dengan titik tengah 128, jadi
       netral itu 128 - bukan 0. Kalau dibandingkan dengan 0
       (seperti upaya pertama), stick tidak akan pernah terdeteksi
       karena nilainya tidak pernah negatif.

       Arah sumbu Y: nilai LEBIH BESAR = BAWAH, LEBIH KECIL = ATAS.
       Ini sama dengan konvensi yang dipakai plugin (W = ly - 127
       = atas, S = ly + 127 = bawah), jadi stick di app dan stick di
       game tidak akan berlawanan arah. */
    const int STICK_CENTER = 128;
    const int STICK_DZ     = 30;

    int sx = (int)pd.leftStick.x - STICK_CENTER;
    int sy = (int)pd.leftStick.y - STICK_CENTER;

    if (sx < -STICK_DZ) out[NAV_LEFT]  = 1;
    if (sx >  STICK_DZ) out[NAV_RIGHT] = 1;
    if (sy < -STICK_DZ) out[NAV_UP]    = 1;   /* y kecil = atas  */
    if (sy >  STICK_DZ) out[NAV_DOWN]  = 1;   /* y besar = bawah */
}

/* Ubah keadaan tombol arah (D-pad / stick / keyboard) jadi "kejadian"
   yang sudah diulang. Hasilnya: 1 = arah ini harus memicu satu langkah
   pada frame ini. Semua arah ditangani serentak supaya memutar pilihan
   tidak terasa tersendat. */
static void nav_step(const int held[NAV_COUNT], int fire[NAV_COUNT]) {
    for (int i = 0; i < NAV_COUNT; i++) {
        fire[i] = 0;
        if (!held[i]) {
            g_nav_held[i]  = 0;
            g_nav_timer[i] = 0;
            continue;
        }
        if (!g_nav_held[i]) {          /* tekanan baru: langsung satu langkah */
            g_nav_held[i]  = 1;
            g_nav_timer[i] = REPEAT_DELAY;
            fire[i]        = 1;
        } else if (--g_nav_timer[i] <= 0) {
            g_nav_timer[i] = REPEAT_RATE;
            fire[i]        = 1;
        }
    }
}

/* ---------------------------------------------------------------- */
/* setelan kamera (kbm.cfg)                                          */
/* ---------------------------------------------------------------- */
static int  cfg_save(void);      /* didefinisikan di bawah */

static int g_sens     = 30;
static int g_curve    = 1;
static int g_deadzone = 1;
static int g_max      = 127;
static int g_invert_y = 0;

/* tulis kbm.cfg dari nilai di atas.
   Ditulis lengkap dengan komentar supaya user yang membuka file ini
   langsung paham, dan supaya plugin selalu menemukan kuncinya. */
static int cam_save(void) {
    mkdir("/data", 0777);
    mkdir("/data/GOLDHEN", 0777);

    FILE *f = fopen(CAM_PATH, "w");
    if (!f) {
        DEBUGLOG << "[kbmcfg] gagal buka " << CAM_PATH;
        return 0;
    }

    fprintf(f, "; kbm.cfg - setelan kamera plugin kbm.prx\n");
    fprintf(f, "; Ditulis oleh app KBM Config. Boleh diedit manual.\n");
    fprintf(f, ";\n");
    fprintf(f, "; sens      = kecepatan kamera (makin besar makin cepat)\n");
    fprintf(f, "; curve     = tambahan kecepatan linear (1 = murni)\n");
    fprintf(f, "; deadzone  = gerakan di bawah nilai ini diabaikan\n");
    fprintf(f, "; max       = batas dorongan stik per frame\n");
    fprintf(f, "; invert_y  = 1 untuk membalik atas/bawah\n");
    fprintf(f, ";\n");
    fprintf(f, "; CATATAN: 'smooth' sudah tidak dipakai plugin.\n");
    fprintf(f, "; Jalankan ulang game setelah mengubah file ini.\n");
    fprintf(f, "\n");
    fprintf(f, "sens=%d\n",     g_sens);
    fprintf(f, "curve=%d\n",    g_curve);
    fprintf(f, "deadzone=%d\n", g_deadzone);
    fprintf(f, "max=%d\n",      g_max);
    fprintf(f, "invert_y=%d\n", g_invert_y);
    fprintf(f, "smooth=0\n");

    fclose(f);
    DEBUGLOG << "[kbmcfg] kbm.cfg disimpan: sens=" << g_sens;
    return 1;
}

static void cam_load(void) {
    FILE *f = fopen(CAM_PATH, "r");
    if (!f) {
        DEBUGLOG << "[kbmcfg] kbm.cfg belum ada, pakai default";
        return;
    }
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ';' || *s == '#' || *s == '\n' || *s == '\0') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        int v = atoi(eq + 1);
        if      (!strcmp(s, "sens"))     g_sens     = v;
        else if (!strcmp(s, "curve"))    g_curve    = v;
        else if (!strcmp(s, "deadzone")) g_deadzone = v;
        else if (!strcmp(s, "max"))      g_max      = v;
        else if (!strcmp(s, "invert_y")) g_invert_y = v;
    }
    fclose(f);
    DEBUGLOG << "[kbmcfg] kbm.cfg dibaca: sens=" << g_sens;
}

/* Ambil template dari setelan yang sedang aktif.
   Mapping tombol ikut disimpan supaya satu template = satu paket
   lengkap. */
static void tpl_from_current(TplData *t, const char *name) {
    memset(t, 0, sizeof(*t));
    strncpy(t->name, name, TPL_NAMELEN - 1);
    t->sens     = g_sens;
    t->curve    = g_curve;
    t->deadzone = g_deadzone;
    t->max      = g_max;
    t->invert_y = g_invert_y;

    t->mapcount = 0;
    for (int i = 0; i < ITEM_COUNT && t->mapcount < 24; i++) {
        if (g_items[i].key) {
            strncpy(t->map[t->mapcount].key, g_items[i].cfgkey, 19);
            t->map[t->mapcount].val = g_items[i].key;
            t->mapcount++;
        }
    }
}

/* Pakai template: tulis ke kbm.cfg + kbm_keys.cfg */
static int tpl_apply(const TplData *t) {
    g_sens     = t->sens;
    g_curve    = t->curve;
    g_deadzone = t->deadzone;
    g_max      = t->max;
    g_invert_y = t->invert_y;

    int ok = cam_save();

    /* mapping tombol hanya ditimpa kalau template menyimpannya */
    if (t->mapcount > 0) {
        for (int i = 0; i < t->mapcount; i++) {
            for (int j = 0; j < ITEM_COUNT; j++) {
                if (strcmp(t->map[i].key, g_items[j].cfgkey) == 0) {
                    g_items[j].key = (uint8_t)t->map[i].val;
                    break;
                }
            }
        }
        if (!cfg_save()) ok = 0;
    }

    DEBUGLOG << "[kbmcfg] template dipakai: " << t->name;
    return ok;
}

/* ---------------------------------------------------------------- */
/* simpan config                                                     */
/* ---------------------------------------------------------------- */
static int cfg_save(void) {
    /* pastikan folder /data/GOLDHEN ada (mis. PS4 yang belum pernah
       pakai GoldHEN sama sekali) */
    mkdir("/data", 0777);
    mkdir("/data/GOLDHEN", 0777);

    FILE *f = fopen(CFG_PATH, "w");
    if (!f) {
        DEBUGLOG << "[kbmcfg] gagal buka " << CFG_PATH;
        return 0;
    }
    fprintf(f, "; dibuat oleh KBM Config app\n");
    fprintf(f, "; nilai = HID usage ID (hex), lihat README\n");
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (g_items[i].key)
            fprintf(f, "%s=%02X\n", g_items[i].cfgkey, g_items[i].key);
    }
    fclose(f);
    DEBUGLOG << "[kbmcfg] config disimpan ke " << CFG_PATH;
    return 1;
}

/* baca cfg yang ada supaya tidak kehilangan setelan lama */
static void cfg_load(void) {
    FILE *f = fopen(CFG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ';' || *s == '#' || *s == '\n' || *s == '\0') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        /* buang spasi di akhir nama kunci */
        char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
        uint8_t v = (uint8_t)strtol(eq + 1, NULL, 16);
        for (int i = 0; i < ITEM_COUNT; i++) {
            if (strcmp(s, g_items[i].cfgkey) == 0) { g_items[i].key = v; break; }
        }
    }
    fclose(f);
}

/* ---------------------------------------------------------------- */
/* main                                                              */
/* ---------------------------------------------------------------- */
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    DEBUGLOG << "[kbmcfg] mulai";

    /* default dulu, lalu timpa dengan file kalau ada */
    for (int i = 0; i < ITEM_COUNT; i++) g_items[i].key = g_items[i].defkey;
    cfg_load();

    auto scene = new Scene2D(FRAME_WIDTH, FRAME_HEIGHT, FRAME_DEPTH);
    if (!scene->Init(0xC000000, 2)) {
        DEBUGLOG << "[kbmcfg] gagal init scene";
        for (;;);
    }

    /* Font. Kalau gagal, JANGAN hang: dulu app masuk for(;;) di sini
       sehingga layar tampak "stuck seperti cuma gambar". Sekarang kita
       catat kegagalannya dan tetap jalan (teks tidak tampil, tapi app
       tetap merespon tombol sehingga bisa didiagnosa).
       Nama file juga dicek ke beberapa kemungkinan. */
    FT_Face fTitle = NULL, fItem = NULL, fHint = NULL;
    const char *fontPath = "/app0/assets/fonts/Gontserrat-Regular.ttf";
    if (!scene->InitFont(&fTitle, fontPath, FONT_TITLE)) {
        DEBUGLOG << "[kbmcfg] GAGAL font title: " << fontPath;
        fTitle = NULL;
    }
    if (!scene->InitFont(&fItem, fontPath, FONT_ITEM)) {
        DEBUGLOG << "[kbmcfg] GAGAL font item: " << fontPath;
        fItem = NULL;
    }
    if (!scene->InitFont(&fHint, fontPath, FONT_HINT)) {
        DEBUGLOG << "[kbmcfg] GAGAL font hint: " << fontPath;
        fHint = NULL;
    }
    if (!fTitle && !fItem && !fHint)
        DEBUGLOG << "[kbmcfg] SEMUA font gagal dimuat - teks tidak akan tampil";

    /* sysmodule + keyboard */
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_KEYBOARD);
    sceKeyboardInit();
    kb_open();
    pad_open();

    /* audio. Kalau gagal, app tetap jalan tanpa suara. */
    sound_init();

    Color bg    = {0x10, 0x12, 0x18};
    Color white = {0xFF, 0xFF, 0xFF};
    Color grey  = {0x9A, 0xA0, 0xAE};
    Color green = {0x6E, 0xE7, 0x8B};
    Color blue  = {0x2A, 0x2E, 0x3A};
    Color accent= {0x5B, 0x8D, 0xEF};
    Color yellow= {0xFF, 0xD2, 0x66};

    int sel  = 0;        /* baris terpilih            */
    int wait = -1;       /* index yang menunggu tombol */
    int saved = 0;       /* hitung mundur notif simpan */
    int frameID = 0;

    /* ---- mode tampilan ----
       0 = TOMBOL   (atur mapping keyboard)
       1 = TEMPLATE (simpan / pakai / hapus setelan)          */
    int mode = 0;

    /* daftar template yang ada di /data/GOLDHEN/templates */
    TplData tpl[TPL_MAX];
    int     tpl_count = 0;
    int     tpl_sel   = 0;
    int     tpl_msg   = 0;          /* hitung mundur pesan status */
    char    tpl_stat[80] = "";
    int     tpl_reload = 1;         /* 1 = baca ulang dari disk   */

    /* nama kandidat untuk "SIMPAN SEBAGAI" — user memilih dengan
       panah kiri/kanan, jadi tidak perlu keyboard untuk mengetik */
    static const char *kandidat[] = {
        "TLOU2", "Spider-Man", "God of War", "Ghost Tsushima",
        "FPS", "TPS", "Sniper", "Santai", "Kompetitif", "Custom-1",
    };
    const int KANDIDAT_COUNT = (int)(sizeof(kandidat) / sizeof(kandidat[0]));
    int kand = 0;

    /* setelan kamera yang tersimpan di kbm.cfg */
    cam_load();

    for (;;) {
        /* ---- kalau device belum terbuka saat start, coba lagi ----
           Ini penting: kalau pad belum terbuka, app tidak bisa
           dikendalikan sama sekali dan terasa "stuck seperti cuma
           gambar". Kita coba lagi tiap 60 frame. */
        if (g_pad < 0 && (frameID % 60) == 0) pad_open();
        if (g_kbh < 0 && (frameID % 60) == 0) kb_open();

        /* ================================================================
           INPUT
           ----------------------------------------------------------------
           Satu tekanan tombol = satu langkah, di mana pun asalnya:
           pad (D-pad / stick kiri), keyboard, atau keduanya.

           Dulu navigasi menumpuk di atas "tekan baru" pad dan keyboard
           diproses terpisah, jadi:
             - D-pad yang getar bikin tekanan hilang
             - stick kiri tidak bisa dipakai sama sekali
             - keyboard tidak bisa mengganti tab atau memakai template
           Sekarang semua sumber digabung dulu jadi satu keadaan tombol,
           baru diubah jadi langkah oleh nav_step().
           ================================================================ */

        /* ---- 1. kumpulkan tombol arah dari pad + keyboard ---- */
        int nav[NAV_COUNT];
        pad_read_nav(nav);

        uint8_t k = kb_poll_new_press();

        /* ---- ganti tab: F1 = TOMBOL, F2 = TEMPLATE ----
           Hanya kalau tidak sedang menunggu tombol keyboard. Kalau
           sedang menunggu, F1/F2 harus bisa di-mapping seperti tombol
           lain - kalau tidak, pengguna yang mau memetakan F1 malah
           dipindahkan ke tab lain dan tombolnya tidak pernah tercatat. */
        if (wait < 0) {
            if (k == 0x3A)      { mode = 0; sound_move(); }   /* F1 */
            else if (k == 0x3B) { mode = 1; sound_move(); }   /* F2 */
        }

        int kb_nav[NAV_COUNT];
        kb_nav_single(k, kb_nav);
        for (int i = 0; i < NAV_COUNT; i++) if (kb_nav[i]) nav[i] = 1;

        int fire[NAV_COUNT];
        nav_step(nav, fire);

        /* ---- 2. tombol aksi + tab dari pad ---- */
        uint32_t b = pad_poll_new();

        /* ganti tab juga bisa dari pad: L1 = TOMBOL, R1 = TEMPLATE */
        if (b & PAD_L1) { mode = 0; sound_move(); }
        if (b & PAD_R1) { mode = 1; sound_move(); }

        /* Kalau layar sedang menunggu tombol keyboard, hanya ESC (batal)
           yang diproses. Tombol lain - termasuk arah - ditelan supaya
           tidak ada aksi ganda saat menekan tombol yang mau di-mapping.
           Luar biasa penting: dulu panah keyboard masih menggerakkan
           pilihan sementara tombol panah juga sedang ditangkap. */
        int capturing = (wait >= 0);

        if (capturing) {
            if (k == 0x29) {                 /* ESC = batal */
                wait = -1;
                sound_cancel();
            } else if (k) {
                g_items[wait].key = k;
                DEBUGLOG << "[kbmcfg] " << g_items[wait].cfgkey
                         << " <- 0x" << std::hex << (int)k;
                wait = -1;
                sound_confirm();
            }
            if (b & PAD_CIRCLE) { wait = -1; sound_cancel(); }
        } else if (mode == 0) {
            /* ---------- TOMBOL: arah memilih baris ---------- */
            if (fire[NAV_UP])    { sel = (sel + ITEM_COUNT - 1) % ITEM_COUNT; sound_move(); }
            if (fire[NAV_DOWN])  { sel = (sel + 1) % ITEM_COUNT;              sound_move(); }

            if ((b & PAD_CROSS) || k == 0x28) {   /* X  atau ENTER */
                wait = sel;
                sound_confirm();
            }
            if ((b & PAD_OPTIONS) || k == 0x2B) { /* OPTIONS atau TAB */
                if (cfg_save() && cam_save()) { saved = 180; sound_save(); }
                else                          { sound_error(); }
            }
        } else {
            /* ---------- TEMPLATE ---------- */
            if (tpl_reload) {
                tpl_count = tpl_scan(tpl, TPL_MAX);
                tpl_reload = 0;
                if (tpl_sel >= tpl_count) tpl_sel = 0;
            }

            int n = (tpl_count > 0) ? tpl_count : 1;

            if (fire[NAV_UP])   { tpl_sel = (tpl_sel + n - 1) % n; sound_move(); }
            if (fire[NAV_DOWN]) { tpl_sel = (tpl_sel + 1) % n;     sound_move(); }
            if (fire[NAV_LEFT]) {
                kand = (kand + KANDIDAT_COUNT - 1) % KANDIDAT_COUNT;
                sound_move();
            }
            if (fire[NAV_RIGHT]) {
                kand = (kand + 1) % KANDIDAT_COUNT;
                sound_move();
            }

            /* X / ENTER = pakai template terpilih */
            if ((b & PAD_CROSS) || k == 0x28) {
                if (tpl_count > 0) {
                    if (tpl_apply(&tpl[tpl_sel])) {
                        snprintf(tpl_stat, sizeof(tpl_stat),
                                 "DIPAKAI: %s - jalankan ulang game", tpl[tpl_sel].name);
                        tpl_msg = 240;
                        sound_save();
                    } else {
                        snprintf(tpl_stat, sizeof(tpl_stat), "GAGAL memakai template");
                        tpl_msg = 240;
                        sound_error();
                    }
                }
            }

            /* SEGITIGA / F3 = simpan setelan sekarang jadi template */
            if ((b & PAD_TRIANGLE) || k == 0x3C) {
                TplData t;
                tpl_from_current(&t, kandidat[kand]);
                if (tpl_save(&t)) {
                    snprintf(tpl_stat, sizeof(tpl_stat),
                             "TERSIMPAN: %s", kandidat[kand]);
                    tpl_msg = 240;
                    tpl_reload = 1;
                    sound_save();
                } else {
                    snprintf(tpl_stat, sizeof(tpl_stat), "GAGAL menyimpan");
                    tpl_msg = 240;
                    sound_error();
                }
            }

            /* KOTAK / F4 = hapus template terpilih */
            if ((b & PAD_SQUARE) || k == 0x3D) {
                if (tpl_count > 0) {
                    if (tpl_delete(tpl[tpl_sel].name)) {
                        snprintf(tpl_stat, sizeof(tpl_stat),
                                 "DIHAPUS: %s", tpl[tpl_sel].name);
                        tpl_msg = 240;
                        tpl_reload = 1;
                        sound_cancel();
                    } else {
                        snprintf(tpl_stat, sizeof(tpl_stat),
                                 "TIDAK BISA dihapus (DEFAULT)");
                        tpl_msg = 240;
                        sound_error();
                    }
                }
            }

            if ((b & PAD_OPTIONS) || k == 0x2B) {   /* simpan semua */
                if (cfg_save() && cam_save()) { saved = 180; sound_save(); }
                else                          { sound_error(); }
            }
        }

        if (saved > 0) saved--;
        if (tpl_msg > 0) tpl_msg--;

        /* ---- gambar ----
           Cukup satu kali isi latar. Sebelumnya FrameBufferClear()
           mengisi putih lalu langsung ditimpa bg, jadi tiap frame
           menggambar dua kali penuh (~4 juta pixel) tanpa guna. */
        scene->FrameBufferFill(bg);

        Color c;
        char buf[256];

        /* Judul. Hanya karakter ASCII: font TrueType digambar glyph
           per glyph, jadi karakter UTF-8 multi-byte seperti em-dash
           tampil sebagai dua glyph sampah. */
        snprintf(buf, sizeof(buf), "KBM Config - %s",
                 mode == 0 ? "Mapping Keyboard" : "Template Setelan");
        scene->DrawText(buf, fTitle, 80, 30, bg, white);

        /* ---- tab mode di kanan atas ----
           F1/F2 gantinya ditekan di keyboard, L1/R1 di pad ---- */
        {
            int tx = FRAME_WIDTH - 620;

            scene->DrawRectangle(tx, 36, 250, 56, mode == 0 ? blue : bg);
            scene->DrawText((char *)"  [F1]  TOMBOL", fHint, tx + 10, 50,
                            mode == 0 ? blue : bg, mode == 0 ? white : grey);

            scene->DrawRectangle(tx + 270, 36, 280, 56, mode == 1 ? blue : bg);
            scene->DrawText((char *)"  [F2]  TEMPLATE", fHint, tx + 280, 50,
                            mode == 1 ? blue : bg, mode == 1 ? white : grey);
        }

        snprintf(buf, sizeof(buf),
                 "Keyboard: %s     Pad: %s     Suara: %s",
                 g_kbh >= 0 ? "OK" : "TIDAK ADA",
                 g_pad >= 0 ? "OK" : "TIDAK ADA",
                 sound_ok() ? "OK" : "TIDAK ADA");
        scene->DrawText(buf, fHint, 80, 120, bg,
                        (g_kbh >= 0 && g_pad >= 0) ? green : yellow);

        /* setelan kamera aktif — supaya user tahu template mana yang
           sedang berlaku tanpa harus membuka file */
        snprintf(buf, sizeof(buf),
                 "Kamera aktif:  sens=%d  curve=%d  deadzone=%d  max=%d",
                 g_sens, g_curve, g_deadzone, g_max);
        scene->DrawText(buf, fHint, 80, 158, bg, grey);

        /* Daftar.
           startY = ATAS teks (lihat DrawText). Jarak baris dan tinggi
           kotak sorot dihitung dari metrik font di bawah ini, jadi
           tidak ada lagi angka yang bisa meleset. */
        const int ROW_Y0 = 190;
        const int PAD_X  = 60;

        /* Kotak sorot dihitung dari METRIK FONT yang sebenarnya,
           bukan angka manual. Ini yang membuat teks dan kotak tidak
           mungkin lagi meleset.

           Sebelumnya kotak memakai FONT_ITEM+10 = 44px, sedangkan teks
           hanya setinggi capheight+ekor = 31px. Selisih 13px itu
           membuat teks tampak "naik ke atas" dan kotak tertinggal di
           bawah - persis yang terlihat di PS4.

           Sekarang: kotak = teks + bantalan 4px atas dan bawah. */
        const int capH  = scene->TextCapHeight(fItem);   /* tinggi huruf besar */
        const int descH = scene->TextHeight(fItem) - scene->TextAscent(fItem);
        const int textH = capH + (descH > 0 ? descH : 0); /* + ekor bawah (g,y,p) */
        const int boxH  = textH + 8;                      /* 4px atas + 4px bawah */

        /* jarak baris harus >= tinggi kotak, kalau tidak baris saling
           menimpa. Diambil dari metrik font supaya ikut menyesuaikan
           otomatis kalau ukuran font diubah. */
        int rowH = boxH + 2;
        if (rowH < FONT_ITEM + 6) rowH = FONT_ITEM + 6;

        /* pastikan seluruh daftar tetap muat di layar. Kalau tidak,
           rapatkan jarak barisnya. */
        int avail = (FRAME_HEIGHT - 100) - ROW_Y0;
        if (ITEM_COUNT * rowH > avail)
            rowH = avail / ITEM_COUNT;

        int y = ROW_Y0;

        if (mode == 0) {
            /* ---------------- mode TOMBOL ---------------- */
            for (int i = 0; i < ITEM_COUNT; i++) {
                if (i == sel) c = accent;
                else if (g_items[i].key) c = green;
                else c = grey;

                if (i == sel) {
                    scene->DrawRectangle(PAD_X, y - 4,
                                         FRAME_WIDTH - (PAD_X * 2),
                                         boxH, blue);
                }

                if (wait == i)
                    snprintf(buf, sizeof(buf), "%-20s <  TEKAN TOMBOL KEYBOARD...  >",
                             g_items[i].name);
                else
                    snprintf(buf, sizeof(buf), "%-20s  %s",
                             g_items[i].name, hid_name(g_items[i].key));

                scene->DrawText(buf, fItem, PAD_X + 20, y, bg, c);
                y += rowH;
            }
        } else {
            /* ---------------- mode TEMPLATE ----------------
               Tiap baris = satu file di /data/GOLDHEN/templates.
               Bentuk:  NAMA            sens=30 curve=1
               Template aktif ditandai panah.                        */
            if (tpl_count == 0) {
                snprintf(buf, sizeof(buf),
                         "Belum ada template. Tekan SEGITIGA untuk menyimpan "
                         "setelan sekarang.");
                scene->DrawText(buf, fItem, PAD_X + 20, y, bg, yellow);
                y += rowH;
            }

            /* berapa baris template yang muat, sisakan ruang untuk
               baris "SIMPAN SEBAGAI" di bawah */
            int maxrows = (FRAME_HEIGHT - 240 - ROW_Y0) / rowH;
            if (maxrows < 1) maxrows = 1;

            /* geser tampilan supaya pilihan selalu terlihat */
            int first = 0;
            if (tpl_sel >= maxrows) first = tpl_sel - maxrows + 1;

            for (int i = first; i < tpl_count && i < first + maxrows; i++) {
                int is_sel = (i == tpl_sel);

                snprintf(buf, sizeof(buf), "%-22s sens=%-4d curve=%-3d dz=%-2d",
                         tpl[i].name, tpl[i].sens, tpl[i].curve, tpl[i].deadzone);

                Color cc = is_sel ? accent : white;
                if (strcasecmp(tpl[i].name, "DEFAULT") == 0) cc = is_sel ? accent : grey;

                if (is_sel)
                    scene->DrawRectangle(PAD_X, y - 4,
                                         FRAME_WIDTH - (PAD_X * 2), boxH, blue);

                scene->DrawText(buf, fItem, PAD_X + 20, y, bg, cc);
                y += rowH;
            }

            /* ---- baris "SIMPAN SEBAGAI" ----
               Nama dipilih dengan panah kiri/kanan supaya tidak perlu
               mengetik apa pun di PS4.                                  */
            y += 8;
            scene->DrawRectangle(PAD_X, y - 4,
                                 FRAME_WIDTH - (PAD_X * 2), boxH, bg);

            snprintf(buf, sizeof(buf),
                     "SIMPAN SEBAGAI:  <  %-16s  >   (panah kiri/kanan)",
                     kandidat[kand]);
            scene->DrawText(buf, fItem, PAD_X + 20, y, bg, yellow);
            y += rowH;

            /* pesan status */
            if (tpl_msg > 0 && tpl_stat[0]) {
                scene->DrawText(tpl_stat, fItem, PAD_X + 20, y + 6, bg, green);
            }
        }

        /* bantuan sesuai mode */
        if (mode == 0) {
            const char *hint =
                "STIK / D-PAD / PANAH = pilih baris    X / ENTER = ubah    "
                "OPTIONS / TAB = simpan    F2 = TEMPLATE";
            scene->DrawText((char *)hint, fHint, 80, 980, bg, grey);
        } else {
            const char *hint =
                "X = pakai    SEGITIGA / F3 = simpan jadi template    "
                "KOTAK / F4 = hapus    F1 = TOMBOL";
            scene->DrawText((char *)hint, fHint, 80, 980, bg, grey);
        }

        if (saved > 0) {
            snprintf(buf, sizeof(buf), "TERSIMPAN! Jalankan ulang game untuk memakai mapping ini");
            scene->DrawText(buf, fHint, 80, 1020, bg, yellow);
        }

        /* Urutan yang benar, sama seperti sample resmi OpenOrbis
           (samples/keyboard, samples/font, dll):
              gambar -> SubmitFlip -> FrameWait -> baru Swap
           Kalau Swap dipanggil SEBELUM SubmitFlip, layar menampilkan
           buffer yang belum selesai digambar: hasilnya selalu satu frame
           terlambat, dan pada frame pertama menampilkan buffer kosong.
           Inilah penyebab layar berkedip. */
        scene->SubmitFlip(frameID);
        scene->FrameWait(frameID);

        scene->FrameBufferSwap();
        frameID++;

        /* Keluarkan satu potongan suara kalau ada nada menunggu.
           Ditaruh SETELAH presentasi frame supaya penundaan audio
           tidak menahan gambar. */
        sound_update();
    }

    return 0;
}

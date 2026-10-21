#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "plugin_common.h"

typedef struct {
    uint32_t buttons;
    uint8_t  lx, ly, rx, ry;
    uint8_t  l2, r2;
} kbm_pad;

/* bit tombol — sama dengan pad.h */
#define PB_L3       0x00000002
#define PB_R3       0x00000004
#define PB_OPTIONS  0x00000008
#define PB_UP       0x00000010
#define PB_RIGHT    0x00000020
#define PB_DOWN     0x00000040
#define PB_LEFT     0x00000080
#define PB_L1       0x00000400
#define PB_R1       0x00000800
#define PB_TRIANGLE 0x00001000
#define PB_CIRCLE   0x00002000
#define PB_CROSS    0x00004000
#define PB_SQUARE   0x00008000
#define PB_TOUCHPAD 0x00100000

/* HID usage ID keyboard page (sama dengan OrbisKeyboardKey_*) */
#define K_A 0x04
#define K_B 0x05
#define K_C 0x06
#define K_D 0x07
#define K_E 0x08
#define K_F 0x09
#define K_G 0x0A
#define K_Q 0x14
#define K_R 0x15
#define K_S 0x16
#define K_T 0x1C
#define K_V 0x19
#define K_W 0x1A
#define K_Y 0x1D
#define K_ENTER  0x28
#define K_ESCAPE 0x29
#define K_BACKSP 0x2A
#define K_TAB    0x2B
#define K_SPACE  0x2C
#define K_RIGHT  0x4F
#define K_LEFT   0x50
#define K_DOWN   0x51
#define K_UP     0x52
#define K_LCTRL  0xE0
#define K_LSHIFT 0xE1
#define K_LALT   0xE2
#define K_RCTRL  0xE4
#define K_RSHIFT 0xE5

#define MOD_LCTRL  0x01
#define MOD_LSHIFT 0x02
#define MOD_RCTRL  0x10
#define MOD_RSHIFT 0x20

static uint8_t g_keys[256];
static uint8_t g_mbtn;
static int     g_mwheel;
static int     g_dx, g_dy;
static int     g_sens     = 30;
static int     g_invert_y = 0;
static int     g_deadzone = 1;
static int     g_curve    = 20;
static int     g_smooth   = 0;   /* TIDAK dipakai lagi (lihat catatan di kbm.cfg) */
static int     g_sm_max   = 127; /* 127 = praktis tanpa batas (jangan potong gerakan normal) */

static int g_sm_x, g_sm_y;     /* nilai stik yang sedang dikirim */
static int g_cam_acc_x, g_cam_acc_y;  /* sisa JARAK mouse yang belum tercicil */

/* ---- pengatur laju pembayaran cicilan kamera --------------------
   Cicilan harus dibayar menurut WAKTU, bukan menurut berapa kali game
   memanggil scePadRead.

   Kalau dibayar per panggilan: game yang minta data 3x per frame
   (biasa untuk 2-4 pemain) membayar 3x lebih cepat, jadi gerakan mouse
   jadi tersendat dan tidak rata. Game yang minta 30x/detik juga akan
   terasa berbeda dari yang 120x/detik, padahal mouse digerakkan sama.

   Dengan jam nyata, satu interval 60 Hz = satu pembayaran. Hasilnya
   sama di semua game, dan "gyro/mouse" terasa sama seperti PC.

   g_sm_x/y = nilai stik yang SEDANG dikirim. Nilainya tetap sampai
   ada pembayaran berikutnya, jadi hook yang dipanggil berkali-kali
   dalam satu interval membaca nilai yang SAMA - tidak ada gerakan
   yang hilang. */
static uint64_t g_cam_next_us = 0;     /* waktu pembayaran berikutnya */
static int      g_cam_due     = 0;     /* 1 = interval ini sudah lewat poll */
static int      g_cam_fx_x    = 0;     /* sisa pecahan x100 (x) */
static int      g_cam_fx_y    = 0;     /* sisa pecahan x100 (y) */

/* 16667 us = 60 Hz. */
#define CAM_STEP_US 16667

/* jam mikrodetik monoton. sceKernelGetProcessTime() mengembalikan
   mikrodetik sejak proses mulai, cukup untuk mengukur selang. */
uint64_t kbm_now_us(void) {
    return sceKernelGetProcessTime();
}

/* ------------------------------------------------------------------ */
/* mapping tombol: bisa diubah lewat /data/GOLDHEN/kbm_keys.cfg        */
/* (file ini ditulis oleh app "KBM Config" di PS4).                    */
/* Nilai = HID usage ID tombol keyboard, 0 artinya tidak dipakai.      */
/* ------------------------------------------------------------------ */
static uint8_t g_map_cross    = K_SPACE;
static uint8_t g_map_square   = K_E;
static uint8_t g_map_circle   = K_F;
static uint8_t g_map_triangle = K_Q;
static uint8_t g_map_l1       = K_R;
static uint8_t g_map_r1       = K_T;
static uint8_t g_map_l3       = K_LSHIFT;
static uint8_t g_map_r3       = K_LCTRL;
static uint8_t g_map_options  = K_ENTER;
static uint8_t g_map_touchpad = K_TAB;
static uint8_t g_map_up       = K_UP;
static uint8_t g_map_down     = K_DOWN;
static uint8_t g_map_left     = K_LEFT;
static uint8_t g_map_right    = K_RIGHT;
static uint8_t g_map_stick_l  = 0;   /* 0 = pakai WASD bawaan */
static uint8_t g_map_stick_r  = 0;   /* 0 = pakai mouse      */
static uint8_t g_map_l2       = 0;   /* 0 = pakai klik kanan mouse */
static uint8_t g_map_r2       = 0;   /* 0 = pakai klik kiri  mouse */

/* kembalikan 1 kalau tombol dengan usage id ini sedang ditekan */
static int kb_down(uint8_t usage) {
    return usage ? g_keys[usage] : 0;
}

void kbm_key(uint8_t usage, int down) { g_keys[usage] = down ? 1 : 0; }
void kbm_mouse_move(int dx, int dy) { g_dx += dx; g_dy += dy; }
void kbm_mouse_button(int bit, int down) {
    if (down) g_mbtn |= (1u << bit); else g_mbtn &= ~(1u << bit);
}

static uint8_t clamp_axis(int v) {
    v += 128;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

void kbm_build(kbm_pad *o) {
    memset(o, 0, sizeof(*o));
    int lx = 0, ly = 0;
    /* stick kiri: default WASD. Kalau user memetakan stick_l ke tombol
       lain, tombol itu dipakai untuk maju (sumbu Y negatif). */
    if (g_keys[K_A]) lx -= 127;
    if (g_keys[K_D]) lx += 127;
    if (g_keys[K_W]) ly -= 127;
    if (g_keys[K_S]) ly += 127;
    if (g_map_stick_l && g_map_stick_l != K_W && kb_down(g_map_stick_l)) ly -= 127;
    o->lx = clamp_axis(lx);
    o->ly = clamp_axis(ly);

    /* ---- kamera: delta mouse -> stik kanan ----
       Analog stick hanya bisa -127..127 per frame, sedangkan mouse PC
       mengakumulasi JARAK. Kalau gerakan besar langsung dipotong ke 127,
       sapu 200px dan sapu 600px menghasilkan nilai yang sama -> kamera
       terasa berat dan "tidak ngaruh".

       Solusinya: gerakan mouse dikumpulkan, lalu dicicil sedikit demi
       sedikit. Jadi 1 px mouse selalu menghasilkan sudut kamera yang
       sama, seperti PC.

       KAPAN cicilan dibayar - ini yang menentukan terasa PC atau tidak.
       Pembayaran diikat ke WAKTU (60 Hz), bukan ke jumlah panggilan
       scePadRead. Alasannya:

         - Game minta data pad dengan irama berbeda-beda. Ada yang 60x
           per detik, ada yang 3x per frame untuk beberapa pemain. Kalau
           pembayaran dihitung per panggilan, irama cicilan ikut berubah
           dan kamera jadi tersendat / kecepatannya beda antar game.
         - Dalam satu interval hanya satu pembayaran. Panggilan lain di
           interval yang sama membaca nilai yang SAMA (g_sm_x/y), jadi
           tidak ada gerakan yang hilang atau terhitung dua kali.

       Tidak ada perataan (smoothing): setiap pixel dibayar pada interval
       berikutnya dan langsung dikirim. Perataan itulah yang dulu terasa
       sebagai DELAY. 'smooth' di cfg tetap dibaca supaya cfg lama tidak
       error, tapi TIDAK mempengaruhi kamera. */

    /* Pembayaran HANYA terjadi kalau kbm_poll menandai interval ini.
       Kalau di sini kita juga memeriksa 'now >= g_cam_next_us', pada game
       yang bertanya lebih sering dari 60 Hz (mis. 120 fps) fungsi ini
       ikut membuka gerbang sendiri di antara dua poll -> DALAM SATU
       INTERVAL TERJADI DUA PEMBAYARAN dan kamera jadi ~2x lebih cepat.

       Jadi kontraknya jelas dan hanya satu arah:
         kbm_poll  -> pemilik gerbang, menyalakan  g_cam_due
         kbm_build -> pemakai gerbang, mematikan g_cam_due saat membayar
       Satu interval = tepat satu pembayaran. */
    if (g_cam_due) {
        g_cam_due = 0;

        /* Ambil gerakan yang terkumpul sejak pembayaran terakhir.
           Dikosongkan DI SINI, bukan di awal fungsi: kalau dikosongkan
           lebih awal, panggilan yang belum waktunya bayar akan membuang
           gerakan mouse yang baru masuk. */
        int dx = g_dx, dy = g_dy;
        g_dx = g_dy = 0;

        /* sens = nilai stik per 10 px mouse. sens=20 -> 2.0 nilai per px.
           curve > 1 menambah dorongan pada gerakan cepat, dan tambahan
           ini LINEAR juga (bukan kuadrat), jadi gerakan kecil tetap
           ringan. curve=1 -> murni linear, paling mirip mouse PC. */
        int mul = g_sens * 10 + (g_curve - 10);
        if (mul < 1) mul = 1;

        /* Jarak diubah ke nilai stik. PENTING: pembagiannya TIDAK boleh
           memotong pecahan, karena tiap cicilan datang dalam potongan
           kecil (mouse melapor tiap beberapa ms). Kalau 1 px = 3.1 nilai
           dipotong jadi 3, tiap cicilan kehilangan 0.1 - dan makin tinggi
           fps game, makin banyak cicilan, makin banyak yang hilang:
           kamera jadi lebih lambat di game yang bertanya lebih sering.

           Jadi sisa pecahannya disimpan dalam g_cam_fx_x/y berskala x100,
           dan ditambahkan pada cicilan berikutnya. Hasilnya 1 px SELALU
           bernilai sama, di fps berapa pun game berjalan. */
        g_cam_fx_x += dx * mul;          /* skala x100 */
        g_cam_fx_y += dy * mul;
        g_cam_acc_x += g_cam_fx_x / 100;
        g_cam_acc_y += g_cam_fx_y / 100;
        g_cam_fx_x %= 100;
        g_cam_fx_y %= 100;

        /* Deadzone = AMBANG, bukan potongan tiap pembayaran. Dibuat
           ambang supaya jumlah pembayaran tidak mengubah jarak total
           yang diterima kamera: game 120 fps dan game 30 fps memberi
           hasil yang sama. Kalau dipotong tiap pembayaran, makin sering
           game bertanya makin banyak yang terpotong - kamera jadi lebih
           lambat di game yang polling-nya lebih rapat.

           Arahnya dijaga: kalau gerakannya ke kanan tapi terkumpulnya
           masih di bawah ambang, nilainya dianggap 0 (diam), bukan
           dibalik arah. */
        if (g_cam_acc_x > -g_deadzone && g_cam_acc_x < g_deadzone) {
            /* masih dalam ambang: tahan dulu, gerakan tetap tersimpan */
        } else {
            if (g_cam_acc_x > 0)      g_cam_acc_x -= g_deadzone;
            else                      g_cam_acc_x += g_deadzone;
        }
        if (g_cam_acc_y > -g_deadzone && g_cam_acc_y < g_deadzone) {
            /* tahan */
        } else {
            if (g_cam_acc_y > 0)      g_cam_acc_y -= g_deadzone;
            else                      g_cam_acc_y += g_deadzone;
        }

        /* Keluarkan sebanyak yang diizinkan satu interval, sisanya
           ditahan untuk interval berikutnya. */
        if (g_cam_acc_x >  g_sm_max) {
            g_sm_x =  g_sm_max;
            g_cam_acc_x -= g_sm_max;
        } else if (g_cam_acc_x < -g_sm_max) {
            g_sm_x = -g_sm_max;
            g_cam_acc_x += g_sm_max;
        } else {
            g_sm_x = g_cam_acc_x;
            g_cam_acc_x = 0;
        }

        if (g_cam_acc_y >  g_sm_max) {
            g_sm_y =  g_sm_max;
            g_cam_acc_y -= g_sm_max;
        } else if (g_cam_acc_y < -g_sm_max) {
            g_sm_y = -g_sm_max;
            g_cam_acc_y += g_sm_max;
        } else {
            g_sm_y = g_cam_acc_y;
            g_cam_acc_y = 0;
        }

        /* Sisa yang sangat kecil dibuang supaya tidak ada nilai
           menggantung yang membuat kamera bergerak sendiri. */
        if (g_cam_acc_x > -2 && g_cam_acc_x < 2) g_cam_acc_x = 0;
        if (g_cam_acc_y > -2 && g_cam_acc_y < 2) g_cam_acc_y = 0;
    }

    int out_x = g_sm_x, out_y = g_sm_y;
    if (g_invert_y) out_y = -out_y;

    o->rx = clamp_axis(out_x);
    o->ry = clamp_axis(out_y);

    /* face buttons */
    if (kb_down(g_map_cross))    o->buttons |= PB_CROSS;
    if (kb_down(g_map_square))   o->buttons |= PB_SQUARE;
    if (kb_down(g_map_circle))   o->buttons |= PB_CIRCLE;
    if (kb_down(g_map_triangle)) o->buttons |= PB_TRIANGLE;
    /* shoulders */
    if (kb_down(g_map_l1))       o->buttons |= PB_L1;
    if (kb_down(g_map_r1))       o->buttons |= PB_R1;
    /* stick clicks */
    if (kb_down(g_map_l3))       o->buttons |= PB_L3;
    if (kb_down(g_map_r3))       o->buttons |= PB_R3;
    /* system */
    if (kb_down(g_map_options))  o->buttons |= PB_OPTIONS;
    if (g_keys[K_ESCAPE])        o->buttons |= PB_OPTIONS;
    if (kb_down(g_map_touchpad)) o->buttons |= PB_TOUCHPAD;
    /* dpad / arah */
    if (kb_down(g_map_up))       o->buttons |= PB_UP;
    if (kb_down(g_map_down))     o->buttons |= PB_DOWN;
    if (kb_down(g_map_left))     o->buttons |= PB_LEFT;
    if (kb_down(g_map_right))    o->buttons |= PB_RIGHT;

    /* mouse buttons: kiri=R2 (tembak), kanan=L2 (aim), tengah=R1.
       Kalau user memetakan L2/R2 ke tombol keyboard, mapping itu juga
       berlaku (ditambah, bukan menggantikan). */
    if (g_mbtn & 1)  o->r2 = 255;
    if (g_mbtn & 2)  o->l2 = 255;
    if (g_mbtn & 4)  o->buttons |= PB_R1;
    if (kb_down(g_map_r2)) o->r2 = 255;
    if (kb_down(g_map_l2)) o->l2 = 255;
}

/* ambil nilai integer setelah "key=" */
static int cfg_int(const char *line, const char *key, int *out) {
    const char *p = strstr(line, key);
    if (!p) return 0;
    p += strlen(key);
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    int v = 0, got = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; got = 1; }
    if (!got) return 0;
    *out = neg ? -v : v;
    return 1;
}

/* ambil nilai HEXADECIMAL setelah "key=" (dipakai untuk mapping tombol).
   cocokkan "cross=" dengan hati-hati supaya "cross=" tidak ikut cocok
   dengan "stick_l=" dsb. */
static int cfg_hex(const char *line, const char *key, uint8_t *out) {
    const char *p = strstr(line, key);
    if (!p) return 0;
    /* key harus di awal baris (setelah spasi) supaya tidak salah cocok */
    const char *ls = line;
    while (*ls == ' ' || *ls == '\t') ls++;
    if (p != ls) return 0;
    p += strlen(key);
    int v = 0, got = 0;
    while (1) {
        int c = *p;
        int d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = v * 16 + d;
        p++; got = 1;
    }
    if (!got) return 0;
    *out = (uint8_t)v;
    return 1;
}

/* baca file mapping tombol yang ditulis oleh app "KBM Config" di PS4 */
static void kbm_load_keymap(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        final_printf("[kbm] %s tidak ada, pakai mapping default\n", path);
        return;
    }
    char line[128];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ';' || *s == '#' || *s == '\n' || *s == '\0') continue;

        if (cfg_hex(line, "cross=",      &g_map_cross))    n++;
        if (cfg_hex(line, "square=",     &g_map_square))   n++;
        if (cfg_hex(line, "circle=",     &g_map_circle))   n++;
        if (cfg_hex(line, "triangle=",   &g_map_triangle)) n++;
        if (cfg_hex(line, "l1=",         &g_map_l1))       n++;
        if (cfg_hex(line, "r1=",         &g_map_r1))       n++;
        if (cfg_hex(line, "l2=",         &g_map_l2))       n++;
        if (cfg_hex(line, "r2=",         &g_map_r2))       n++;
        if (cfg_hex(line, "l3=",         &g_map_l3))       n++;
        if (cfg_hex(line, "r3=",         &g_map_r3))       n++;
        if (cfg_hex(line, "options=",    &g_map_options))  n++;
        if (cfg_hex(line, "touchpad=",   &g_map_touchpad)) n++;
        if (cfg_hex(line, "up=",         &g_map_up))       n++;
        if (cfg_hex(line, "down=",       &g_map_down))     n++;
        if (cfg_hex(line, "left=",       &g_map_left))     n++;
        if (cfg_hex(line, "right=",      &g_map_right))    n++;
        if (cfg_hex(line, "stick_l=",    &g_map_stick_l))  n++;
        if (cfg_hex(line, "stick_r=",    &g_map_stick_r))  n++;
    }
    fclose(f);
    final_printf("[kbm] keymap dimuat: %d baris dari %s\n", n, path);
    final_printf("[kbm]   cross=%02X square=%02X circle=%02X triangle=%02X\n",
                 g_map_cross, g_map_square, g_map_circle, g_map_triangle);
    final_printf("[kbm]   l1=%02X r1=%02X l3=%02X r3=%02X options=%02X pad=%02X\n",
                 g_map_l1, g_map_r1, g_map_l3, g_map_r3, g_map_options, g_map_touchpad);
    final_printf("[kbm]   dpad=%02X/%02X/%02X/%02X l2=%02X r2=%02X\n",
                 g_map_up, g_map_down, g_map_left, g_map_right, g_map_l2, g_map_r2);
}

void kbm_parse_cfg(const char *buf) {
    int v;

    if (cfg_int(buf, "sens=", &v) && v > 0 && v <= 200) g_sens = v;

    if (cfg_int(buf, "invert_y=", &v)) g_invert_y = (v != 0);
    if (cfg_int(buf, "deadzone=", &v)) g_deadzone = (v < 0) ? 0 : (v > 20 ? 20 : v);
    if (cfg_int(buf, "curve=", &v))    g_curve = (v < 1) ? 1 : (v > 200 ? 200 : v);
    if (cfg_int(buf, "smooth=", &v))   g_smooth = (v < 0) ? 0 : (v > 8 ? 8 : v);
    if (cfg_int(buf, "max=", &v))      g_sm_max = (v < 10) ? 10 : (v > 127 ? 127 : v);
} 

void kbm_load_cfg(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { final_printf("[kbm] config %s tidak ada, pakai default\n", path); }
    else {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            /* lewati komentar ';' dan '#' */
            char *s = line;
            while (*s == ' ' || *s == '\t') s++;
            if (*s == ';' || *s == '#' || *s == '\n' || *s == '\0') continue;
            kbm_parse_cfg(line);
        }
        fclose(f);
        final_printf("[kbm] config: sens=%d invert_y=%d deadzone=%d curve=%d smooth=%d max=%d\n",
                     g_sens, g_invert_y, g_deadzone, g_curve, g_smooth, g_sm_max);
    }

    /* mapping tombol keyboard — ditulis oleh app "KBM Config" di PS4 */
    kbm_load_keymap("/data/GOLDHEN/kbm_keys.cfg");
}

/* ---- struct sesuai SDK resmi ---- */

/* SceMouseData — layout resmi (BSD/Orbis):
   timestamp(8) connected(1) pad(3) buttons(4) xAxis(4) yAxis(4) tilt(4) wheel(4) pad(8)
   CATATAN: struct ini 40 byte dan SceMouseRead HARUS mengembalikan tepat
   sejumlah record berukuran 40 byte, kalau tidak data akan tergeser. */
typedef struct {
    uint64_t timestamp;
    uint8_t  connected;
    uint8_t  reserve[3];
    uint32_t buttons;
    int32_t  xAxis, yAxis;
    int32_t  tilt;
    int32_t  wheel;
    uint8_t  reserve2[4];
} kbm_mouse_data;

/* SceMouseOpenParam */
typedef struct {
    uint8_t behaviorFlag;
    uint8_t tablet_mode;
    uint8_t reserve[6];
} kbm_mouse_param;

/* OrbisKeyboardData / SceKeyboardData */
typedef struct {
    uint32_t timestamp;
    uint8_t  padding[12];
    int32_t  unk1;
    int32_t  nkeys;
    uint32_t locks;
    uint32_t mods;
    uint16_t keycodes[32];
} kbm_keyboard_data;

typedef int (*fn_kb_init)(void);
typedef int (*fn_kb_open)(int userId, int type, int index, void *param);
typedef int (*fn_kb_read)(int handle, kbm_keyboard_data *d);
typedef int (*fn_kb_close)(int handle);
typedef int (*fn_ms_init)(void);
typedef int (*fn_ms_open)(int userId, int type, int index, kbm_mouse_param *p);
typedef int (*fn_ms_read)(int handle, kbm_mouse_data *d, int num);
typedef int (*fn_ms_close)(int handle);
typedef int (*fn_user_initial)(int *userId);

static fn_kb_init    f_kbinit;
static fn_kb_open    f_kbopen;
static fn_kb_read    f_kbread;
static fn_kb_close   f_kbclose;
static fn_ms_init    f_msinit;
static fn_ms_open    f_msopen;
static fn_ms_read    f_msread;
static fn_ms_close   f_msclose;
static fn_user_initial f_user;

static int g_uid      = -1;
static int g_kbh      = -1;   /* keyboard handle */
static int g_mh       = -1;   /* mouse handle */
static int g_kb_retry = 0;
static int g_ms_retry = 0;
static int g_kb_inited = 0;
static int g_ms_inited = 0;

void kbm_set_api(void *kb_init, void *kb_open, void *kb_read, void *kb_close,
                 void *ms_init, void *ms_open, void *ms_read, void *ms_close,
                 void *user_initial) {
    f_kbinit = (fn_kb_init)kb_init;
    f_kbopen = (fn_kb_open)kb_open;
    f_kbread = (fn_kb_read)kb_read;
    f_kbclose = (fn_kb_close)kb_close;
    f_msinit = (fn_ms_init)ms_init;
    f_msopen = (fn_ms_open)ms_open;
    f_msread = (fn_ms_read)ms_read;
    f_msclose = (fn_ms_close)ms_close;
    f_user   = (fn_user_initial)user_initial;
}

static void poll_keyboard(void) {
    if (!f_kbread || !f_kbopen) return;

    if (g_kbh < 0) {
        if (g_kb_retry > 0) { g_kb_retry--; return; }
        g_kb_retry = 120;
        if (!g_kb_inited) { if (f_kbinit) f_kbinit(); g_kb_inited = 1; }
        if (g_uid < 0 && f_user) f_user(&g_uid);
        int h = f_kbopen(g_uid, 0, 0, NULL); /* type 0 = USB (lewat hub juga terdeteksi) */
        if (h < 0) return;
        g_kbh = h;
    }

    kbm_keyboard_data kd;
    memset(&kd, 0, sizeof(kd));
    if (f_kbread(g_kbh, &kd) < 0) { g_kbh = -1; g_kb_retry = 60; return; }

    /* keycodes = daftar tombol yang sedang ditekan (state, bukan event) */
    memset(g_keys, 0, sizeof(g_keys));
    int n = kd.nkeys;
    if (n > 32) n = 32;
    for (int i = 0; i < n; i++) {
        uint16_t kc = kd.keycodes[i];
        if (kc < 256) g_keys[kc] = 1;
    }
    /* modifier kadang hanya muncul di field mods */
    if (kd.mods & (MOD_LCTRL))  g_keys[K_LCTRL]  = 1;
    if (kd.mods & (MOD_LSHIFT)) g_keys[K_LSHIFT] = 1;
    if (kd.mods & (MOD_RCTRL))  g_keys[K_RCTRL]  = 1;
    if (kd.mods & (MOD_RSHIFT)) g_keys[K_RSHIFT] = 1;
}

static void poll_mouse(void) {
    if (!f_msread || !f_msopen) return;

    if (g_mh < 0) {
        if (g_ms_retry > 0) { g_ms_retry--; return; }
        g_ms_retry = 120;
        if (!g_ms_inited) { if (f_msinit) f_msinit(); g_ms_inited = 1; }
        if (g_uid < 0 && f_user) f_user(&g_uid);
        kbm_mouse_param p;
        memset(&p, 0, sizeof(p));
        int h = f_msopen(g_uid, 0, 0, &p); /* type 0 = USB */
        if (h < 0) {
            static int logged = 0;
            if (!logged) { logged = 1; final_printf("[kbm] mouse open gagal: uid=%d ret=0x%08x\n", g_uid, h); }
            return;
        }
        g_mh = h;
        final_printf("[kbm] mouse terbuka: uid=%d handle=%d\n", g_uid, g_mh);
    }

    kbm_mouse_data d[8];
    memset(d, 0, sizeof(d));
    int n = f_msread(g_mh, d, 8);
    if (n < 0) {
        static int logged = 0;
        if (!logged) { logged = 1; final_printf("[kbm] mouse read gagal: ret=0x%08x\n", n); }
        g_mh = -1;
        g_ms_retry = 60;
        return;
    }
    if (n > 8) n = 8;

    /* kalau tidak ada event, jangan sentuh state tombol sama sekali —
       ini mencegah tombol "nyangkut" dari data sampah */
    if (n == 0) return;

    for (int i = 0; i < n; i++) {
        if (!d[i].connected) continue;

        /* hanya gerakan yang masuk akal (delta kecil) supaya data sampah
           tidak menggerakkan kamera */
        int mx = d[i].xAxis, my = d[i].yAxis;
        if (mx > -2048 && mx < 2048 && my > -2048 && my < 2048) {
            kbm_mouse_move(mx, my);
        }

        /* tombol mouse: bit 0=kiri 1=kanan 2=tengah */
        g_mbtn = (uint8_t)(d[i].buttons & 0x07);
        g_mwheel = d[i].wheel;
    }
}

/* Berapa kali game memanggil scePadRead per interval kamera (60 Hz).
   Angka ini penting untuk diagnosa: kalau jauh di atas 1, berarti game
   memang meminta data berkali-kali per frame - dan itulah alasan
   pembayaran cicilan HARUS diikat ke waktu, bukan ke panggilan. */
static int g_calls_this_step = 0;
static int g_calls_last_max   = 0;

void kbm_poll(void) {
    /* Device cukup dibaca sekali per interval kamera.

       Dulu poll jalan di SETIAP panggilan scePadRead. Untuk game yang
       minta data 3x per frame, mouse ikut dibaca 3x per frame - boros,
       dan pada driver tertentu bisa kehilangan laporan gerakan.

       Sekarang satu interval (60 Hz) = satu pembacaan. Gerakan mouse
       PS4 dilaporkan pada irama yang jauh lebih lambat dari 60 Hz
       (beberapa kali per detik), jadi tidak ada laporan yang terlewat. */
    uint64_t now = kbm_now_us();

    /* Gerbang waktu yang SAMA dipakai pembayaran kamera (kbm_build).
       Kalau dua-duanya punya jam sendiri, pembacaan device dan cicilan
       kamera bisa bergeser satu sama lain dan sesekali terasa seperti
       gerakan yang telat atau dobel. Satu gerbang -> mustahil bergeser. */
    if (now < g_cam_next_us) {
        g_calls_this_step++;
        if (g_calls_this_step > g_calls_last_max)
            g_calls_last_max = g_calls_this_step;
        return;
    }

    if (g_calls_this_step > g_calls_last_max)
        g_calls_last_max = g_calls_this_step;
    g_calls_this_step = 0;

    /* Tandai bahwa interval ini sudah lewat: kbm_build() boleh membayar
       cicilan kamera SEKARANG, walaupun jam pembayaran berikutnya belum
       lewat. Dengan begitu poll dan pembayaran selalu satu langkah. */
    g_cam_due = 1;

    /* Majukan gerbang TETAP satu langkah dari gerbang lama, bukan dari
       'now'. Kalau dimajukan dari 'now', tiap kali frame game memotong
       batas gerbang, jadwalnya ikut bergeser dan gerbang terbuka LEBIH
       dari 60x per detik pada game 120 fps - kamera jadi tidak rata.

       Kalau game sempat tidak bertanya lama (loading), lompat langsung
       ke sekarang supaya tidak membayar menumpuk. */
    if (now - g_cam_next_us > CAM_STEP_US * 4)
        g_cam_next_us = now + CAM_STEP_US;
    else
        g_cam_next_us += CAM_STEP_US;

    poll_keyboard();
    poll_mouse();

    /* laporan status tiap ~5 detik supaya mudah didiagnosa lewat klog */
    static int tick = 0;
    if (++tick >= 300) {
        tick = 0;
        final_printf("[kbm] status: kb=%d mh=%d sens=%d mul=%d max=%d | panggilan pad/step maks=%d | sisa=%d,%d\n",
                     g_kbh, g_mh, g_sens,
                     g_sens * 10 + (g_curve - 10), g_sm_max,
                     g_calls_last_max, g_cam_acc_x, g_cam_acc_y);
        g_calls_last_max = 0;
    }
}

int kbm_mouse_ok(void)    { return g_mh >= 0; }
int kbm_keyboard_ok(void) { return g_kbh >= 0; }
int kbm_get_uid(void)     { return g_uid; }

void kbm_shutdown(void) {
    if (g_kbh >= 0) { if (f_kbclose) f_kbclose(g_kbh); g_kbh = -1; }
    if (g_mh  >= 0) { if (f_msclose) f_msclose(g_mh);  g_mh  = -1; }
}

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
static int     g_sens     = 14;
static int     g_invert_y = 0;
static int     g_deadzone = 1;
static int     g_curve    = 20;
static int     g_smooth   = 1;   /* 0 = tanpa smoothing, default 1 = responsif */
static int     g_sm_max   = 60;  /* batas dorongan stik per frame (anti over-swing) */

static int g_sm_x, g_sm_y;     /* sisa smoothing antar frame */

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
       analog stick itu POSISI, bukan delta. Supaya terasa seperti mouse:
       1. deadzone  -> gerakan 1px tidak bikin kamera bergetar
       2. curve     -> gerakan lambat = presisi, gerakan cepat = cepat
       3. smoothing -> kurangi lompatan antar frame                        */
    int dx = g_dx, dy = g_dy;
    g_dx = g_dy = 0;

    if (dx > -g_deadzone && dx < g_deadzone) dx = 0;
    if (dy > -g_deadzone && dy < g_deadzone) dy = 0;

    /* curve kuadratik: dorongan = sign(x) * (x^2 / curve + x) */
    int ax = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;
    if (ax) ax = (ax * ax) / g_curve + ax;
    if (ay) ay = (ay * ay) / g_curve + ay;
    dx = dx < 0 ? -ax : ax;
    dy = dy < 0 ? -ay : ay;

    dx = dx * g_sens / 10;
    dy = dy * g_sens / 10;

    /* batasi dorongan per frame: mencegah kamera "over-swing" saat gerakan
       mouse besar, dan mengurangi efek delay dari smoothing berlebih */
    if (dx >  g_sm_max) dx =  g_sm_max;
    if (dx < -g_sm_max) dx = -g_sm_max;
    if (dy >  g_sm_max) dy =  g_sm_max;
    if (dy < -g_sm_max) dy = -g_sm_max;

    /* smoothing: gabung sisa frame sebelumnya.
       g_smooth=0 -> langsung (paling responsif)
       g_smooth=1 -> 50% nilai lama (seimbang, default)
       g_smooth=8 -> paling halus tapi ada delay                       */
    if (g_smooth > 0) {
        g_sm_x = (g_sm_x * g_smooth + dx) / (g_smooth + 1);
        g_sm_y = (g_sm_y * g_smooth + dy) / (g_smooth + 1);
    } else {
        g_sm_x = dx;
        g_sm_y = dy;
    }

    /* kalau tidak ada gerakan sama sekali, langsung balik ke tengah
       (mencegah kamera nempel di posisi terakhir) */
    if (dx == 0 && dy == 0 && g_smooth > 0) {
        g_sm_x = (g_sm_x * (g_smooth - 1)) / (g_smooth + 1);
        g_sm_y = (g_sm_y * (g_smooth - 1)) / (g_smooth + 1);
        if (g_sm_x > -2 && g_sm_x < 2) g_sm_x = 0;
        if (g_sm_y > -2 && g_sm_y < 2) g_sm_y = 0;
    }

    if (g_invert_y) g_sm_y = -g_sm_y;

    o->rx = clamp_axis(g_sm_x);
    o->ry = clamp_axis(g_sm_y);

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

void kbm_poll(void) {
    poll_keyboard();
    poll_mouse();

    /* laporan status tiap ~5 detik supaya mudah didiagnosa lewat klog */
    static int tick = 0;
    if (++tick >= 300) {
        tick = 0;
        final_printf("[kbm] status: kb_handle=%d kb_retry=%d | mouse_handle=%d mouse_retry=%d sens=%d\n",
                     g_kbh, g_kb_retry, g_mh, g_ms_retry, g_sens);
    }
}

int kbm_mouse_ok(void)    { return g_mh >= 0; }
int kbm_keyboard_ok(void) { return g_kbh >= 0; }
int kbm_get_uid(void)     { return g_uid; }

void kbm_shutdown(void) {
    if (g_kbh >= 0) { if (f_kbclose) f_kbclose(g_kbh); g_kbh = -1; }
    if (g_mh  >= 0) { if (f_msclose) f_msclose(g_mh);  g_mh  = -1; }
}

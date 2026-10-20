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
static int     g_dx, g_dy;
static int     g_sens    = 12;
static int     g_invert_y = 0;

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
    if (g_keys[K_A]) lx -= 127;
    if (g_keys[K_D]) lx += 127;
    if (g_keys[K_W]) ly -= 127;
    if (g_keys[K_S]) ly += 127;
    o->lx = clamp_axis(lx);
    o->ly = clamp_axis(ly);

    int ry = g_dy * g_sens;
    if (g_invert_y) ry = -ry;
    o->rx = clamp_axis(g_dx * g_sens);
    o->ry = clamp_axis(ry);
    g_dx = g_dy = 0;

    /* face buttons */
    if (g_keys[K_SPACE])  o->buttons |= PB_CROSS;
    if (g_keys[K_E])      o->buttons |= PB_SQUARE;
    if (g_keys[K_F])      o->buttons |= PB_CIRCLE;
    if (g_keys[K_Q])      o->buttons |= PB_TRIANGLE;
    /* shoulders */
    if (g_keys[K_R])      o->buttons |= PB_L1;
    if (g_keys[K_T])      o->buttons |= PB_R1;
    /* stick clicks */
    if (g_keys[K_LSHIFT] || g_keys[K_RSHIFT]) o->buttons |= PB_L3;
    if (g_keys[K_LCTRL]  || g_keys[K_RCTRL])  o->buttons |= PB_R3;
    /* system */
    if (g_keys[K_ENTER])  o->buttons |= PB_OPTIONS;
    if (g_keys[K_TAB])    o->buttons |= PB_TOUCHPAD;
    if (g_keys[K_ESCAPE]) o->buttons |= PB_OPTIONS;
    /* dpad / arah */
    if (g_keys[K_UP])     o->buttons |= PB_UP;
    if (g_keys[K_DOWN])   o->buttons |= PB_DOWN;
    if (g_keys[K_LEFT])   o->buttons |= PB_LEFT;
    if (g_keys[K_RIGHT])  o->buttons |= PB_RIGHT;

    /* mouse buttons: kiri=R2, kanan=L2, tengah=R1, back=L1, forward=Cross */
    if (g_mbtn & 1)  o->r2 = 255;
    if (g_mbtn & 2)  o->l2 = 255;
    if (g_mbtn & 4)  o->buttons |= PB_R1;
    if (g_mbtn & 8)  o->buttons |= PB_L1;
    if (g_mbtn & 16) o->buttons |= PB_CROSS;
}

void kbm_parse_cfg(const char *buf) {
    const char *p = strstr(buf, "sens=");
    if (p) {
        int v = 0;
        for (p += 5; *p >= '0' && *p <= '9'; p++) v = v * 10 + (*p - '0');
        if (v > 0 && v < 100) g_sens = v;
        return;
    }
    p = strstr(buf, "invert_y=");
    if (p) g_invert_y = (p[9] == '1');
}

void kbm_load_cfg(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { final_printf("[kbm] config %s tidak ada, pakai default\n", path); return; }
    char line[128];
    while (fgets(line, sizeof(line), f)) kbm_parse_cfg(line);
    fclose(f);
    final_printf("[kbm] config: sens=%d invert_y=%d\n", g_sens, g_invert_y);
}

/* ---- struct sesuai SDK resmi ---- */

/* SceMouseData */
typedef struct {
    uint32_t timestamp;
    uint8_t  connected;
    uint8_t  reserve[3];
    uint32_t buttons;
    int32_t  xAxis, yAxis, tilt, wheel;
    uint8_t  reserve2[8];
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
        if (h < 0) return;
        g_mh = h;
    }

    kbm_mouse_data d[8];
    memset(d, 0, sizeof(d));
    int n = f_msread(g_mh, d, 8);
    if (n < 0) { g_mh = -1; g_ms_retry = 60; return; }
    for (int i = 0; i < n; i++) {
        kbm_mouse_move(d[i].xAxis, d[i].yAxis);
        g_mbtn = (uint8_t)(d[i].buttons & 0x1F);
    }
}

void kbm_poll(void) {
    poll_keyboard();
    poll_mouse();
}

void kbm_shutdown(void) {
    if (g_kbh >= 0) { if (f_kbclose) f_kbclose(g_kbh); g_kbh = -1; }
    if (g_mh  >= 0) { if (f_msclose) f_msclose(g_mh);  g_mh  = -1; }
}

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
#include <stdlib.h>
#include <unistd.h>
#include <sstream>

#include <orbis/libkernel.h>
#include <orbis/Pad.h>
#include <orbis/Keyboard.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>

#include "graphics.h"
#include "log.h"

#define FRAME_WIDTH   1920
#define FRAME_HEIGHT  1080
#define FRAME_DEPTH      4

#define FONT_TITLE     64
#define FONT_ITEM      40
#define FONT_HINT      32

#define CFG_PATH  "/data/GOLDHEN/kbm_keys.cfg"

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
    { "SQUARE     (\x01)   ", "square",   0x08, 0 },  /* E      */
    { "CIRCLE     (O)   ", "circle",   0x09, 0 },  /* F      */
    { "TRIANGLE   (D)   ", "triangle", 0x14, 0 },  /* Q      */
    { "L1                ", "l1",       0x15, 0 },  /* R      */
    { "R1                ", "r1",       0x1C, 0 },  /* T      */
    { "L2 (mouse aim)    ", "l2",       0x00, 0 },
    { "R2 (mouse fire)   ", "r2",       0x00, 0 },
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

#define PAD_UP      0x00000010
#define PAD_DOWN    0x00000040
#define PAD_CROSS   0x00004000
#define PAD_CIRCLE  0x00002000
#define PAD_OPTIONS 0x00000008

static uint32_t g_pad_prev = 0;

static uint32_t pad_poll_new(void) {
    if (g_pad < 0) return 0;
    OrbisPadData pd;
    memset(&pd, 0, sizeof(pd));
    if (scePadReadState(g_pad, &pd) < 0) return 0;
    uint32_t now = pd.buttons;
    uint32_t newly = now & ~g_pad_prev;
    g_pad_prev = now;
    return newly;
}

/* ---------------------------------------------------------------- */
/* simpan config                                                     */
/* ---------------------------------------------------------------- */
static int cfg_save(void) {
    FILE *f = fopen(CFG_PATH, "w");
    if (!f) {
        DEBUGLOG << "[kbmcfg] gagal buka " << CFG_PATH;
        return 0;
    }
    fprintf(f, "; dibuat oleh KBM Config app\n");
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (g_items[i].key)
            fprintf(f, "%s=%02X\n", g_items[i].cfgkey, g_items[i].key);
    }
    fclose(f);
    DEBUGLOG << "[kbmcfg] config disimpan";
    return 1;
}

/* baca cfg yang ada supaya tidak kehilangan setelan lama */
static void cfg_load(void) {
    FILE *f = fopen(CFG_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == ';' || line[0] == '#') continue;
        *eq = 0;
        uint8_t v = (uint8_t)strtol(eq + 1, NULL, 16);
        for (int i = 0; i < ITEM_COUNT; i++) {
            if (strcmp(line, g_items[i].cfgkey) == 0) { g_items[i].key = v; break; }
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

    FT_Face fTitle, fItem, fHint;
    if (!scene->InitFont(&fTitle, "/app0/assets/fonts/Gontserrat-Regular.ttf", FONT_TITLE) ||
        !scene->InitFont(&fItem,  "/app0/assets/fonts/Gontserrat-Regular.ttf", FONT_ITEM) ||
        !scene->InitFont(&fHint,  "/app0/assets/fonts/Gontserrat-Regular.ttf", FONT_HINT)) {
        DEBUGLOG << "[kbmcfg] gagal load font";
        for (;;);
    }

    /* sysmodule + keyboard */
    sceSysmoduleLoadModule(ORBIS_SYSMODULE_KEYBOARD);
    sceKeyboardInit();
    kb_open();
    pad_open();

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

    for (;;) {
        /* ---- kalau device belum terbuka saat start, coba lagi ---- */
        if (g_pad < 0 && (frameID % 60) == 0) pad_open();
        if (g_kbh < 0 && (frameID % 60) == 0) kb_open();

        /* ---- input keyboard (tangkap tombol baru) ---- */
        if (wait >= 0) {
            uint8_t k = kb_poll_new_press();
            if (k) {
                if (k == 0x29) {          /* ESC = batal */
                    wait = -1;
                } else {
                    g_items[wait].key = k;
                    DEBUGLOG << "[kbmcfg] " << g_items[wait].cfgkey
                             << " <- 0x" << std::hex << (int)k;
                    wait = -1;
                }
            }
        } else {
            kb_poll_new_press();          /* tetap reset state */
        }

        /* ---- input pad ---- */
        uint32_t b = pad_poll_new();
        if (wait < 0) {
            if (b & PAD_DOWN)  sel = (sel + 1) % ITEM_COUNT;
            if (b & PAD_UP)    sel = (sel + ITEM_COUNT - 1) % ITEM_COUNT;
            if (b & PAD_CROSS) wait = sel;
            if (b & PAD_OPTIONS) {
                if (cfg_save()) saved = 180;
            }
        } else {
            if (b & PAD_CIRCLE) wait = -1;
        }
        if (saved > 0) saved--;

        /* ---- gambar ---- */
        scene->FrameBufferClear();
        scene->FrameBufferFill(bg);

        Color c;
        char buf[256];

        /* judul */
        snprintf(buf, sizeof(buf), "KBM Config — Mapping Keyboard");
        scene->DrawText(buf, fTitle, 80, 40, bg, white);

        snprintf(buf, sizeof(buf),
                 "Keyboard: %s     Config: %s",
                 g_kbh >= 0 ? "terdeteksi" : "TIDAK ADA",
                 CFG_PATH);
        scene->DrawText(buf, fHint, 80, 120, bg, g_kbh >= 0 ? green : yellow);

        /* daftar */
        int y = 190;
        for (int i = 0; i < ITEM_COUNT; i++) {
            if (i == sel) c = accent;
            else if (g_items[i].key) c = green;
            else c = grey;

            if (i == sel) {
                scene->DrawRectangle(60, y - 6, FRAME_WIDTH - 120, 52, blue);
            }

            if (wait == i)
                snprintf(buf, sizeof(buf), "%-20s <  TEKAN TOMBOL KEYBOARD...  >",
                         g_items[i].name);
            else
                snprintf(buf, sizeof(buf), "%-20s  %s",
                         g_items[i].name, hid_name(g_items[i].key));

            scene->DrawText(buf, fItem, 80, y, bg, c);
            y += 52;
        }

        /* bantuan */
        const char *hint =
            "PANAH ATAS/BAWAH = pilih     CROSS = ubah     OPTIONS = simpan     CIRCLE = batal";
        scene->DrawText((char *)hint, fHint, 80, FRAME_HEIGHT - 60, bg, grey);

        if (saved > 0) {
            snprintf(buf, sizeof(buf), "TERSIMPAN! Jalankan ulang game untuk memakai mapping ini");
            scene->DrawText(buf, fItem, 80, FRAME_HEIGHT - 130, bg, yellow);
        }

        scene->FrameBufferSwap();
        scene->SubmitFlip(frameID);
        scene->FrameWait(frameID);
        frameID++;
    }

    return 0;
}

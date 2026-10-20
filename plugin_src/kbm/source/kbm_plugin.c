#include "plugin_common.h"
#include "Common.h"
#include <Patcher.h>
#include "pad.h"

attr_public const char* g_pluginName = "kbm";
attr_public const char* g_pluginDesc = "Keyboard and mouse to controller";
attr_public const char* g_pluginAuth = "you";
attr_public u32 g_pluginVersion = 0x00000200;

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);
HOOK_INIT(scePadOpen);
HOOK_INIT(scePadGetHandle);

Patcher* scePadReadExtPatcher;
Patcher* scePadReadStateExtPatcher;

/* libScePad.sprx — sudah di-load oleh plugin, deklarasikan manual */
int32_t scePadGetHandle(int32_t userID, uint32_t controller_type, uint32_t controller_index);
int32_t scePadOpen(int32_t userID, int32_t type, int32_t index, void *param);

/* trampolin ke fungsi asli (dibuat oleh HOOK_INIT + Detour) */
HOOK_EXTERN(scePadOpen);
HOOK_EXTERN(scePadGetHandle);

/* handle pad milik player 1; input kbm hanya diterapkan ke handle ini
   supaya tidak "bocor" ke player 2 di game coop/split-screen */
static int g_p1_handle = -1;

/* daftar handle yang pernah dibuka game, dipakai kalau scePadGetHandle
   tidak mengembalikan apa pun (mis. RE Engine) */
#define KBM_MAX_HANDLES 8
static int32_t g_handles[KBM_MAX_HANDLES];
static int     g_handle_count = 0;

static void kbm_note_handle(int32_t h) {
    if (h < 0) return;
    for (int i = 0; i < g_handle_count; i++) {
        if (g_handles[i] == h) return;
    }
    if (g_handle_count < KBM_MAX_HANDLES) {
        g_handles[g_handle_count++] = h;
        final_printf("[kbm] pad handle tercatat: %d (total %d)\n", h, g_handle_count);
    }
}

/* harus sama persis dengan struct di kbm_core.c */
typedef struct {
    uint32_t buttons;
    uint8_t  lx, ly, rx, ry;
    uint8_t  l2, r2;
} kbm_pad;

extern void kbm_build(kbm_pad *o);
extern void kbm_poll(void);
extern void kbm_shutdown(void);
extern void kbm_load_cfg(const char *path);
extern void kbm_set_api(void *kb_init, void *kb_open, void *kb_read, void *kb_close,
                        void *ms_init, void *ms_open, void *ms_read, void *ms_close,
                        void *user_initial);
extern int  kbm_mouse_ok(void);
extern int  kbm_keyboard_ok(void);
extern int  kbm_get_uid(void);

static void *kbm_sym(int h, const char *name) {
    void *a = NULL;
    if (h < 0) return NULL;
    if (sceKernelDlsym(h, name, &a) != 0) return NULL;
    return a;
}

static void kbm_load_apis(void) {
    char path[256];
    int hu = 0, hm = 0, hk = 0;

    snprintf(path, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libSceUserService.sprx");
    sys_dynlib_load_prx(path, &hu);
    snprintf(path, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libSceMouse.sprx");
    int rm = sys_dynlib_load_prx(path, &hm);
    snprintf(path, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libSceKeyboard.sprx");
    int rk = sys_dynlib_load_prx(path, &hk);

    void *u  = kbm_sym(hu, "sceUserServiceGetInitialUser");

    void *ki = kbm_sym(hk, "sceKeyboardInit");
    void *ko = kbm_sym(hk, "sceKeyboardOpen");
    void *kr = kbm_sym(hk, "sceKeyboardReadState");
    void *kc = kbm_sym(hk, "sceKeyboardClose");

    void *mi = kbm_sym(hm, "sceMouseInit");
    void *mo = kbm_sym(hm, "sceMouseOpen");
    void *mr = kbm_sym(hm, "sceMouseRead");
    void *mc = kbm_sym(hm, "sceMouseClose");

    final_printf("[kbm] modules: user=%d mouse=%d(rc=%d) keyboard=%d(rc=%d)\n", hu, hm, rm, hk, rk);
    final_printf("[kbm] keyboard: init=%p open=%p read=%p close=%p\n", ki, ko, kr, kc);
    final_printf("[kbm] mouse:    init=%p open=%p read=%p close=%p\n", mi, mo, mr, mc);
    kbm_set_api(ki, ko, kr, kc, mi, mo, mr, mc, u);
}

/* Handle player 1 tidak bisa dipastikan saat plugin_load karena game belum
   membuka pad-nya. Strategi latch, dari paling presisi:
   1) handle yang cocok dengan scePadGetHandle(uid_keyboard, 0, 0)
   2) handle pertama yang tercatat lewat scePadOpen
   3) handle pertama yang di-read

   Kalau semua gagal, fallback terakhir = handle terkecil yang pernah
   di-read (game umumnya membuka player 1 lebih dulu). */
static void kbm_latch_p1(int32_t handle) {
    if (g_p1_handle >= 0) return;
    g_p1_handle = handle;
    final_printf("[kbm] player1 handle terdeteksi: %d\n", handle);
}

static int kbm_is_p1(int32_t handle) {
    /* sudah terkunci */
    if (g_p1_handle >= 0) {
        return handle == g_p1_handle;
    }

    /* 1) coba tanya langsung ke libScePad */
    int uid = kbm_get_uid();
    if (uid >= 0) {
        int h = scePadGetHandle(uid, 0, 0);
        if (h == handle) { kbm_latch_p1(handle); return 1; }
    }

    /* 2) handle pertama yang tercatat lewat scePadOpen */
    if (g_handle_count > 0) {
        kbm_latch_p1(g_handles[0]);
        return handle == g_p1_handle;
    }

    /* 3) fallback: handle pertama yang di-read = player 1 */
    kbm_latch_p1(handle);
    return 1;
}

static void kbm_apply(ScePadData* d, const kbm_pad* k) {
    d->buttons |= k->buttons;

    if (k->l2) { d->buttons |= SCE_PAD_BUTTON_L2; d->analogButtons.l2 = k->l2; }
    if (k->r2) { d->buttons |= SCE_PAD_BUTTON_R2; d->analogButtons.r2 = k->r2; }

    /* stik kiri: hanya override kalau ada input keyboard */
    if (k->lx != 128 || k->ly != 128) {
        d->leftStick.x = k->lx;
        d->leftStick.y = k->ly;
    }

    /* stik kanan (kamera): mouse aktif -> selalu tulis nilainya, termasuk
       saat kembali ke tengah. Kalau tidak, kamera bisa "nyangkut" di posisi
       terakhir waktu mouse berhenti. */
    if (kbm_mouse_ok()) {
        d->rightStick.x = k->rx;
        d->rightStick.y = k->ry;
    } else if (k->rx != 128 || k->ry != 128) {
        d->rightStick.x = k->rx;
        d->rightStick.y = k->ry;
    }
}

int32_t scePadOpen_hook(int32_t userID, int32_t type, int32_t index, void *param) {
    int32_t h = HOOK_CONTINUE(scePadOpen, int32_t (*)(int32_t, int32_t, int32_t, void *), userID, type, index, param);
    final_printf("[kbm] scePadOpen(user=%d type=%d idx=%d) -> %d\n", userID, type, index, h);
    if (h >= 0) {
        kbm_note_handle(h);
        /* user id dari keyboard/mouse harus jadi player 1 */
        if (kbm_get_uid() >= 0 && userID == kbm_get_uid() && g_p1_handle < 0) {
            kbm_latch_p1(h);
        }
    }
    return h;
}

int32_t scePadGetHandle_hook(int32_t userID, uint32_t type, uint32_t index) {
    int32_t h = HOOK_CONTINUE(scePadGetHandle, int32_t (*)(int32_t, uint32_t, uint32_t), userID, type, index);
    static int logged = 0;
    if (!logged && h >= 0) {
        logged = 1;
        final_printf("[kbm] scePadGetHandle(user=%d) -> %d\n", userID, h);
    }
    if (h >= 0) kbm_note_handle(h);
    return h;
}

int32_t scePadRead_hook(int32_t handle, ScePadData* pData, int32_t num) {
    int ret = scePadReadExt(handle, pData, num);

    /* hanya player 1 yang dapat input keyboard/mouse.
       poll tetap dijalankan supaya device terlacak walau game
       tidak mengembalikan data valid */
    if (!kbm_is_p1(handle)) {
        return ret;
    }
    kbm_poll();
    if (ret <= 0) {
        return ret;
    }
    kbm_pad k;
    kbm_build(&k);
    for (int i = 0; i < ret; i++) {
        kbm_apply(&pData[i], &k);
    }
    return ret;
}

int32_t scePadReadState_hook(int32_t handle, ScePadData* pData) {
    int ret = scePadReadStateExt(handle, pData);

    if (!kbm_is_p1(handle)) {
        return ret;
    }
    kbm_poll();
    if (ret) {
        return ret;
    }
    kbm_pad k;
    kbm_build(&k);
    kbm_apply(pData, &k);
    return ret;
}
s32 attr_public plugin_load(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    boot_ver();

    char module[256];
    snprintf(module, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libScePad.sprx");
    int h = 0;
    sys_dynlib_load_prx(module, &h);

    scePadReadExtPatcher = (Patcher*)malloc(sizeof(Patcher));
    scePadReadStateExtPatcher = (Patcher*)malloc(sizeof(Patcher));
    Patcher_Construct(scePadReadExtPatcher);
    Patcher_Construct(scePadReadStateExtPatcher);

    uint8_t xor_ecx_ecx[5] = {0x31, 0xC9, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(scePadReadExtPatcher, (uint64_t)scePadReadExt, xor_ecx_ecx, sizeof(xor_ecx_ecx));

    uint8_t xor_edx_edx[5] = {0x31, 0xD2, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(scePadReadStateExtPatcher, (uint64_t)scePadReadStateExt, xor_edx_edx, sizeof(xor_edx_edx));

    kbm_load_cfg("/data/GoldHEN/kbm.cfg");
    kbm_load_apis();

    HOOK32(scePadRead);
    HOOK32(scePadReadState);
    HOOK32(scePadOpen);
    HOOK32(scePadGetHandle);

    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);
    UNHOOK(scePadOpen);
    UNHOOK(scePadGetHandle);

    kbm_shutdown();

    Patcher_Destroy(scePadReadExtPatcher);
    Patcher_Destroy(scePadReadStateExtPatcher);
    free(scePadReadExtPatcher);
    free(scePadReadStateExtPatcher);

    return 0;
}

s32 attr_module_hidden module_start(s64 argc, const void *args)
{
    return 0;
}

s32 attr_module_hidden module_stop(s64 argc, const void *args)
{
    return 0;
}

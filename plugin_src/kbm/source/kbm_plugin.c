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

Patcher* scePadReadExtPatcher;
Patcher* scePadReadStateExtPatcher;

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
    sys_dynlib_load_prx(path, &hm);
    snprintf(path, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libSceKeyboard.sprx");
    sys_dynlib_load_prx(path, &hk);

    void *u  = kbm_sym(hu, "sceUserServiceGetInitialUser");

    void *ki = kbm_sym(hk, "sceKeyboardInit");
    void *ko = kbm_sym(hk, "sceKeyboardOpen");
    void *kr = kbm_sym(hk, "sceKeyboardReadState");
    void *kc = kbm_sym(hk, "sceKeyboardClose");

    void *mi = kbm_sym(hm, "sceMouseInit");
    void *mo = kbm_sym(hm, "sceMouseOpen");
    void *mr = kbm_sym(hm, "sceMouseRead");
    void *mc = kbm_sym(hm, "sceMouseClose");

    final_printf("[kbm] modules: user=%d mouse=%d keyboard=%d\n", hu, hm, hk);
    final_printf("[kbm] keyboard: init=%p open=%p read=%p close=%p\n", ki, ko, kr, kc);
    final_printf("[kbm] mouse:    init=%p open=%p read=%p close=%p\n", mi, mo, mr, mc);
    kbm_set_api(ki, ko, kr, kc, mi, mo, mr, mc, u);
}

static void kbm_apply(ScePadData* d, const kbm_pad* k) {
    d->buttons |= k->buttons;

    if (k->l2) { d->buttons |= SCE_PAD_BUTTON_L2; d->analogButtons.l2 = k->l2; }
    if (k->r2) { d->buttons |= SCE_PAD_BUTTON_R2; d->analogButtons.r2 = k->r2; }

    /* hanya override stik kalau ada input keyboard/mouse;
       stik controller fisik tetap jalan kalau idle */
    if (k->lx != 128 || k->ly != 128) {
        d->leftStick.x = k->lx;
        d->leftStick.y = k->ly;
    }
    if (k->rx != 128 || k->ry != 128) {
        d->rightStick.x = k->rx;
        d->rightStick.y = k->ry;
    }
}

int32_t scePadRead_hook(int32_t handle, ScePadData* pData, int32_t num) {
    int ret = scePadReadExt(handle, pData, num);
    if (ret <= 0) {
        return ret;
    }
    kbm_poll();
    kbm_pad k;
    kbm_build(&k);
    for (int i = 0; i < ret; i++) {
        kbm_apply(&pData[i], &k);
    }
    return ret;
}

int32_t scePadReadState_hook(int32_t handle, ScePadData* pData) {
    int ret = scePadReadStateExt(handle, pData);
    if (ret) {
        return ret;
    }
    kbm_poll();
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

    return 0;
}

s32 attr_public plugin_unload(s32 argc, const char* argv[]) {
    final_printf("[GoldHEN] <%s\\Ver.0x%08x> %s\n", g_pluginName, g_pluginVersion, __func__);
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);

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

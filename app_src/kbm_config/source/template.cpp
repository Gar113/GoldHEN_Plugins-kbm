/*
 * template.cpp — sistem template setelan untuk KBM Config
 *
 * Format file template (mudah dibaca dan diedit manual juga):
 *
 *     ; Template KBM Config
 *     nama=TLOU2
 *     sens=30
 *     curve=1
 *     deadzone=1
 *     max=127
 *     invert_y=0
 *     cross=2C
 *     square=08
 *     ...
 *
 * File ini TIDAK dibaca plugin. App yang membacanya, lalu menuliskan
 * nilainya ke kbm.cfg dan kbm_keys.cfg yang aktif. Jadi plugin tidak
 * perlu diubah dan file lama tetap kompatibel.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>

#include "template.h"
#include "log.h"

/* ---------------------------------------------------------------- */
/* setelan bawaan — SAMA dengan default di kbm.cfg / kbm_core.c     */
/* ---------------------------------------------------------------- */
void tpl_default(TplData *t)
{
    memset(t, 0, sizeof(*t));
    strncpy(t->name, "DEFAULT", TPL_NAMELEN - 1);
    t->sens     = 30;
    t->curve    = 1;
    t->deadzone = 1;
    t->max      = 127;
    t->invert_y = 0;
    t->mapcount = 0;      /* 0 = mapping tombol dipakai apa adanya */
}

/* nama file dari nama template, dibuang karakter yang berbahaya */
static void safe_name(const char *in, char *out, int outlen)
{
    int j = 0;
    for (int i = 0; in[i] && j < outlen - 1; i++) {
        char c = in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ')
            out[j++] = c;
    }
    /* buang spasi di ujung */
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = 0;
    if (j == 0) strncpy(out, "TANPA-NAMA", outlen - 1);
}

static void path_of(const char *name, char *out, int outlen)
{
    char safe[TPL_NAMELEN];
    safe_name(name, safe, sizeof(safe));
    snprintf(out, outlen, "%s/%s.cfg", TPL_DIR, safe);
}

/* ---------------------------------------------------------------- */
/* tulis satu template ke disk                                      */
/* ---------------------------------------------------------------- */
int tpl_save(const TplData *t)
{
    mkdir("/data", 0777);
    mkdir("/data/GOLDHEN", 0777);
    mkdir(TPL_DIR, 0777);

    char path[256];
    path_of(t->name, path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) {
        DEBUGLOG << "[kbmcfg] gagal tulis template " << path;
        return 0;
    }

    fprintf(f, "; Template KBM Config\n");
    fprintf(f, "; Pilih template ini di app, lalu jalankan ulang game.\n");
    fprintf(f, "nama=%s\n", t->name);
    fprintf(f, "sens=%d\n",     t->sens);
    fprintf(f, "curve=%d\n",    t->curve);
    fprintf(f, "deadzone=%d\n", t->deadzone);
    fprintf(f, "max=%d\n",      t->max);
    fprintf(f, "invert_y=%d\n", t->invert_y);

    for (int i = 0; i < t->mapcount; i++) {
        if (t->map[i].key[0])
            fprintf(f, "%s=%02X\n", t->map[i].key, t->map[i].val & 0xFF);
    }
    fclose(f);

    DEBUGLOG << "[kbmcfg] template disimpan: " << path;
    return 1;
}

/* ---------------------------------------------------------------- */
/* baca satu template                                               */
/* ---------------------------------------------------------------- */
int tpl_load_camera(const char *name, TplData *out)
{
    char path[256];
    path_of(name, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    tpl_default(out);
    strncpy(out->name, name, TPL_NAMELEN - 1);
    out->mapcount = 0;

    char line[160];
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == ';' || *s == '#' || *s == '\n' || *s == '\0') continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;

        char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;

        if      (!strcmp(s, "nama"))     { strncpy(out->name, v, TPL_NAMELEN - 1); }
        else if (!strcmp(s, "sens"))     { out->sens     = atoi(v); }
        else if (!strcmp(s, "curve"))    { out->curve    = atoi(v); }
        else if (!strcmp(s, "deadzone")) { out->deadzone = atoi(v); }
        else if (!strcmp(s, "max"))      { out->max      = atoi(v); }
        else if (!strcmp(s, "invert_y")) { out->invert_y = atoi(v); }
        else if (s[0]) {
            /* kunci lain dianggap mapping tombol */
            if (out->mapcount < 24) {
                strncpy(out->map[out->mapcount].key, s, 19);
                out->map[out->mapcount].val =
                    (int)strtol(v, NULL, 16) & 0xFF;
                out->mapcount++;
            }
        }
    }
    fclose(f);

    /* buang newline yang mungkin terbawa dari nama= */
    char *nl = strchr(out->name, '\n');
    if (nl) *nl = 0;
    nl = strchr(out->name, '\r');
    if (nl) *nl = 0;

    return 1;
}

/* ---------------------------------------------------------------- */
/* daftar semua template di disk                                    */
/* ---------------------------------------------------------------- */
int tpl_scan(TplData *out, int maxout)
{
    mkdir("/data", 0777);
    mkdir("/data/GOLDHEN", 0777);
    mkdir(TPL_DIR, 0777);

    int n = 0;

    /* DEFAULT selalu di posisi pertama supaya selalu bisa dipilih */
    tpl_default(&out[n]);
    n++;

    DIR *d = opendir(TPL_DIR);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL && n < maxout) {
            const char *nm = de->d_name;
            size_t len = strlen(nm);
            if (len < 5) continue;
            if (strcasecmp(nm + len - 4, ".cfg") != 0) continue;

            char base[TPL_NAMELEN];
            size_t bl = len - 4;
            if (bl >= sizeof(base)) bl = sizeof(base) - 1;
            memcpy(base, nm, bl);
            base[bl] = 0;

            /* DEFAULT sudah ditambahkan di atas */
            if (strcasecmp(base, "DEFAULT") == 0) continue;

            TplData t;
            if (tpl_load_camera(base, &t)) {
                out[n++] = t;
            }
        }
        closedir(d);
    }

    return n;
}

/* ---------------------------------------------------------------- */
/* hapus template                                                   */
/* ---------------------------------------------------------------- */
int tpl_delete(const char *name)
{
    if (strcasecmp(name, "DEFAULT") == 0) {
        DEBUGLOG << "[kbmcfg] DEFAULT tidak boleh dihapus";
        return 0;
    }

    char path[256];
    path_of(name, path, sizeof(path));

    if (unlink(path) == 0) {
        DEBUGLOG << "[kbmcfg] template dihapus: " << path;
        return 1;
    }

    DEBUGLOG << "[kbmcfg] gagal hapus " << path;
    return 0;
}

void tpl_name_from_index(int idx, char *out, int outlen)
{
    snprintf(out, outlen, "TEMPLATE-%d", idx + 1);
}

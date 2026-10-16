#ifndef CUSTOM_HAT_H
#define CUSTOM_HAT_H
/* custom_hat.h -- player-drawn hats (WOTAN Hat Studio, WOTAN_HAT_STORE_NORTHSTAR.md Phase 4).
 *
 * The studio at wotan.okemily.com/hat-studio.html exports a 32x32 pixel-art hat as a plain-text
 * ".hat" file:
 *
 *     HAT1
 *     32 32
 *     rrggbbaa rrggbbaa ... (32 tokens)     <- 32 rows
 *
 * Drop the file in the game's hats/ folder (BRAWLPIT_HATS_DIR overrides it) and it appears in the
 * character-select hat picker after the three free hats. Plain text on purpose: no image decoder
 * is needed in the client, and no heap -- every hat lives in a fixed array (standing rule: no
 * malloc, see the repo CLAUDE.md). Pure functions (parse/scan) so tests/test_custom_hat.c can
 * cover them headless; drawing stays in apps/lobby/src/main.c where the GL helpers live. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CUSTOM_HAT_SIZE      32
#define CUSTOM_HAT_MAX       16
#define CUSTOM_HAT_NAME_MAX  32
#define CUSTOM_HAT_FILE_MAX  (32 * 32 * 9 + 64)   /* 9 chars per token incl. separator + header */

typedef struct {
    char    name[CUSTOM_HAT_NAME_MAX];
    uint8_t rgba[CUSTOM_HAT_SIZE * CUSTOM_HAT_SIZE * 4];
    int     opaque;                                /* number of pixels with alpha > 0 */
} CustomHat;

static CustomHat g_custom_hats[CUSTOM_HAT_MAX];
static int       g_custom_hat_count = 0;

static int custom_hat_hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse the text of a .hat file. Returns 1 on success. Strict: wrong magic, wrong size, a short
 * row, a bad hex digit, or a completely empty hat all reject. */
static int custom_hat_parse(const char *text, size_t len, CustomHat *out) {
    size_t p = 0;
    if (len < 12 || memcmp(text, "HAT1", 4) != 0) return 0;
    p = 4;
    while (p < len && (text[p] == '\r' || text[p] == '\n' || text[p] == ' ')) p++;
    int w = 0, h = 0;
    while (p < len && text[p] >= '0' && text[p] <= '9') { w = w * 10 + (text[p++] - '0'); if (w > 4096) return 0; }
    while (p < len && text[p] == ' ') p++;
    while (p < len && text[p] >= '0' && text[p] <= '9') { h = h * 10 + (text[p++] - '0'); if (h > 4096) return 0; }
    if (w != CUSTOM_HAT_SIZE || h != CUSTOM_HAT_SIZE) return 0;
    out->opaque = 0;
    for (int i = 0; i < CUSTOM_HAT_SIZE * CUSTOM_HAT_SIZE; i++) {
        while (p < len && (text[p] == ' ' || text[p] == '\r' || text[p] == '\n' || text[p] == '\t')) p++;
        if (p + 8 > len) return 0;
        for (int k = 0; k < 4; k++) {
            int hi = custom_hat_hex((unsigned char)text[p + 2 * k]);
            int lo = custom_hat_hex((unsigned char)text[p + 2 * k + 1]);
            if (hi < 0 || lo < 0) return 0;
            out->rgba[i * 4 + k] = (uint8_t)(hi * 16 + lo);
        }
        if (out->rgba[i * 4 + 3]) out->opaque++;
        p += 8;
    }
    return out->opaque > 0;
}

/* Hat name from a filename: strip the directory and ".hat", keep printable ASCII, cap length. */
static void custom_hat_name(const char *filename, char *dst) {
    const char *base = filename;
    for (const char *c = filename; *c; c++) if (*c == '/' || *c == '\\') base = c + 1;
    int n = 0;
    for (; base[n] && n < CUSTOM_HAT_NAME_MAX - 1; n++) {
        if (base[n] == '.' && strcmp(base + n, ".hat") == 0) break;
        dst[n] = (base[n] >= 32 && base[n] < 127) ? base[n] : '_';
    }
    dst[n] = 0;
    if (n == 0) { strcpy(dst, "Custom Hat"); }
}

static int custom_hat_load_file(const char *path) {
    if (g_custom_hat_count >= CUSTOM_HAT_MAX) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static char buf[CUSTOM_HAT_FILE_MAX + 1];
    size_t n = fread(buf, 1, CUSTOM_HAT_FILE_MAX, f);
    fclose(f);
    CustomHat *h = &g_custom_hats[g_custom_hat_count];
    if (!custom_hat_parse(buf, n, h)) return 0;
    custom_hat_name(path, h->name);
    g_custom_hat_count++;
    return 1;
}

#ifdef _WIN32
#include <windows.h>
static int custom_hats_scan(const char *dir) {
    char pat[512];
    snprintf(pat, sizeof pat, "%s\\*.hat", dir);
    WIN32_FIND_DATAA fd;
    HANDLE hf = FindFirstFileA(pat, &fd);
    if (hf == INVALID_HANDLE_VALUE) return 0;
    do {
        char path[600];
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        custom_hat_load_file(path);
    } while (FindNextFileA(hf, &fd));
    FindClose(hf);
    return g_custom_hat_count;
}
#else
#include <dirent.h>
static int custom_hats_scan(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l < 5 || strcmp(e->d_name + l - 4, ".hat") != 0) continue;
        char path[600];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        custom_hat_load_file(path);
    }
    closedir(d);
    return g_custom_hat_count;
}
#endif

/* Load every .hat file in hats/ once at startup (sorted-ness is not guaranteed by readdir; names are
 * only for display so order is irrelevant). */
static void custom_hats_init(void) {
    g_custom_hat_count = 0;
    const char *dir = getenv("BRAWLPIT_HATS_DIR");
    custom_hats_scan(dir && *dir ? dir : "hats");
}

#endif

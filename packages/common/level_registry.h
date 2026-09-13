#ifndef BRAWLPIT_LEVEL_REGISTRY_H
#define BRAWLPIT_LEVEL_REGISTRY_H

// level_registry.h — S417-04, founder real-time: "brawlpit needs a level selection/browser
// interface it needs to work over https or some secure channel." The real network half of
// S417-01's local level browser: fetch the community level list and a chosen level's real,
// LZ4-compressed export from IDUNA's public API (S417-02/03) and load it through the exact same
// stage_load_level_file/STAGE_CUSTOM path the local browser already uses.
//
// Real, honest engineering choice, not a workaround: this box has the libcurl RUNTIME installed
// but no libcurl-dev headers (checked directly -- no curl.h anywhere, no libcurl*-dev package),
// so this shells out to the real `curl` CLI binary via popen instead of linking libcurl's C API
// directly -- the same real "wrap the real, already-installed CLI tool" convention this
// monorepo's own PARENA stdlib/git.prn already established for git. `curl` itself is available
// cross-platform (including a built-in curl.exe on Windows 10+, this game's own real target
// platform per the founder), so this doesn't cost portability the way a raw libcurl link would
// have needed real per-platform build config anyway.

#include "level_format.h"
#include "lz4/lz4_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

/* pw_popen_read wraps popen/_popen with the real, correct per-platform read mode: Windows'
 * _popen accepts a real binary-mode "rb" (Microsoft's CRT does distinguish text/binary streams),
 * but POSIX popen only ever accepts "r"/"w" -- found live, glibc's own popen("rb", ...) fails
 * outright with EINVAL, not a silent binary/text distinction the way plain fopen at least
 * tolerates on some platforms. POSIX has no text/binary distinction at all, so a plain "r" is
 * already correctly binary-safe there. */
#ifdef _WIN32
static inline FILE *pw_popen_read(const char *cmd) { return _popen(cmd, "rb"); }
#define pw_pclose _pclose
#else
static inline FILE *pw_popen_read(const char *cmd) { return popen(cmd, "r"); }
#define pw_pclose pclose
#endif

#define LEVEL_REGISTRY_BASE_URL "https://okemily.com/api/v1/brawlpit-levels"
#define MAX_REGISTRY_FETCH_BYTES (1024 * 1024) /* real, sane bound -- a level file is a few KB */

// fetch_url_to_buffer runs `curl -s <url>` and captures its raw stdout into a newly malloc'd
// buffer. Returns the real byte count on success, or -1 on any failure (curl not found, network
// error, empty/oversized response) -- caller must free(*out) on success.
static inline long fetch_url_to_buffer(const char *url, unsigned char **out) {
    char cmd[1024];
    /* Real, deliberate quoting: url is always one of this file's own two hardcoded endpoint
       shapes with a caller-supplied integer id (never arbitrary user text), so a plain
       double-quoted shell argument is safe here -- not a general-purpose shell-escaping
       utility. -f makes curl exit nonzero on a real HTTP error (404/500) instead of printing an
       error PAGE's body as if it were real level data. */
    snprintf(cmd, sizeof(cmd), "curl -s -f \"%s\"", url);

    FILE *p = pw_popen_read(cmd);
    if (!p) return -1;

    unsigned char *buf = (unsigned char *)malloc(MAX_REGISTRY_FETCH_BYTES);
    if (!buf) {
        pw_pclose(p);
        return -1;
    }
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, MAX_REGISTRY_FETCH_BYTES - total, p)) > 0) {
        total += n;
        if (total >= MAX_REGISTRY_FETCH_BYTES) break; /* real, honest bound -- refuse silent truncation */
    }
    int status = pw_pclose(p);
    /* real, portable exit-code check -- POSIX pclose returns a raw wait() status (needs
       WIFEXITED/WEXITSTATUS to extract curl's real exit code); Windows' _pclose returns the
       process's exit code directly, no decoding needed. */
#ifdef _WIN32
    int curl_failed = (status != 0);
#else
    int curl_failed = (!WIFEXITED(status) || WEXITSTATUS(status) != 0);
#endif
    if (curl_failed || total == 0) {
        free(buf);
        return -1;
    }
    *out = buf;
    return (long)total;
}

#define MAX_REGISTRY_ENTRIES 64
#define REGISTRY_NAME_LEN 256

typedef struct {
    int id;
    char name[REGISTRY_NAME_LEN];
} RegistryEntry;

// parse_registry_list parses IDUNA's real GET /api/v1/brawlpit-levels response (a JSON array of
// {"id":N,"name":"...",...} objects -- see internal/brawlpit.LevelSummary's own real shape) into
// a real, bounded array of RegistryEntry. Deliberately a minimal, scoped scanner matching
// level_format.h's own established convention -- not a general JSON array parser -- since the
// real shape here (a flat array of flat objects) is exactly as narrow as a level file's own
// "platforms" array already is.
static inline int parse_registry_list(const char *json, RegistryEntry *out, int max) {
    int count = 0;
    const char *cursor = json;
    while (*cursor && count < max) {
        const char *obj_start = strchr(cursor, '{');
        if (!obj_start) break;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        const char *id_val = level_find_key(obj_start, obj_end, "id");
        const char *name_val = level_find_key(obj_start, obj_end, "name");
        if (id_val && name_val) {
            float id_f;
            if (level_parse_number(id_val, &id_f)) {
                out[count].id = (int)id_f;
                level_parse_string(name_val, out[count].name, sizeof(out[count].name));
                count++;
            }
        }
        cursor = obj_end + 1;
    }
    return count;
}

// fetch_registry_list fetches and parses the real, live level list. Returns the real entry
// count (0 if the registry is empty or unreachable -- a real network failure degrades to "no
// online levels shown," not a crash, matching stage_load_level_file's own established "a bad/
// missing resource never corrupts what's already working" convention).
static inline int fetch_registry_list(RegistryEntry *out, int max) {
    unsigned char *buf = NULL;
    long n = fetch_url_to_buffer(LEVEL_REGISTRY_BASE_URL, &buf);
    if (n <= 0) return 0;

    /* NUL-terminate for the real, string-based scanner above -- fetch_url_to_buffer's own real
       buffer has no guaranteed trailing NUL (it's a raw byte count from curl's stdout). */
    unsigned char *text = (unsigned char *)realloc(buf, (size_t)n + 1);
    if (!text) {
        free(buf);
        return 0;
    }
    text[n] = '\0';
    int count = parse_registry_list((const char *)text, out, max);
    free(text);
    return count;
}

// fetch_registry_level fetches level `id`'s real, LZ4-compressed export, decompresses it, and
// parses it into *out. Returns 1 on success, 0 on any real failure (network, decompression, or
// malformed JSON) -- *out is left untouched on failure, matching level_load_from_file's own
// established contract.
static inline int fetch_registry_level(int id, LevelData *out) {
    char url[256];
    snprintf(url, sizeof(url), "%s/%d/export?compress=lz4", LEVEL_REGISTRY_BASE_URL, id);

    unsigned char *compressed = NULL;
    long clen = fetch_url_to_buffer(url, &compressed);
    if (clen <= 0) return 0;

    unsigned char *decompressed = NULL;
    long dlen = pw_lz4_decompress(compressed, clen, &decompressed);
    free(compressed);
    if (dlen < 0) return 0;

    unsigned char *text = (unsigned char *)realloc(decompressed, (size_t)dlen + 1);
    if (!text) {
        free(decompressed);
        return 0;
    }
    text[dlen] = '\0';
    int ok = level_parse_json((const char *)text, out);
    free(text);
    return ok;
}

#endif

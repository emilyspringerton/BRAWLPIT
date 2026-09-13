#ifndef BRAWLPIT_LEVEL_FORMAT_H
#define BRAWLPIT_LEVEL_FORMAT_H

// level_format.h — S415-01 (founder real-time: "get the brawlpit level editor online - web
// technologies... parlay [NOCK] into an online brawlpit level editor"), the real, blocking
// Phase 0 prerequisite named in docs/BP_LEVEL_EDITOR_NORTHSTAR.md: BRAWLPIT levels were compiled
// C code, not data -- checked directly, no file format or runtime loader existed anywhere.
//
// Real, honest correction to that doc's own framing: `levels/*.c` (StageDef Battlefield/Final
// Destination/Weird Void) and `core/stage.h`/`core/selection_screen.c` are NOT actually compiled
// into the real client build (`apps/lobby/src/main.c` never includes `selection_screen.c` or
// `core/stage.h` at all) -- they're dead code. The REAL, live, tested collision geometry is
// `physics.h`'s own `stage_fd_geo`/`stage_timeline_geo` `Platform2D` arrays, selected via
// `stage_set_active`. This file's real scope is re-expressing THOSE as data, not the dead system.
//
// Format: a small, real, versioned JSON object -- {"version":1,"name":"...","platforms":
// [{"x":0,"y":-5,"w":60,"h":10,"type":0}, ...]}. Deliberately NOT a general-purpose JSON parser
// (no JSON library exists anywhere in this monorepo's C code, checked directly) -- a real, small,
// dependency-free scanner scoped exactly to this narrow, flat, fixed-field shape, robust to
// whitespace/field-order variation (a browser's `JSON.stringify` and a human hand-editing the
// file both produce valid input) but not attempting to parse arbitrary JSON.

#include "protocol.h" // Platform2D
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LEVEL_FORMAT_VERSION 1
#define MAX_LEVEL_PLATFORMS 64
#define MAX_LEVEL_NAME 64

typedef struct {
    char name[MAX_LEVEL_NAME];
    Platform2D platforms[MAX_LEVEL_PLATFORMS];
    int platform_count;
} LevelData;

// level_skip_ws advances p past any whitespace.
static inline const char *level_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

// level_find_key returns a pointer just past the colon following "key" inside [start, end), or
// NULL if not found -- a real, minimal key search (not a full tokenizer), robust to whichever
// order a real JSON serializer emits object keys in.
static inline const char *level_find_key(const char *start, const char *end, const char *key) {
    size_t keylen = strlen(key);
    char pattern[MAX_LEVEL_NAME + 2];
    if (keylen + 2 >= sizeof(pattern)) return NULL;
    pattern[0] = '"';
    memcpy(pattern + 1, key, keylen);
    pattern[keylen + 1] = '"';
    pattern[keylen + 2] = '\0';

    const char *p = start;
    while (p < end) {
        const char *found = strstr(p, pattern);
        if (!found || found >= end) return NULL;
        const char *after = found + keylen + 2;
        after = level_skip_ws(after);
        if (*after == ':') return level_skip_ws(after + 1);
        p = found + 1; // real key text appeared but wasn't followed by ':' -- keep scanning
    }
    return NULL;
}

// level_parse_number parses a real, plain JSON number (optional '-', digits, optional
// '.'+digits) starting at p. Returns 1 and sets *out on success, 0 if p doesn't start with a
// valid number.
static inline int level_parse_number(const char *p, float *out) {
    char *endptr = NULL;
    float v = strtof(p, &endptr);
    if (endptr == p) return 0;
    *out = v;
    return 1;
}

// level_parse_string extracts a real, unescaped-only JSON string value (no \" support needed --
// a level name never contains a quote) starting at the opening quote. Returns 1 on success,
// copying into out (size outsize, always null-terminated, truncated if too long rather than
// overflowing).
static inline int level_parse_string(const char *p, char *out, size_t outsize) {
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outsize) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return *p == '"';
}

// level_parse_json parses buf (a null-terminated JSON document) into *out. Returns 1 on success,
// 0 on any real failure (missing "platforms" array, a malformed platform object, or more
// platforms than MAX_LEVEL_PLATFORMS) -- *out is left partially written on failure, matching this
// codebase's own established "caller checks the return value, doesn't trust partial output"
// convention (e.g. idunaclient's own error-returning reads).
static inline int level_parse_json(const char *buf, LevelData *out) {
    memset(out, 0, sizeof(*out));
    size_t len = strlen(buf);
    const char *end = buf + len;

    const char *name_val = level_find_key(buf, end, "name");
    if (name_val) {
        level_parse_string(name_val, out->name, sizeof(out->name));
    } else {
        strncpy(out->name, "Untitled", sizeof(out->name) - 1);
    }

    const char *arr = level_find_key(buf, end, "platforms");
    if (!arr) return 0;
    arr = level_skip_ws(arr);
    if (*arr != '[') return 0;
    arr++;

    const char *arr_end = strchr(arr, ']');
    if (!arr_end) return 0;

    int count = 0;
    const char *cursor = arr;
    while (cursor < arr_end) {
        cursor = level_skip_ws(cursor);
        if (cursor >= arr_end) break;
        if (*cursor == ',') { cursor++; continue; }
        if (*cursor != '{') { cursor++; continue; }

        // Bound this one platform object to its own matching closing brace so level_find_key
        // never reads a later object's fields by accident.
        const char *obj_start = cursor;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end || obj_end > arr_end) return 0;

        if (count >= MAX_LEVEL_PLATFORMS) return 0; // real, honest bound -- refuse silent truncation

        float x = 0, y = 0, w = 0, h = 0, type_f = 0;
        const char *v;
        int ok = 1;
        if ((v = level_find_key(obj_start, obj_end, "x"))) ok &= level_parse_number(v, &x); else ok = 0;
        if ((v = level_find_key(obj_start, obj_end, "y"))) ok &= level_parse_number(v, &y); else ok = 0;
        if ((v = level_find_key(obj_start, obj_end, "w"))) ok &= level_parse_number(v, &w); else ok = 0;
        if ((v = level_find_key(obj_start, obj_end, "h"))) ok &= level_parse_number(v, &h); else ok = 0;
        if ((v = level_find_key(obj_start, obj_end, "type"))) ok &= level_parse_number(v, &type_f); else ok = 0;
        if (!ok) return 0;

        out->platforms[count].x = x;
        out->platforms[count].y = y;
        out->platforms[count].w = w;
        out->platforms[count].h = h;
        out->platforms[count].type = (int)type_f;
        count++;
        cursor = obj_end + 1;
    }

    out->platform_count = count;
    return 1;
}

// level_write_json serializes lvl into buf (bufsize bytes), always null-terminated. Returns the
// number of bytes written (excluding the null terminator) on success, or -1 if bufsize was too
// small to hold the real output -- the real, symmetric round-trip counterpart to
// level_parse_json, so a native tool (or a test) can also produce files the web editor reads.
static inline int level_write_json(const LevelData *lvl, char *buf, size_t bufsize) {
    int n = snprintf(buf, bufsize, "{\n  \"version\": %d,\n  \"name\": \"%s\",\n  \"platforms\": [\n",
                      LEVEL_FORMAT_VERSION, lvl->name);
    if (n < 0 || (size_t)n >= bufsize) return -1;
    size_t pos = (size_t)n;

    for (int i = 0; i < lvl->platform_count; i++) {
        const Platform2D *p = &lvl->platforms[i];
        n = snprintf(buf + pos, bufsize - pos,
                     "    {\"x\": %g, \"y\": %g, \"w\": %g, \"h\": %g, \"type\": %d}%s\n",
                     p->x, p->y, p->w, p->h, p->type,
                     (i + 1 < lvl->platform_count) ? "," : "");
        if (n < 0 || (size_t)n >= bufsize - pos) return -1;
        pos += (size_t)n;
    }

    n = snprintf(buf + pos, bufsize - pos, "  ]\n}\n");
    if (n < 0 || (size_t)n >= bufsize - pos) return -1;
    pos += (size_t)n;
    return (int)pos;
}

// level_load_from_file reads path and parses it into *out. Returns 1 on success, 0 if the file
// doesn't exist, can't be read, or fails to parse -- *out is left untouched on any failure so a
// caller can safely fall back to a compiled-in default (see physics.h's own stage_set_active).
static inline int level_load_from_file(const char *path, LevelData *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 65536) { fclose(f); return 0; } // real, sane bound on a level file
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return 0; }
    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read] = '\0';

    LevelData tmp;
    int ok = level_parse_json(buf, &tmp);
    free(buf);
    if (!ok) return 0;
    *out = tmp;
    return 1;
}

// level_save_to_file serializes lvl and writes it to path. Returns 1 on success, 0 on any
// real I/O or buffer-size failure.
static inline int level_save_to_file(const LevelData *lvl, const char *path) {
    char buf[16384];
    int n = level_write_json(lvl, buf, sizeof(buf));
    if (n < 0) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t written = fwrite(buf, 1, (size_t)n, f);
    fclose(f);
    return written == (size_t)n;
}

#endif

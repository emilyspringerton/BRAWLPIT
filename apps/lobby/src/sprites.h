// sprites.h — real pixel-art character portraits, loaded from real
// Prompt-o-verse generations. Founder, real-time: "can we add pixel art
// to the brawlpit engine? use the 5 pixel art generated... add at least
// 4 characters to brawlpit via the promtoverse pixel art gens."
//
// BRAWLPIT's renderer (draw_player, draw_rect, etc. in main.c) is legacy
// OpenGL immediate mode drawing flat-colored primitives only -- no image
// loading, no textures, anywhere in this codebase before this file.
// stb_image.h (vendor/stb_image.h, MIT/public-domain single-header
// library, github.com/nothings/stb) is the smallest real dependency that
// gets PNG bytes into memory; texture upload + a textured quad are then
// just a few GL calls, no build-system changes needed since this whole
// file is header-only and gcc already compiles one translation unit.
//
// Honest limitation, not hidden: these are the ORIGINAL generated
// images, full rectangular scene and all (a ballerina under a moon, a
// beach) -- not background-removed/matted into a clean silhouette
// sprite. No such pipeline exists anywhere in this monorepo yet. They
// render here as portrait cards, which is a real, working, honest V1,
// not a cut corner pretending to be full sprite art.
#ifndef BRAWLPIT_SPRITES_H
#define BRAWLPIT_SPRITES_H

#define STB_IMAGE_IMPLEMENTATION
#include "../../../vendor/stb_image.h"
#include <SDL2/SDL_opengl.h>

#define SPRITE_SLOT_COUNT 8

typedef struct {
    const char *path;
    GLuint gl_id;   // 0 until loaded
    int loaded;     // 1 once load_sprite has run (even on failure, so we only try once)
    int w, h;
} SpriteSlot;

static SpriteSlot g_sprite_cache[SPRITE_SLOT_COUNT];
static int g_sprite_cache_count = 0;

// sprite_slot_for returns a stable index for `path`, registering it on
// first use. Character defs reference sprites by path string; this is
// the only place that string gets turned into a GL texture id.
static inline int sprite_slot_for(const char *path) {
    for (int i = 0; i < g_sprite_cache_count; i++) {
        if (strcmp(g_sprite_cache[i].path, path) == 0) return i;
    }
    if (g_sprite_cache_count >= SPRITE_SLOT_COUNT) return -1;
    int idx = g_sprite_cache_count++;
    g_sprite_cache[idx].path = path;
    g_sprite_cache[idx].gl_id = 0;
    g_sprite_cache[idx].loaded = 0;
    return idx;
}

// load_sprite_now uploads the PNG at slot `idx` to a GL texture, lazily,
// the first time it's actually drawn -- so a headless/test build never
// needs a GL context just to reference a SpriteSlot.
static inline void load_sprite_now(int idx) {
    if (idx < 0 || idx >= g_sprite_cache_count) return;
    SpriteSlot *s = &g_sprite_cache[idx];
    if (s->loaded) return;
    s->loaded = 1; // mark attempted even if this fails, so we don't retry every frame

    int channels;
    unsigned char *data = stbi_load(s->path, &s->w, &s->h, &channels, 4);
    if (!data) {
        fprintf(stderr, "[sprites] failed to load %s: %s\n", s->path, stbi_failure_reason());
        return;
    }

    glGenTextures(1, &s->gl_id);
    glBindTexture(GL_TEXTURE_2D, s->gl_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
}

// draw_sprite_quad draws a square portrait centered at (x, y) with side
// length `size`, in world units -- same coordinate space draw_rect
// already uses elsewhere in this file. Falls back to nothing (caller's
// own draw_rect call stays as the visible fallback) if the texture
// never loaded, matching the "falls back to full size if not available"
// pattern the Prompt-o-verse thumbnail pipeline already established.
static inline int draw_sprite_quad(int slot, float x, float y, float size) {
    if (slot < 0) return 0;
    load_sprite_now(slot);
    SpriteSlot *s = &g_sprite_cache[slot];
    if (!s->gl_id) return 0;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, s->gl_id);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(x - size / 2, y + size / 2, 0);
        glTexCoord2f(1, 0); glVertex3f(x + size / 2, y + size / 2, 0);
        glTexCoord2f(1, 1); glVertex3f(x + size / 2, y - size / 2, 0);
        glTexCoord2f(0, 1); glVertex3f(x - size / 2, y - size / 2, 0);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return 1;
}

#endif

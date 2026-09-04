#ifndef TEXT_H
#define TEXT_H

#include <ctype.h>
#include <math.h>

#include <SDL2/SDL_opengl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
} TextSegment;

typedef struct {
    const TextSegment *segments;
    int count;
} TextGlyph;

static inline void text_draw_segments(const TextSegment *segments, int count, float x, float y, float s) {
    glBegin(GL_LINES);
    for (int i = 0; i < count; i++) {
        const TextSegment seg = segments[i];
        glVertex2f(x + seg.x1 * s, y + seg.y1 * s);
        glVertex2f(x + seg.x2 * s, y + seg.y2 * s);
    }
    glEnd();
}

static inline TextGlyph text_lookup_glyph(char raw) {
    static const TextSegment glyph_0[] = {{0,0, 1,0}, {1,0, 1,1}, {1,1, 0,1}, {0,1, 0,0}};
    static const TextSegment glyph_1[] = {{0.5f,0, 0.5f,1}};
    static const TextSegment glyph_2[] = {{0,1, 1,1}, {1,1, 1,0.5f}, {1,0.5f, 0,0.5f}, {0,0.5f, 0,0}, {0,0, 1,0}};
    static const TextSegment glyph_3[] = {{0,1, 1,1}, {1,1, 1,0}, {0,0.5f, 1,0.5f}, {0,0, 1,0}};
    static const TextSegment glyph_4[] = {{0,1, 0,0.5f}, {0,0.5f, 1,0.5f}, {1,1, 1,0}};
    static const TextSegment glyph_5[] = {{1,1, 0,1}, {0,1, 0,0.5f}, {0,0.5f, 1,0.5f}, {1,0.5f, 1,0}, {1,0, 0,0}};
    static const TextSegment glyph_6[] = {{1,1, 0,1}, {0,1, 0,0}, {0,0, 1,0}, {1,0, 1,0.5f}, {1,0.5f, 0,0.5f}};
    static const TextSegment glyph_7[] = {{0,1, 1,1}, {1,1, 0.4f,0}};
    static const TextSegment glyph_8[] = {{0,0, 1,0}, {1,0, 1,1}, {1,1, 0,1}, {0,1, 0,0}, {0,0.5f, 1,0.5f}};
    static const TextSegment glyph_9[] = {{1,0, 1,1}, {1,1, 0,1}, {0,1, 0,0.5f}, {0,0.5f, 1,0.5f}};

    static const TextSegment glyph_a[] = {{0,0, 0,1}, {0,1, 1,1}, {1,1, 1,0}, {0,0.5f, 1,0.5f}};
    static const TextSegment glyph_b[] = {{0,0, 0,1}, {0,1, 0.8f,1}, {0.8f,1, 1,0.8f}, {1,0.8f, 1,0.2f}, {1,0.2f, 0.8f,0}, {0.8f,0, 0,0}, {0,0.5f, 0.8f,0.5f}};
    static const TextSegment glyph_c[] = {{1,1, 0,1}, {0,1, 0,0}, {0,0, 1,0}};
    static const TextSegment glyph_d[] = {{0,0, 0,1}, {0,1, 0.8f,1}, {0.8f,1, 1,0.8f}, {1,0.8f, 1,0.2f}, {1,0.2f, 0.8f,0}, {0.8f,0, 0,0}};
    static const TextSegment glyph_e[] = {{1,1, 0,1}, {0,1, 0,0}, {0,0, 1,0}, {0,0.5f, 0.8f,0.5f}};
    static const TextSegment glyph_f[] = {{0,0, 0,1}, {0,1, 1,1}, {0,0.5f, 0.8f,0.5f}};
    static const TextSegment glyph_g[] = {{1,1, 0,1}, {0,1, 0,0}, {0,0, 1,0}, {1,0, 1,0.5f}, {1,0.5f, 0.5f,0.5f}};
    static const TextSegment glyph_h[] = {{0,0, 0,1}, {1,0, 1,1}, {0,0.5f, 1,0.5f}};
    static const TextSegment glyph_i[] = {{0.5f,0, 0.5f,1}, {0.2f,1, 0.8f,1}, {0.2f,0, 0.8f,0}};
    static const TextSegment glyph_j[] = {{0.2f,1, 0.8f,1}, {0.5f,1, 0.5f,0.1f}, {0.5f,0.1f, 0.2f,0}};
    static const TextSegment glyph_k[] = {{0,0, 0,1}, {1,1, 0,0.5f}, {0,0.5f, 1,0}};
    static const TextSegment glyph_l[] = {{0,1, 0,0}, {0,0, 1,0}};
    static const TextSegment glyph_m[] = {{0,0, 0,1}, {0,1, 0.5f,0.5f}, {0.5f,0.5f, 1,1}, {1,1, 1,0}};
    static const TextSegment glyph_n[] = {{0,0, 0,1}, {0,1, 1,0}, {1,0, 1,1}};
    static const TextSegment glyph_o[] = {{0,0, 1,0}, {1,0, 1,1}, {1,1, 0,1}, {0,1, 0,0}};
    static const TextSegment glyph_p[] = {{0,0, 0,1}, {0,1, 1,1}, {1,1, 1,0.5f}, {1,0.5f, 0,0.5f}};
    static const TextSegment glyph_q[] = {{0,0, 1,0}, {1,0, 1,1}, {1,1, 0,1}, {0,1, 0,0}, {0.6f,0.4f, 1,0}};
    static const TextSegment glyph_r[] = {{0,0, 0,1}, {0,1, 1,1}, {1,1, 1,0.6f}, {1,0.6f, 0,0.6f}, {0,0.6f, 1,0}};
    static const TextSegment glyph_s[] = {{1,1, 0,1}, {0,1, 0,0.5f}, {0,0.5f, 1,0.5f}, {1,0.5f, 1,0}, {1,0, 0,0}};
    static const TextSegment glyph_t[] = {{0.5f,1, 0.5f,0}, {0,1, 1,1}};
    static const TextSegment glyph_u[] = {{0,1, 0,0}, {0,0, 1,0}, {1,0, 1,1}};
    static const TextSegment glyph_v[] = {{0,1, 0.5f,0}, {0.5f,0, 1,1}};
    static const TextSegment glyph_w[] = {{0,1, 0.25f,0}, {0.25f,0, 0.5f,0.6f}, {0.5f,0.6f, 0.75f,0}, {0.75f,0, 1,1}};
    static const TextSegment glyph_x[] = {{0,1, 1,0}, {0,0, 1,1}};
    static const TextSegment glyph_y[] = {{0,1, 0.5f,0.5f}, {1,1, 0.5f,0.5f}, {0.5f,0.5f, 0.5f,0}};
    static const TextSegment glyph_z[] = {{0,1, 1,1}, {1,1, 0,0}, {0,0, 1,0}};

    static const TextSegment glyph_dash[] = {{0,0.5f, 1,0.5f}};
    static const TextSegment glyph_dot[] = {{0.5f,0, 0.5f,0.1f}};
    static const TextSegment glyph_colon[] = {{0.5f,0.75f, 0.5f,0.85f}, {0.5f,0.15f, 0.5f,0.25f}};
    static const TextSegment glyph_percent[] = {{0,0, 1,1}, {0.2f,0.8f, 0.3f,0.9f}, {0.7f,0.1f, 0.8f,0.2f}};
    static const TextSegment glyph_space[] = {};
    static const TextSegment glyph_question[] = {{0,1, 1,1}, {1,1, 1,0.6f}, {1,0.6f, 0.5f,0.4f}, {0.5f,0.4f, 0.5f,0.2f}, {0.5f,0.05f, 0.5f,0}};
    static const TextSegment glyph_slash[] = {{0,0, 1,1}};
    /* BPUX-12444 drive-by: '(' and ')' were real, missing glyphs -- every caller passing either
       (e.g. "VS BOT (STAGE 1)", "FIND MATCH (8 PLAYER)") fell through to default and silently
       rendered a '?' instead, found live via a real Xvfb screenshot of this exact card's own
       fix. 3-segment curve approximation, same stroke-count style as the digit/letter glyphs
       above (not a smooth arc -- this is a straight-line-segment font). */
    static const TextSegment glyph_lparen[] = {{0.6f,1, 0.2f,0.75f}, {0.2f,0.75f, 0.2f,0.25f}, {0.2f,0.25f, 0.6f,0}};
    static const TextSegment glyph_rparen[] = {{0.2f,1, 0.6f,0.75f}, {0.6f,0.75f, 0.6f,0.25f}, {0.6f,0.25f, 0.2f,0}};

    char c = (char)toupper((unsigned char)raw);
    switch (c) {
        case '0': return (TextGlyph){glyph_0, 4};
        case '1': return (TextGlyph){glyph_1, 1};
        case '2': return (TextGlyph){glyph_2, 5};
        case '3': return (TextGlyph){glyph_3, 4};
        case '4': return (TextGlyph){glyph_4, 3};
        case '5': return (TextGlyph){glyph_5, 5};
        case '6': return (TextGlyph){glyph_6, 5};
        case '7': return (TextGlyph){glyph_7, 2};
        case '8': return (TextGlyph){glyph_8, 5};
        case '9': return (TextGlyph){glyph_9, 4};
        case 'A': return (TextGlyph){glyph_a, 4};
        case 'B': return (TextGlyph){glyph_b, 7};
        case 'C': return (TextGlyph){glyph_c, 3};
        case 'D': return (TextGlyph){glyph_d, 6};
        case 'E': return (TextGlyph){glyph_e, 4};
        case 'F': return (TextGlyph){glyph_f, 3};
        case 'G': return (TextGlyph){glyph_g, 5};
        case 'H': return (TextGlyph){glyph_h, 3};
        case 'I': return (TextGlyph){glyph_i, 3};
        case 'J': return (TextGlyph){glyph_j, 3};
        case 'K': return (TextGlyph){glyph_k, 3};
        case 'L': return (TextGlyph){glyph_l, 2};
        case 'M': return (TextGlyph){glyph_m, 4};
        case 'N': return (TextGlyph){glyph_n, 3};
        case 'O': return (TextGlyph){glyph_o, 4};
        case 'P': return (TextGlyph){glyph_p, 4};
        case 'Q': return (TextGlyph){glyph_q, 5};
        case 'R': return (TextGlyph){glyph_r, 5};
        case 'S': return (TextGlyph){glyph_s, 5};
        case 'T': return (TextGlyph){glyph_t, 2};
        case 'U': return (TextGlyph){glyph_u, 3};
        case 'V': return (TextGlyph){glyph_v, 2};
        case 'W': return (TextGlyph){glyph_w, 4};
        case 'X': return (TextGlyph){glyph_x, 2};
        case 'Y': return (TextGlyph){glyph_y, 3};
        case 'Z': return (TextGlyph){glyph_z, 3};
        case '-': return (TextGlyph){glyph_dash, 1};
        case '.': return (TextGlyph){glyph_dot, 1};
        case ':': return (TextGlyph){glyph_colon, 2};
        case '%': return (TextGlyph){glyph_percent, 3};
        case '/': return (TextGlyph){glyph_slash, 1};
        case '?': return (TextGlyph){glyph_question, 5};
        case '(': return (TextGlyph){glyph_lparen, 3};
        case ')': return (TextGlyph){glyph_rparen, 3};
        case ' ': return (TextGlyph){glyph_space, 0};
        default:  return (TextGlyph){glyph_question, 5};
    }
}

static inline void text_draw_char(char c, float x, float y, float size, float thickness) {
    TextGlyph glyph = text_lookup_glyph(c);
    glLineWidth(thickness);
    if (glyph.count > 0) {
        text_draw_segments(glyph.segments, glyph.count, x, y, size);
    }
}

static inline void text_draw_string(const char *str, float x, float y, float size, float spacing, float thickness) {
    for (const char *ch = str; *ch; ch++) {
        text_draw_char(*ch, x, y, size, thickness);
        x += size * spacing;
    }
}

/* text_draw_string_shadowed (BPUX-12444, "cant really read the words... can we make the font
 * nicer") -- real, decisive finding: this vector stick-letter glyph system is real and legible
 * up close, but has zero contrast handling -- a light-colored word drawn directly over a
 * similarly-light stage background (or another player's own bright sprite/effect) genuinely
 * disappears, since every glyph is a bare 1-2px line stroke with nothing behind it. Real,
 * low-risk fix, matching a real technique already live in this exact monorepo (SHANKPIT's own
 * apps/lobby/src/main.c draw_lobby_buttons -- draws every button label as a black copy offset
 * by (2,-2) THEN the real bright color on top, a real drop-shadow, not a new invention here):
 * draws one dark backing copy of the whole string offset by a small amount, then the caller's
 * own already-set current color on top, unchanged. Real, minimal API: takes the shadow color and
 * offset explicitly rather than hardcoding SHANKPIT's own literal (2,-2) black, since BRAWLPIT's
 * own screens run at very different point sizes than SHANKPIT's 3D HUD overlay. */
static inline void text_draw_string_shadowed(const char *str, float x, float y, float size,
                                              float spacing, float thickness,
                                              float shadow_offset) {
    float cur[4];
    glGetFloatv(GL_CURRENT_COLOR, cur);
    glColor4f(0.0f, 0.0f, 0.0f, cur[3]);
    text_draw_string(str, x + shadow_offset, y - shadow_offset, size, spacing, thickness);
    glColor4f(cur[0], cur[1], cur[2], cur[3]);
    text_draw_string(str, x, y, size, spacing, thickness);
}

#ifdef __cplusplus
}
#endif

#endif

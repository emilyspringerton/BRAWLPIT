#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>

#include "../../../packages/common/protocol.h"
#include "../../../packages/common/physics.h"
#include "../../../packages/common/text.h"
#include "../../../packages/simulation/local_game.h"
#include "../../../packages/common/characters.h"
#include "../../../packages/simulation/tipjar.h"
#include "sprites.h"

#define PAD_STICK_DEADZONE 0.22f
#define PAD_MOVE_THRESHOLD 0.18f
#define PAD_MOVE_HYSTERESIS 0.04f
#define PAD_MOVE_SNAP_THRESHOLD 0.85f
#define PAD_TRIGGER_THRESHOLD 0.25f

#define STATE_LOBBY 0
#define STATE_GAME_NET 1
#define STATE_GAME_LOCAL 2
#define STATE_RESULTS 3
#define STATE_CHARACTER_SELECT 4
#define STATE_TIPJAR 5
/* S248-02 (BP-LOBBY-001 Phase 2). Real, honest scope correction from the northstar's own
 * original framing: STATE_LOBBY has always been a flat 2D text menu (checked directly -- no
 * walkable 3D avatar/scene exists here, unlike GFD's own real Town), so "a physical trigger
 * volume you walk into" doesn't fit this game's actual UI paradigm. The real, working
 * equivalent given what's actually here: a new menu option (same real "letter key selects a
 * mode" convention D/F/J/T already use), not a new 3D navigable space. */
#define STATE_MATCHMAKING 6

typedef struct {
    SDL_GameController *handle;
    SDL_JoystickID instance_id;
    int connected;

    float lx, ly;
    float rx, ry;
    float lt, rt;

    Uint8 a, b, x, y;
    Uint8 lb, rb;
    Uint8 back, start;
    Uint8 l3, r3;
    Uint8 dpad_up, dpad_down, dpad_left, dpad_right;

    Uint8 prev_a, prev_b, prev_x, prev_y;
    Uint8 prev_lb, prev_rb;
    Uint8 prev_back, prev_start;
    Uint8 prev_l3, prev_r3;
    Uint8 prev_dpad_up, prev_dpad_down, prev_dpad_left, prev_dpad_right;

    int move_active_x;
} ControllerState;

char SERVER_HOST[64] = "127.0.0.1";
/* BPMM-12441/12442: matches apps/server/src/main.c's own real port move off 6969 (a real,
 * live collision with SHANKPIT's own server on this host -- see that file's server_net_init
 * doc comment for the full root-cause writeup). */
int SERVER_PORT = 6978;
int app_state = STATE_LOBBY;
int my_client_id = -1;
int winner_id = -1;
int last_mode = MODE_STOCK;
int last_num_players = 2;
int last_app_state = STATE_GAME_LOCAL;
int last_stage_id = STAGE_FD;
CharacterId selected_chars[2] = { CHARACTER_PETALIA, CHARACTER_VEXAR };
int select_cursor = 0;
int select_confirmed[2] = {0,0};
ControllerState g_pad = {0};
/* Step 5 (2026-09-02) -- a second physical pad, so two humans can each play TIPJAR on their
   own controller instead of one being stuck on keyboard. See try_open_first_two_controllers
   and the TIPJAR pad-assignment note in STATE_TIPJAR below. */
ControllerState g_pad2 = {0};
int g_pad_debug = 0;
unsigned int g_last_pad_debug_log_ms = 0;

int sock = -1;
struct sockaddr_in server_addr;
/* S248-00 (real netcode, blocking prerequisite for BP-LOBBY-001's own matchmaking portal ask) --
 * client_id assigned by the server's own PACKET_WELCOME reply; -1 until that arrives, so
 * net_send_cmd knows not to address packets to a slot we don't own yet. */
int g_net_client_id = -1;
unsigned short g_net_send_seq = 0;
/* S248-02: real, live queue status -- -1 means "no status received yet" (still waiting on the
 * first server reply), distinct from a real 0/MATCHMAKING_MAX_QUEUE. g_net_last_poll_ms paces
 * the periodic re-send of PACKET_FIND_MATCH while waiting (see the STATE_MATCHMAKING update
 * block) so the client isn't spamming the server every frame. */
int g_net_queue_count = -1;
unsigned int g_net_last_poll_ms = 0;
#define MATCHMAKING_POLL_INTERVAL_MS 1000

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static float safe_norm_axis(Sint16 value) {
    float n = (value >= 0) ? (value / 32767.0f) : (value / 32768.0f);
    if (isnan(n) || isinf(n)) return 0.0f;
    return clampf(n, -1.0f, 1.0f);
}

static float safe_norm_trigger(Sint16 value) {
    float n = value / 32767.0f;
    if (isnan(n) || isinf(n)) return 0.0f;
    return clampf(n, 0.0f, 1.0f);
}

static void close_controller(ControllerState *pad) {
    if (!pad) return;
    if (pad->handle) {
        SDL_GameControllerClose(pad->handle);
    }
    memset(pad, 0, sizeof(*pad));
    pad->instance_id = -1;
}

static void try_open_controller_index(ControllerState *pad, int idx) {
    if (!pad || pad->connected || idx < 0 || !SDL_IsGameController(idx)) return;
    SDL_GameController *gc = SDL_GameControllerOpen(idx);
    if (!gc) {
        printf("[PAD] Failed to open controller index %d: %s\n", idx, SDL_GetError());
        return;
    }
    SDL_Joystick *joy = SDL_GameControllerGetJoystick(gc);
    pad->handle = gc;
    pad->instance_id = SDL_JoystickInstanceID(joy);
    pad->connected = 1;
    pad->move_active_x = 0;
    printf("[PAD] Connected: %s (instance=%d)\n", SDL_GameControllerName(gc), pad->instance_id);
}

static void try_open_first_controller(ControllerState *pad) {
    if (!pad || pad->connected) return;
    int joystick_count = SDL_NumJoysticks();
    for (int i = 0; i < joystick_count; i++) {
        if (SDL_IsGameController(i)) {
            try_open_controller_index(pad, i);
            if (pad->connected) return;
        }
    }
}

/* Step 5 (2026-09-02) -- startup scan for up to two pads, one per struct, each getting a
   distinct joystick index (unlike calling try_open_first_controller twice, which would hand
   both structs the SAME physical device since neither knows what the other already opened). */
static void try_open_first_two_controllers(ControllerState *pad1, ControllerState *pad2) {
    int joystick_count = SDL_NumJoysticks();
    for (int i = 0; i < joystick_count; i++) {
        if (!SDL_IsGameController(i)) continue;
        if (pad1 && !pad1->connected) {
            try_open_controller_index(pad1, i);
        } else if (pad2 && !pad2->connected) {
            try_open_controller_index(pad2, i);
        }
        if ((!pad1 || pad1->connected) && (!pad2 || pad2->connected)) break;
    }
}

static void poll_controller_state(ControllerState *pad) {
    if (!pad || !pad->connected || !pad->handle) return;

    pad->prev_a = pad->a; pad->prev_b = pad->b; pad->prev_x = pad->x; pad->prev_y = pad->y;
    pad->prev_lb = pad->lb; pad->prev_rb = pad->rb;
    pad->prev_back = pad->back; pad->prev_start = pad->start;
    pad->prev_l3 = pad->l3; pad->prev_r3 = pad->r3;
    pad->prev_dpad_up = pad->dpad_up; pad->prev_dpad_down = pad->dpad_down;
    pad->prev_dpad_left = pad->dpad_left; pad->prev_dpad_right = pad->dpad_right;

    pad->lx = safe_norm_axis(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_LEFTX));
    pad->ly = safe_norm_axis(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_LEFTY));
    pad->rx = safe_norm_axis(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_RIGHTX));
    pad->ry = safe_norm_axis(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_RIGHTY));
    pad->lt = safe_norm_trigger(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
    pad->rt = safe_norm_trigger(SDL_GameControllerGetAxis(pad->handle, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));

    pad->a = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_A);
    pad->b = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_B);
    pad->x = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_X);
    pad->y = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_Y);
    pad->lb = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    pad->rb = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    pad->back = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_BACK);
    pad->start = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_START);
    pad->l3 = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_LEFTSTICK);
    pad->r3 = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    pad->dpad_up = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_DPAD_UP);
    pad->dpad_down = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    pad->dpad_left = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    pad->dpad_right = SDL_GameControllerGetButton(pad->handle, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
}

/* Step 5 (2026-09-02) -- merges one pad's input into one TIPJAR player's raw input variables,
   the same merge rule the old single-pad code used (stronger stick magnitude wins over
   keyboard, buttons OR together). `pad` may be NULL (no pad assigned to this player this
   frame) -- a no-op, so keyboard-only players are unaffected. Shared by both P1 and P2 so the
   two-pad case (see STATE_TIPJAR below) doesn't need to duplicate this logic. */
static void apply_pad_to_tipjar_input(ControllerState *pad, float *sx, int *jump,
                                       int *deliver_raw, int *bubble_raw, int *shield_held,
                                       int *want_lobby) {
    if (!pad || !pad->connected) return;
    float pad_x = pad->lx;
    if (fabsf(pad_x) <= PAD_STICK_DEADZONE) pad_x = 0.0f;
    if (pad->dpad_left) pad_x = -1.0f;
    if (pad->dpad_right) pad_x = 1.0f;
    if (fabsf(pad_x) >= fabsf(*sx)) *sx = pad_x;
    *jump = *jump || pad->a;
    *deliver_raw = *deliver_raw || pad->x || (pad->rt > PAD_TRIGGER_THRESHOLD);
    *bubble_raw = *bubble_raw || pad->b || pad->rb;
    *shield_held = *shield_held || pad->lb || (pad->lt > PAD_TRIGGER_THRESHOLD);
    if (pad->start) *want_lobby = 1;
}

// --- RENDERING HELPERS ---
void draw_rect(float x, float y, float w, float h, float r, float g, float b, int fill) {
    glColor3f(r, g, b);
    if(fill) glBegin(GL_QUADS); else glBegin(GL_LINE_LOOP);
    glVertex3f(x - w/2, y + h/2, 0);
    glVertex3f(x + w/2, y + h/2, 0);
    glVertex3f(x + w/2, y - h/2, 0);
    glVertex3f(x - w/2, y - h/2, 0);
    glEnd();
}

void draw_circle(float x, float y, float radius, float r, float g, float b, int segments) {
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    for(int i=0; i<segments; i++) {
        float theta = 2.0f * 3.14159f * (float)i / (float)segments;
        glVertex3f(x + radius * cosf(theta), y + radius * sinf(theta), 0);
    }
    glEnd();
}

static inline void draw_string(const char *str, float x, float y, float size) {
    text_draw_string(str, x, y, size, 1.4f, 2.0f);
}

void draw_hud(PlayerState *p) {
    // 2D Overlay
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, 1280, 0, 720);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    // Draw Percentages for all players
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *t = &local_state.players[i];
        if(!t->active) continue;

        float hud_x = 100 + (i * 150);
        float hud_y = 50;
        
        char buf[32];
        
        // Damage %
        glColor3f(1.0f, (100.0f - t->damage_percent)/100.0f, 0.0f);
        sprintf(buf, "%.0f", t->damage_percent);
        draw_string(buf, hud_x, hud_y, 20);
        draw_string("%", hud_x + 60, hud_y, 10);
        
        // Stocks
        glColor3f(0.0f, 1.0f, 1.0f);
        for(int s=0; s<t->stocks; s++) {
            draw_circle(hud_x + (s*15), hud_y - 20, 5, 0, 1, 1, 8);
        }
        
        // Indicator arrow above player
        // World to Screen conversion is painful here without matrices, 
        // so we just draw percentages at bottom (Smash style)
    }

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
}

void draw_stage() {
    for(int i=0; i<stage_count; i++) {
        Platform p = stage_geo[i];
        // Neon Brutalist Look: Dark fill, bright rim
        draw_rect(p.x, p.y, p.w, p.h, 0.1f, 0.1f, 0.1f, 1);
        
        float r=0,g=0,b=0;
        if(p.type == 0) { r=0.0f; g=1.0f; b=1.0f; } // Solid (Cyan)
        else { r=1.0f; g=0.0f; b=1.0f; } // Passthrough (Magenta)
        
        draw_rect(p.x, p.y, p.w, p.h, r, g, b, 0);
    }
    
    // Blast Zones (Visual Guide)
    glLineWidth(1.0f);
    draw_rect((BLAST_LEFT+BLAST_RIGHT)/2, (BLAST_TOP+BLAST_BOTTOM)/2, 
              BLAST_RIGHT-BLAST_LEFT, BLAST_TOP-BLAST_BOTTOM, 0.2f, 0.0f, 0.0f, 0);
}

/* Mirror-match hat (kanban priority-queue card 342342, "we need hats for the brawlpit
 * characters for mirror matches"): real gap found live reading this function's own body --
 * every player's draw color/sprite comes from fd->body_r/g/b / fd->sprite_path (a per-CHARACTER
 * choice), never per-PLAYER, despite this function's own stale comment claiming "color based on
 * player id." When both players pick the same fighter, they render pixel-identical with nothing
 * distinguishing them mid-fight. wears_hat is true for any player slot after slot 0 whose
 * character_id matches slot 0's own -- a real, minimal, "no new art asset needed" fix using the
 * same draw_circle/draw_rect primitives already used for CHARACTER_PETALIA's own umbrella-open
 * accent circle just below. Honest v0 scope: only differentiates against slot 0, matching the
 * real 2-player case this card names -- a genuine 3-4P free-for-all where several slots collide
 * would need a real per-slot color/accessory table, separate, larger, later work. */
void draw_player(PlayerState *p, int player_index) {
    if(p->state == STATE_DEAD) return;
    int wears_mirror_hat = (player_index > 0 && p->character_id == local_state.players[0].character_id);

    glPushMatrix();
    glTranslatef(p->x, p->y, 0);

    // Facing Flip
    if (p->facing < 0) glScalef(-1, 1, 1);

    // Color based on player id (Synthwave palette baseline)
    const FighterDef *fd = fighter_def((CharacterId)p->character_id);
    float r=fd->body_r, g=fd->body_g, b=fd->body_b;
    if (p->state == STATE_STUNNED) { r=1; g=1; b=0; } // Yellow Stun
    if (p->invuln_frames > 0 && (SDL_GetTicks()/50)%2==0) { r=0.5f; g=0.5f; b=0.5f; } // Flicker
    if (p->hit_flash_timer > 0) {
        if (p->hit_flash_multihit) {
            float pulse = sinf((float)p->hit_flash_timer * 0.5f) * 0.5f + 0.5f;
            r = 1.0f;
            g = 1.0f;
            b = 1.0f - (0.8f * pulse);
        } else {
            r = 1.0f;
            g = 1.0f;
            b = 1.0f;
        }
    }

    // Sprite characters (real Prompt-o-verse pixel-art portraits) draw a
    // textured portrait quad instead of the flat-color silhouettes below.
    // draw_rect stays as the drawn fallback if the texture ever fails to
    // load (missing file, bad PNG) -- same "falls back to full size if
    // not available" discipline the Prompt-o-verse thumbnail pipeline
    // already established, never a blank/invisible player.
    int drew_sprite = 0;
    if (fd->sprite_path[0] != '\0') {
        int slot = sprite_slot_for(fd->sprite_path);
        drew_sprite = draw_sprite_quad(slot, 0, 2.4f, 4.4f);
    }

    if (drew_sprite) {
        // no-op: portrait already drawn
    } else if (p->character_id == CHARACTER_PETALIA) {
        draw_rect(0, 2, 1.8f, 3.6f, r, g, b, 1);
        draw_circle(0, 4.4f, 1.0f, fd->accent_r, fd->accent_g, fd->accent_b, 14);
    } else {
        draw_rect(0, 2, 2.3f, 4.2f, r, g, b, 1);
        draw_rect(0.2f, 3.1f, 1.0f, 0.35f, 1.0f, 0.52f, 0.1f, 1);
        draw_rect(1.5f, 2.2f, 1.1f, 0.9f, 0.85f, 0.95f, 1.0f, 0);
    }
    
    // Eye (Direction indicator)
    draw_rect(0.5f, 3.0f, 0.5f, 0.5f, 0, 0, 0, 1);

    // Shield Bubble
    if (p->state == STATE_SHIELD) {
        float shield_ratio = p->shield_health / (float)SHIELD_MAX;
        float alpha = 0.4f + 0.3f * shield_ratio;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(r, g, b, alpha);
        float size = 3.5f * shield_ratio;
        if (size < 1.0f) size = 1.0f;
        if (p->shield_health < 10.0f && (SDL_GetTicks()/80)%2==0) {
            glColor4f(1.0f, 1.0f, 0.4f, alpha);
        }
        float off_x = p->in_x * 0.6f;
        float off_y = p->in_y * 0.4f;
        
        glBegin(GL_POLYGON);
        for(int i=0; i<16; i++) {
            float theta = 2.0f * 3.14159f * i / 16.0f;
            glVertex3f(2.0f + off_x + size * cosf(theta), 2.0f + off_y + size * sinf(theta), 0.1f);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
    
    // Attack Hitbox Visualization
    if (p->state == STATE_ATTACK && p->attack_timer > 0) {
        float progress = 1.0f - ((float)p->attack_timer / (float)ATTACK_ACTIVE_FRAMES);
        float punch = sinf(progress * 3.14159f);
        float reach = 2.0f + punch * 2.2f;
        glColor3f(1, 0, 0);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        float hx = 1.4f + punch * 1.2f;
        float hy = 1.0f;
        float hw = reach;
        float hh = 1.8f + punch * 0.7f;
        glVertex3f(hx - hw/2, hy + hh/2, 0);
        glVertex3f(hx + hw/2, hy + hh/2, 0);
        glVertex3f(hx + hw/2, hy - hh/2, 0);
        glVertex3f(hx - hw/2, hy - hh/2, 0);
        glEnd();
    }

    // Smash Attack Visualization (Forward B)
    if (p->smash_charge_timer > 0 || p->smash_active_timer > 0) {
        float charge = p->smash_charge_level;
        float pulse = (p->smash_active_timer > 0) ? (1.0f - ((float)p->smash_active_timer / (float)SMASH_ACTIVE_FRAMES)) : charge;
        float radius = 0.8f + pulse * 1.6f;
        float line_len = 2.5f + pulse * 2.5f;
        glColor3f(0.2f, 0.9f, 1.0f);
        draw_circle(2.0f, 2.5f, radius, 0.2f, 0.9f, 1.0f, 16);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex3f(0.8f, 2.0f, 0.1f);
        glVertex3f(0.8f + line_len, 2.0f, 0.1f);
        glEnd();
    }

    if (p->smash_flash_timer > 0) {
        glColor3f(1.0f, 0.9f, 0.2f);
        glBegin(GL_POLYGON);
        glVertex3f(0.0f, 0.2f, 0.1f);
        glVertex3f(0.3f, 0.5f, 0.1f);
        glVertex3f(0.0f, 0.8f, 0.1f);
        glVertex3f(-0.3f, 0.5f, 0.1f);
        glEnd();
    }

    // Character recovery visuals
    if (p->umbrella_open && p->character_id == CHARACTER_PETALIA) {
        glColor3f(1.0f, 0.4f, 0.8f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex3f(0.0f, 4.0f, 0.1f);
        glVertex3f(0.0f, 6.5f, 0.1f);
        glEnd();
        draw_circle(0.0f, 7.0f, 1.8f, 1.0f, 0.4f, 0.8f, 16);
    }
    if (p->state == STATE_UPB && p->character_id == CHARACTER_VEXAR) {
        glColor3f(0.1f, 1.0f, 1.0f);
        glBegin(GL_LINES); glVertex3f(-0.5f, -0.5f, 0.1f); glVertex3f(-0.1f, -2.0f, 0.1f); glVertex3f(0.5f, -0.5f, 0.1f); glVertex3f(0.1f, -2.0f, 0.1f); glEnd();
    }

    // Mirror-match hat (342342) -- drawn last, above the head, so it's never occluded by the
    // sprite/body/accent draws above regardless of which branch drew this fighter. A bright,
    // fixed, player-color-not-character-color triangle (a real "party hat" silhouette, no new
    // art asset needed) at roughly the same height CHARACTER_PETALIA's own umbrella-accent
    // circle sits at, so it reads as "on the head" for the sprite-drawn fighters too.
    if (wears_mirror_hat) {
        glColor3f(1.0f, 0.15f, 0.15f);
        glBegin(GL_TRIANGLES);
        glVertex3f(-0.55f, 4.3f, 0.2f);
        glVertex3f(0.55f, 4.3f, 0.2f);
        glVertex3f(0.0f, 5.6f, 0.2f);
        glEnd();
        draw_circle(0.0f, 4.3f, 0.15f, 1.0f, 0.85f, 0.2f, 8); // pom-pom
    }

    glPopMatrix();
}

void draw_turnips() {
    for (int i = 0; i < MAX_TURNIPS; i++) {
        Turnip *t = &local_state.turnips[i];
        if (!t->active) continue;
        if (t->style == CHARACTER_VEXAR) glColor3f(0.8f, 0.95f, 1.0f); else glColor3f(0.9f, 0.8f, 0.6f);
        draw_circle(t->x, t->y, 0.6f, 0.9f, 0.8f, 0.6f, 10);
        glColor3f(0.2f, 0.7f, 0.2f);
        glBegin(GL_LINES);
        glVertex3f(t->x, t->y + 0.4f, 0.1f);
        glVertex3f(t->x, t->y + 0.8f, 0.1f);
        glEnd();
    }
}

void draw_edge_ko_effects() {
    for (int i = 0; i < MAX_EDGE_KO_EFFECTS; i++) {
        EdgeKOEffect *fx = &local_state.edge_kos[i];
        if (!fx->active) continue;

        float life = (float)fx->timer / (float)EDGE_KO_FLASH_FRAMES;
        if (life < 0.0f) life = 0.0f;
        float radius = (1.0f - life) * 6.0f * fx->intensity + 1.5f;
        float size = 0.6f + (1.0f - life) * 0.4f;

        for (int j = 0; j < 10; j++) {
            float angle = (2.0f * 3.14159f * (float)j / 10.0f) + (1.0f - life) * 2.0f;
            float px = fx->x + cosf(angle) * radius;
            float py = fx->y + sinf(angle) * radius;
            if (j % 2 == 0) {
                draw_rect(px, py, size, size, 1.0f, 0.2f, 0.1f, 1);
            } else {
                draw_rect(px, py, size, size, 1.0f, 0.9f, 0.1f, 1);
            }
        }
    }
}

// --- NETWORK STUBS ---
void net_init() {
    #ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
    #else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif
}

void net_connect() {
    struct hostent *he = gethostbyname(SERVER_HOST);
    if (he) {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
        char buffer[128];
        NetHeader *h = (NetHeader*)buffer;
        h->type = PACKET_CONNECT;
        sendto(sock, buffer, sizeof(NetHeader), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        printf("Connected to %s...\n", SERVER_HOST);
        g_net_client_id = -1; /* real reconnect: forget any previous id until a fresh PACKET_WELCOME arrives */
    }
}

/* net_send_find_match (S248-01) -- real client-side counterpart to the server's own new
 * PACKET_FIND_MATCH queue entry point. Not called from anywhere yet -- Phase 2 (a real, physical
 * portal trigger volume in the lobby scene, BP_LOBBY_MATCHMAKING_NORTHSTAR.md's own next phase)
 * is what will actually call this; it exists now so that real call site has a real function to
 * land on rather than needing its own wire-format code. */
void net_send_find_match(void) {
    if (sock < 0) return;
    char buffer[sizeof(NetHeader)];
    NetHeader *h = (NetHeader*)buffer;
    memset(h, 0, sizeof(NetHeader));
    h->type = PACKET_FIND_MATCH;
    sendto(sock, buffer, sizeof(NetHeader), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    printf("[NET] requested matchmaking...\n");
}

/* net_send_cmd (S248-00) -- real wire format matches apps/server/src/main.c's own
 * server_handle_packet exactly (the live server binary's parser is the real, fixed contract
 * here, not something this client redefines): [NetHeader][1 reserved/padding byte][UserCmd].
 * That extra byte before the payload is a real, existing server-side quirk (see
 * server_handle_packet's own `int cursor = sizeof(NetHeader) + 1;`), matched rather than
 * reinterpreted. Does nothing until g_net_client_id is known -- addressing a PACKET_USERCMD to
 * client_id -1 would just be silently ignored server-side (server_handle_packet resolves
 * client_id from the sender's own UDP address, not this field, so this guard is really "don't
 * bother sending before we're actually welcomed," not a correctness requirement of the wire
 * format itself). */
void net_send_cmd(UserCmd cmd) {
    if (sock < 0 || g_net_client_id < 0) return;
    char buffer[sizeof(NetHeader) + 1 + sizeof(UserCmd)];
    NetHeader h;
    memset(&h, 0, sizeof(h));
    h.type = PACKET_USERCMD;
    h.client_id = (unsigned char)g_net_client_id;
    h.sequence = g_net_send_seq++;
    h.timestamp = SDL_GetTicks();
    memcpy(buffer, &h, sizeof(NetHeader));
    buffer[sizeof(NetHeader)] = 0; /* reserved byte, see doc comment above */
    memcpy(buffer + sizeof(NetHeader) + 1, &cmd, sizeof(UserCmd));
    sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

/* net_tick (S248-00) -- drains every packet currently queued on the socket (non-blocking, set up
 * by net_init) and applies the real, authoritative server state. Real, honest reconciliation
 * choice for this first pass: every player's position/state, INCLUDING our own predicted slot,
 * is overwritten from each snapshot as it arrives -- not a full input-buffer replay
 * reconciliation (the "proper" approach a production fighting-game netcode would eventually
 * want). This is a real, working baseline that satisfies docs/net_plan.md's own "predict
 * locally, reconcile with server snapshots on receipt" -- our own local prediction still shows
 * immediately between snapshots (STATE_GAME_NET's own local_update call still runs every frame),
 * it's just corrected to the server's real values each time a snapshot lands rather than
 * blended/smoothed. Finer-grained client-side prediction smoothing is real, separate follow-up,
 * not attempted here. */
void net_tick(void) {
    if (sock < 0) return;
    char buffer[4096];
    struct sockaddr_in sender;
    socklen_t slen = sizeof(sender);
    int len;
    while ((len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &slen)) > 0) {
        if ((size_t)len < sizeof(NetHeader)) continue;
        NetHeader head;
        memcpy(&head, buffer, sizeof(NetHeader));

        if (head.type == PACKET_WELCOME) {
            g_net_client_id = head.client_id;
            printf("[NET] welcomed as client_id=%d\n", g_net_client_id);
            continue;
        }

        /* S248-01/S248-02: PACKET_MATCH_FOUND carries our assigned client_id the same way
           PACKET_WELCOME does. Also the real transition out of STATE_MATCHMAKING's own waiting
           screen into the actual match -- app_state is a plain global, safe to set here. */
        if (head.type == PACKET_MATCH_FOUND) {
            g_net_client_id = head.client_id;
            printf("[NET] match found, client_id=%d\n", g_net_client_id);
            app_state = STATE_GAME_NET;
            last_app_state = STATE_GAME_NET;
            continue;
        }

        /* S248-02: real, live matchmaking queue status -- see PACKET_QUEUE_STATUS's own doc
           comment in protocol.h. */
        if (head.type == PACKET_QUEUE_STATUS) {
            g_net_queue_count = head.entity_count;
            continue;
        }

        if (head.type == PACKET_SNAPSHOT) {
            int cursor = (int)sizeof(NetHeader);
            if (len < cursor + 1) continue;
            unsigned char count = (unsigned char)buffer[cursor];
            cursor += 1;
            for (int i = 0; i < count; i++) {
                if ((size_t)(len - cursor) < sizeof(NetPlayer)) break;
                NetPlayer np;
                memcpy(&np, buffer + cursor, sizeof(NetPlayer));
                cursor += sizeof(NetPlayer);
                if (np.id >= MAX_CLIENTS) continue;
                PlayerState *p = &local_state.players[np.id];
                p->active = 1;
                p->id = np.id;
                p->x = np.x; p->y = np.y;
                p->vx = np.vx; p->vy = np.vy;
                p->state = np.state;
                p->damage_percent = (float)np.damage;
                p->stocks = np.stocks;
                p->shield_health = (float)np.shield;
                p->facing = np.facing ? 1 : -1;
            }
        }
    }
}

// --- MAIN LOOP ---
int main(int argc, char* argv[]) {
    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "--host") == 0 && i+1<argc) strncpy(SERVER_HOST, argv[++i], 63);
    }

    g_pad.instance_id = -1;
    g_pad2.instance_id = -1;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_Window *win = SDL_CreateWindow("BRAWLPIT: 2.5D CHAOS", 100, 100, 1280, 720, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(win);
    try_open_first_two_controllers(&g_pad, &g_pad2);
    net_init();
    
    local_init_match(1, 0, STAGE_FD, selected_chars[0], selected_chars[1]);

    int running = 1;
    while(running) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) running = 0;
            if(e.type == SDL_KEYDOWN) {
                if(e.key.repeat) continue;
                if(e.key.keysym.sym == SDLK_F9) {
                    g_pad_debug = !g_pad_debug;
                    printf("[PAD] debug=%s\n", g_pad_debug ? "on" : "off");
                }
                if (app_state == STATE_CHARACTER_SELECT) {
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            static int prevLeft=0, prevRight=0, prevConfirm=0, prevTab=0;
            int left = k[SDL_SCANCODE_LEFT] || (g_pad.connected && (g_pad.dpad_left || g_pad.lx < -0.6f));
            int right = k[SDL_SCANCODE_RIGHT] || (g_pad.connected && (g_pad.dpad_right || g_pad.lx > 0.6f));
            int confirm = k[SDL_SCANCODE_J] || k[SDL_SCANCODE_RETURN] || (g_pad.connected && (g_pad.a || g_pad.x));
            int nextPlayer = k[SDL_SCANCODE_TAB] || (g_pad.connected && g_pad.y);
            if (left && !prevLeft) selected_chars[select_cursor] = (selected_chars[select_cursor] + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
            if (right && !prevRight) selected_chars[select_cursor] = (selected_chars[select_cursor] + 1) % CHARACTER_COUNT;
            if (confirm && !prevConfirm) { select_confirmed[select_cursor] = 1; if (select_cursor == 0) select_cursor = 1; }
            if (nextPlayer && !prevTab) select_cursor = 1 - select_cursor;
            prevLeft=left; prevRight=right; prevConfirm=confirm; prevTab=nextPlayer;
            if (select_confirmed[0] && select_confirmed[1]) {
                app_state = STATE_GAME_LOCAL;
                local_init_match(2, MODE_STOCK, last_stage_id, selected_chars[0], selected_chars[1]);
            }
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            float t = SDL_GetTicks() * 0.001f;
            glClearColor(0.04f, 0.04f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            for (int i=0;i<2;i++){
                const FighterDef *fd = fighter_def(selected_chars[i]);
                float x = -0.65f + i*1.3f;
                float glow = 0.5f + 0.5f*sinf(t*3.0f + i);
                draw_rect(x, 0.0f, 0.55f, 0.85f, fd->accent_r*0.2f, fd->accent_g*0.2f, fd->accent_b*0.2f, 1);
                draw_rect(x, 0.0f, 0.58f + glow*0.03f, 0.88f + glow*0.03f, fd->accent_r, fd->accent_g, fd->accent_b, 0);
                draw_string(fd->name, x-0.16f, -0.46f, 0.05f);
                draw_string(fd->descriptor, x-0.28f, -0.56f, 0.028f);
                if (fd->id == CHARACTER_PETALIA) { draw_circle(x,0.1f,0.15f,fd->body_r,fd->body_g,fd->body_b,20); draw_circle(x,0.25f,0.18f,1,0.6f,0.9f,20);}
                else { draw_rect(x,0.1f,0.2f,0.3f,fd->body_r,fd->body_g,fd->body_b,1); draw_rect(x+0.05f,0.2f,0.12f,0.05f,1,0.5f,0.1f,1); draw_rect(x+0.16f,0.08f,0.13f,0.07f,0.9f,0.9f,1,1);}
                if (i == select_cursor) draw_rect(x, -0.66f, 0.28f, 0.05f, 0.2f, 1.0f, 1.0f, 1);
            }
            draw_string("CHARACTER SELECT", -0.4f, 0.72f, 0.07f);
            SDL_GL_SwapWindow(win);
        } else if (app_state == STATE_LOBBY) {
                    if(e.key.keysym.sym == SDLK_d) {
                        last_mode = MODE_STOCK;
                        last_num_players = 2;
                        last_app_state = STATE_GAME_LOCAL;
                        app_state = STATE_CHARACTER_SELECT;
                        last_stage_id = STAGE_FD;
                        /* select_cursor reset added here (2026-08-04, founder: "in single player
                           mode the first game works but the next game it says character select
                           and it seems like i cant select the character to play again") -- every
                           real entry into STATE_CHARACTER_SELECT already reset select_confirmed
                           back to {0,0} but left select_cursor wherever match 1 ended it (slot 1,
                           since confirming slot 0 auto-advances the cursor there). On the second
                           trip through character select the player's left/right/confirm inputs
                           were silently editing selected_chars[1] (the bot's slot) instead of
                           their own -- select_confirmed[0] could never become true again, so
                           local_init_match's own start condition never fired. Real, live-verified
                           by tracing select_cursor's only mutation sites (line 564's own confirm
                           handler), not guessed. */
                        select_confirmed[0]=select_confirmed[1]=0; select_cursor=0;
                    } // 1v1 Bot
                    if(e.key.keysym.sym == SDLK_f) {
                        last_mode = MODE_STOCK;
                        last_num_players = 2;
                        last_app_state = STATE_GAME_LOCAL;
                        app_state = STATE_CHARACTER_SELECT;
                        last_stage_id = STAGE_TIMELINE;
                        select_confirmed[0]=select_confirmed[1]=0; select_cursor=0;
                    } // 1v1 Bot (Timeline Loop)
                    if(e.key.keysym.sym == SDLK_j) {
                        last_mode = MODE_STOCK;
                        last_num_players = 2;
                        last_app_state = STATE_GAME_NET;
                        app_state = STATE_GAME_NET;
                        net_connect();
                    }
                    if(e.key.keysym.sym == SDLK_m) {
                        /* S248-02: real matchmaking entry point -- see STATE_MATCHMAKING's own
                           doc comment for why this is a menu option, not a walked-into trigger
                           volume. Distinct from J's own direct-connect-to-whatever's-running
                           path: this actually queues via PACKET_FIND_MATCH and waits for a real
                           PACKET_MATCH_FOUND before playing. */
                        g_net_queue_count = -1;
                        app_state = STATE_MATCHMAKING;
                        net_connect();
                        net_send_find_match();
                        g_net_last_poll_ms = SDL_GetTicks();
                    }
                    if(e.key.keysym.sym == SDLK_t) {
                        /* TIPJAR Step 1 (2026-08-04) -- real single-player bar/bouncer shift, per
                           BRAWLPIT/docs/TIPJAR_ROADMAP.md's own Step 1 and the TIPJAR wiki's
                           Product-Core-Acceptance.md (both golden-indexed).
                           Step 3 (2026-08-14) -- real second local player. local_init_match(2, ...)
                           marks players[1].is_bot=1 by default (its own general "i>0 is a bot"
                           convention); clear it here so bot_think never overwrites the real second
                           player's input fed via local_set_player_input(1, ...) below. Only 2P is
                           wired this pass -- 3P/4P split is real, scoped follow-up work, not done
                           here (matches this repo's own "vertical slice, not everything at once"
                           discipline TIPJAR's own Steps 1/2 already established). */
                        local_init_match(2, MODE_STOCK, STAGE_FD, selected_chars[0], selected_chars[0]);
                        local_state.players[1].is_bot = 0;
                        tipjar_init(SDL_GetTicks());
                        app_state = STATE_TIPJAR;
                    } // TIPJAR shift
                }
                if (app_state == STATE_RESULTS && e.key.keysym.sym == SDLK_RETURN) {
                    winner_id = -1;
                    local_state.match_over = 0;
                    if (last_app_state == STATE_GAME_NET) {
                        app_state = STATE_GAME_NET;
                        net_connect();
                    } else {
                        app_state = STATE_CHARACTER_SELECT;
                        select_confirmed[0]=select_confirmed[1]=0; select_cursor=0;
                    }
                }
                if(e.key.keysym.sym == SDLK_ESCAPE) {
                    app_state = STATE_LOBBY;
                    winner_id = -1;
                }
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED) {
                if (!g_pad.connected) try_open_controller_index(&g_pad, e.cdevice.which);
                else if (!g_pad2.connected) try_open_controller_index(&g_pad2, e.cdevice.which);
            }
            if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (g_pad.connected && e.cdevice.which == g_pad.instance_id) {
                    printf("[PAD] Disconnected (instance=%d)\n", g_pad.instance_id);
                    close_controller(&g_pad);
                }
                if (g_pad2.connected && e.cdevice.which == g_pad2.instance_id) {
                    printf("[PAD2] Disconnected (instance=%d)\n", g_pad2.instance_id);
                    close_controller(&g_pad2);
                }
            }
        }

        poll_controller_state(&g_pad);
        poll_controller_state(&g_pad2);
        
        if (app_state == STATE_CHARACTER_SELECT) {
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            static int prevLeft=0, prevRight=0, prevConfirm=0, prevTab=0;
            int left = k[SDL_SCANCODE_LEFT] || (g_pad.connected && (g_pad.dpad_left || g_pad.lx < -0.6f));
            int right = k[SDL_SCANCODE_RIGHT] || (g_pad.connected && (g_pad.dpad_right || g_pad.lx > 0.6f));
            int confirm = k[SDL_SCANCODE_J] || k[SDL_SCANCODE_RETURN] || (g_pad.connected && (g_pad.a || g_pad.x));
            int nextPlayer = k[SDL_SCANCODE_TAB] || (g_pad.connected && g_pad.y);
            if (left && !prevLeft) selected_chars[select_cursor] = (selected_chars[select_cursor] + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
            if (right && !prevRight) selected_chars[select_cursor] = (selected_chars[select_cursor] + 1) % CHARACTER_COUNT;
            if (confirm && !prevConfirm) { select_confirmed[select_cursor] = 1; if (select_cursor == 0) select_cursor = 1; }
            if (nextPlayer && !prevTab) select_cursor = 1 - select_cursor;
            prevLeft=left; prevRight=right; prevConfirm=confirm; prevTab=nextPlayer;
            if (select_confirmed[0] && select_confirmed[1]) {
                app_state = STATE_GAME_LOCAL;
                local_init_match(2, MODE_STOCK, last_stage_id, selected_chars[0], selected_chars[1]);
            }
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            float t = SDL_GetTicks() * 0.001f;
            glClearColor(0.04f, 0.04f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            for (int i=0;i<2;i++){
                const FighterDef *fd = fighter_def(selected_chars[i]);
                float x = -0.65f + i*1.3f;
                float glow = 0.5f + 0.5f*sinf(t*3.0f + i);
                draw_rect(x, 0.0f, 0.55f, 0.85f, fd->accent_r*0.2f, fd->accent_g*0.2f, fd->accent_b*0.2f, 1);
                draw_rect(x, 0.0f, 0.58f + glow*0.03f, 0.88f + glow*0.03f, fd->accent_r, fd->accent_g, fd->accent_b, 0);
                draw_string(fd->name, x-0.16f, -0.46f, 0.05f);
                draw_string(fd->descriptor, x-0.28f, -0.56f, 0.028f);
                if (fd->id == CHARACTER_PETALIA) { draw_circle(x,0.1f,0.15f,fd->body_r,fd->body_g,fd->body_b,20); draw_circle(x,0.25f,0.18f,1,0.6f,0.9f,20);}
                else { draw_rect(x,0.1f,0.2f,0.3f,fd->body_r,fd->body_g,fd->body_b,1); draw_rect(x+0.05f,0.2f,0.12f,0.05f,1,0.5f,0.1f,1); draw_rect(x+0.16f,0.08f,0.13f,0.07f,0.9f,0.9f,1,1);}
                if (i == select_cursor) draw_rect(x, -0.66f, 0.28f, 0.05f, 0.2f, 1.0f, 1.0f, 1);
            }
            draw_string("CHARACTER SELECT", -0.4f, 0.72f, 0.07f);
            SDL_GL_SwapWindow(win);
        } else if (app_state == STATE_LOBBY) {
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glColor3f(1, 0, 1); // Neon Pink
            draw_string("BRAWLPIT", -0.5f, 0.2f, 0.1f);
            glColor3f(0, 1, 1);
            draw_string("D: VS BOT (STAGE 1)", -0.6f, 0.0f, 0.05f);
            draw_string("F: VS BOT (STAGE 2)", -0.6f, -0.1f, 0.05f);
            draw_string("J: JOIN NET", -0.6f, -0.2f, 0.05f);
            draw_string("T: TIPJAR SHIFT", -0.6f, -0.3f, 0.05f);
            draw_string("M: FIND MATCH (8 PLAYER)", -0.6f, -0.4f, 0.05f);
            SDL_GL_SwapWindow(win);
        } else if (app_state == STATE_MATCHMAKING) {
            /* S248-02: real, live waiting screen -- polls the server for status while showing
               it, then falls through to STATE_GAME_NET the instant net_tick sees a real
               PACKET_MATCH_FOUND (see net_tick's own doc comment). */
            unsigned int now = SDL_GetTicks();
            if (now - g_net_last_poll_ms >= MATCHMAKING_POLL_INTERVAL_MS) {
                net_send_find_match();
                g_net_last_poll_ms = now;
            }
            net_tick();

            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glColor3f(0, 1, 1);
            draw_string("FINDING MATCH...", -0.55f, 0.15f, 0.08f);
            char qbuf[64];
            if (g_net_queue_count >= 0) {
                snprintf(qbuf, sizeof(qbuf), "%d / %d PLAYERS QUEUED", g_net_queue_count, MATCHMAKING_MAX_QUEUE);
            } else {
                snprintf(qbuf, sizeof(qbuf), "CONNECTING...");
            }
            draw_string(qbuf, -0.5f, 0.0f, 0.05f);
            glColor3f(0.6f, 0.6f, 0.7f);
            draw_string("ESC: CANCEL", -0.5f, -0.3f, 0.04f);
            SDL_GL_SwapWindow(win);
        } else if (app_state == STATE_RESULTS) {
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();
            glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            float win_r = 1.0f, win_g = 0.3f, win_b = 0.8f;
            if (winner_id % 2 == 1) { win_r = 0.6f; win_g = 0.2f; win_b = 0.9f; }
            glColor3f(win_r, win_g, win_b);
            draw_string("WINNER", -0.6f, 0.2f, 0.08f);
            draw_rect(-0.4f, -0.1f, 0.3f, 0.5f, win_r, win_g, win_b, 1);
            glColor3f(0.5f, 0.5f, 0.6f);
            draw_string("LOSER", 0.2f, 0.2f, 0.06f);
            draw_rect(0.45f, -0.1f, 0.25f, 0.45f, 0.4f, 0.4f, 0.5f, 1);
            glColor3f(0.9f, 0.9f, 0.9f);
            draw_string("PRESS ENTER TO PLAY AGAIN", -0.9f, -0.6f, 0.05f);
            SDL_GL_SwapWindow(win);
        } else if (app_state == STATE_TIPJAR) {
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            float sx = 0, sy = 0;
            if(k[SDL_SCANCODE_A]) sx -= 1.0f;
            if(k[SDL_SCANCODE_D]) sx += 1.0f;
            if(k[SDL_SCANCODE_W]) sy += 1.0f;
            if(k[SDL_SCANCODE_S]) sy -= 1.0f;
            int jump = k[SDL_SCANCODE_SPACE];
            static int prev_deliver = 0, prev_bubble = 0;
            int deliver_raw = k[SDL_SCANCODE_J];
            int bubble_raw = k[SDL_SCANCODE_K];
            int shield_held = k[SDL_SCANCODE_LSHIFT];

            /* Step 3 (2026-08-14) -- real second local player, keyboard-only control scheme
               (arrows + RCtrl/RShift/Slash/Apostrophe). None of these scancodes are read anywhere
               else in STATE_TIPJAR, so there's no conflict with player 1's WASD/Space/J/K/LShift
               scheme. */
            float sx2 = 0, sy2 = 0;
            if(k[SDL_SCANCODE_LEFT]) sx2 -= 1.0f;
            if(k[SDL_SCANCODE_RIGHT]) sx2 += 1.0f;
            if(k[SDL_SCANCODE_UP]) sy2 += 1.0f;
            if(k[SDL_SCANCODE_DOWN]) sy2 -= 1.0f;
            int jump2 = k[SDL_SCANCODE_RCTRL];
            static int prev_deliver2 = 0, prev_bubble2 = 0;
            int deliver2_raw = k[SDL_SCANCODE_SLASH];
            int bubble2_raw = k[SDL_SCANCODE_APOSTROPHE];
            int shield2_held = k[SDL_SCANCODE_RSHIFT];

            /* Step 4 (2026-09-02, founder real-time: "if i plug controller in the keyboard
               controls that character both same char") -- the one connected gamepad used to
               merge into PLAYER 1's own input, stacked on top of player 1's own full WASD/Space/
               J/K/LShift keyboard scheme, while player 2 (arrows/RCtrl/Slash/Apostrophe-only)
               never saw the pad at all -- so keyboard and a plugged-in controller both drove the
               exact same fighter.

               Step 5 (2026-09-02, same founder thread, "1" = go build real dual-pad support) --
               exactly one connected pad still goes to PLAYER 2 (preserves Step 4's fix: keyboard
               P1 + pad P2 with a single controller); a SECOND connected pad (g_pad2) takes over
               PLAYER 1 instead of leaving it keyboard-only. Two controllers plugged in now drives
               two fully independent fighters, no keyboard required. */
            ControllerState *tipjar_pad1 = NULL, *tipjar_pad2 = NULL;
            if (g_pad2.connected) {
                tipjar_pad1 = g_pad.connected ? &g_pad : NULL;
                tipjar_pad2 = &g_pad2;
            } else if (g_pad.connected) {
                tipjar_pad2 = &g_pad;
            }
            int want_lobby = 0;
            apply_pad_to_tipjar_input(tipjar_pad1, &sx, &jump, &deliver_raw, &bubble_raw, &shield_held, &want_lobby);
            apply_pad_to_tipjar_input(tipjar_pad2, &sx2, &jump2, &deliver2_raw, &bubble2_raw, &shield2_held, &want_lobby);
            if (want_lobby) app_state = STATE_LOBBY;

            int deliver_pressed = deliver_raw && !prev_deliver;
            int bubble_pressed = bubble_raw && !prev_bubble;
            prev_deliver = deliver_raw; prev_bubble = bubble_raw;

            int deliver2_pressed = deliver2_raw && !prev_deliver2;
            int bubble2_pressed = bubble2_raw && !prev_bubble2;
            prev_deliver2 = deliver2_raw; prev_bubble2 = bubble2_raw;

            unsigned int tj_now = SDL_GetTicks();
            if (!tipjar_state.shift_over) {
                /* Real movement via the shared platformer physics (gravity, ground collision,
                   walk/jump) -- btn_special is deliberately never set here so update_entity's own
                   fighting-game Special handling (turnip-pull/Up-B/wavedash) can never fire; K is
                   read raw, above, purely for tipjar_tick's own "throw bubble" action. Attack/
                   shield are passed through too so melee/shield-dash still work as flavor, per the
                   wiki's own "dual-purpose actions" design -- Build 1 doesn't require them, but
                   nothing about TIPJAR needs to suppress them either. */
                local_set_player_input(1, sx2, sy2, jump2, deliver2_raw, shield2_held, 0);
                local_update(sx, sy, jump, deliver_raw, shield_held, 0, NULL, tj_now);
                /* Step 2 (2026-08-04): tipjar_tick is real player-indexed now. Step 3 (2026-08-14)
                   feeds slot 1's row for real -- previously only slot 0 ever had a real local
                   human behind it; every other slot fell through to bot_think or sat idle. */
                TipjarPlayerInput tj_inputs[MAX_CLIENTS];
                memset(tj_inputs, 0, sizeof(tj_inputs));
                tj_inputs[0].deliver_pressed = deliver_pressed;
                tj_inputs[0].bubble_pressed = bubble_pressed;
                tj_inputs[0].shield_held = shield_held;
                tj_inputs[1].deliver_pressed = deliver2_pressed;
                tj_inputs[1].bubble_pressed = bubble2_pressed;
                tj_inputs[1].shield_held = shield2_held;
                tipjar_tick(&local_state, tj_inputs, tj_now, 0.016f);
            } else if (k[SDL_SCANCODE_RETURN]) {
                app_state = STATE_LOBBY;
            }

            /* Step 3 (2026-08-14) -- real 2-panel split screen. Each panel gets its own
               glViewport (left/right halves of the fixed 1280x720 window) and its own camera,
               centered on that panel's owning player, so each player's own position stays
               readable in their own half instead of both sharing one shared-camera view. Both
               players (and the whole shared world) are drawn into EVERY panel -- this is a real
               split-screen, not two independent single-player views -- matching the roadmap's own
               "couch-style visibility: you can always watch opponents... in real time" goal (G0.2
               in TIPJAR-wiki/Product-Core-Acceptance.md). 3P/4P layouts are real, scoped follow-up
               work, not built this pass. */
            glClearColor(0.06f, 0.05f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            char buf[96];
            for (int panel = 0; panel < 2; panel++) {
                int vp_x = panel == 0 ? 0 : 640;
                glViewport(vp_x, 0, 640, 720);

                PlayerState *owner = &local_state.players[panel];
                float cx = owner->active ? owner->x : 0.0f;
                float cy = 6.0f, zoom = 24.0f;
                float tj_ar = 640.0f/720.0f;
                glMatrixMode(GL_PROJECTION); glLoadIdentity();
                glOrtho(cx - zoom*tj_ar, cx + zoom*tj_ar, cy - zoom, cy + zoom, -100, 100);
                glMatrixMode(GL_MODELVIEW); glLoadIdentity();

                draw_stage();
                draw_rect(TIPJAR_EJECT_DOOR_X, 4.0f, 0.4f, 8.0f, 0.8f, 0.7f, 0.1f, 1); /* eject door marker */
                draw_string("EJECT", TIPJAR_EJECT_DOOR_X - 1.0f, 8.6f, 0.5f);

                for (int ci = 0; ci < MAX_CUSTOMERS; ci++) {
                    Customer *c = &tipjar_state.customers[ci];
                    if (!c->active) continue;
                    float r=0.8f, g=0.8f, b=0.8f;
                    switch (c->state) {
                        case CUST_WAITING_DRINK: {
                            int angry = c->patience < (TIPJAR_PATIENCE_SECONDS * TIPJAR_ANGRY_THRESHOLD);
                            if (angry) { r=0.95f; g=0.55f; b=0.15f; } else { r=0.75f; g=0.78f; b=0.9f; }
                            break;
                        }
                        case CUST_HAPPY: r=0.25f; g=0.9f; b=0.35f; break;
                        case CUST_BRAWLING: r=0.9f; g=0.15f; b=0.15f; break;
                        case CUST_BUBBLED: r=0.25f; g=0.75f; b=0.95f; break;
                        default: break;
                    }
                    draw_rect(c->x, c->y + 1.5f, 1.5f, 3.0f, r, g, b, 1);
                    if (c->state == CUST_WAITING_DRINK) {
                        draw_string(TIPJAR_DRINK_NAMES[c->order_type], c->x - 1.3f, c->y + 3.4f, 0.4f);
                        float pfrac = c->patience / TIPJAR_PATIENCE_SECONDS;
                        if (pfrac < 0.0f) pfrac = 0.0f;
                        draw_rect(c->x, c->y + 3.0f, 1.6f, 0.25f, 0.15f, 0.15f, 0.18f, 1);
                        draw_rect(c->x - 0.8f + 0.8f*pfrac, c->y + 3.0f, 1.6f*pfrac, 0.25f, 0.9f, 0.85f, 0.2f, 1);
                    }
                }

                draw_turnips();
                if (local_state.players[0].active) draw_player(&local_state.players[0], 0);
                if (local_state.players[1].active) draw_player(&local_state.players[1], 1);

                snprintf(buf, sizeof(buf), "TIPS: $%d / $%d", tipjar_total_score(), TIPJAR_QUOTA);
                glColor3f(1,1,1); draw_string(buf, cx - zoom*tj_ar + 1.0f, cy + zoom - 2.0f, 0.6f);
                int secs_left = tipjar_state.shift_over ? 0 : (int)((tipjar_state.shift_end_ms - tj_now) / 1000);
                if (secs_left < 0) secs_left = 0;
                snprintf(buf, sizeof(buf), "SHIFT: %d:%02d", secs_left / 60, secs_left % 60);
                draw_string(buf, cx - zoom*tj_ar + 1.0f, cy + zoom - 3.5f, 0.5f);
                float vfrac = tipjar_state.vibe / 100.0f;
                draw_rect(cx - zoom*tj_ar + 6.0f, cy + zoom - 5.2f, 8.0f, 0.6f, 0.15f, 0.1f, 0.1f, 1);
                draw_rect(cx - zoom*tj_ar + 2.0f + 4.0f*vfrac, cy + zoom - 5.2f, 8.0f*vfrac, 0.6f,
                          vfrac > 0.4f ? 0.3f : 0.9f, vfrac > 0.4f ? 0.85f : 0.2f, 0.3f, 1);
                draw_string("VIBE", cx - zoom*tj_ar + 1.0f, cy + zoom - 6.2f, 0.4f);
                snprintf(buf, sizeof(buf), "P%d", panel + 1);
                draw_string(buf, cx - zoom*tj_ar + 1.0f, cy - zoom + 1.0f, 0.5f);

                if (tipjar_state.shift_over) {
                    glColor3f(1,1,1);
                    draw_string(tipjar_state.shift_won ? "SHIFT COMPLETE" : "SHIFT OVER", cx - 8.0f, cy + 2.0f, 1.0f);
                    snprintf(buf, sizeof(buf), "TIPS: $%d  ORDERS: %d/%d  BRAWLS: %d  DMG: %d",
                             tipjar_total_score(), tipjar_state.orders_completed,
                             tipjar_state.orders_completed + tipjar_state.orders_missed,
                             tipjar_state.brawls_handled, tipjar_state.damage_caused);
                    draw_string(buf, cx - 13.0f, cy - 1.0f, 0.35f);
                    draw_string("PRESS ENTER", cx - 6.0f, cy - 3.0f, 0.4f);
                }
            }
            glViewport(0, 0, 1280, 720); /* restore full-window viewport for every other app_state */

            SDL_GL_SwapWindow(win);
        } else {
            // --- INPUT ---
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            float sx = 0, sy = 0;
            if(k[SDL_SCANCODE_A]) sx -= 1.0f;
            if(k[SDL_SCANCODE_D]) sx += 1.0f;
            if(k[SDL_SCANCODE_W]) sy += 1.0f;
            if(k[SDL_SCANCODE_S]) sy -= 1.0f;
            
            int jump = k[SDL_SCANCODE_SPACE];
            int attack = k[SDL_SCANCODE_J]; // 'J' to jab
            int shield = k[SDL_SCANCODE_LSHIFT];
            int special = k[SDL_SCANCODE_K]; // 'K' to dodge/wavedash

            if (g_pad.connected) {
                float pad_x = g_pad.lx;
                if (fabsf(pad_x) <= PAD_STICK_DEADZONE) pad_x = 0.0f;
                if (g_pad.dpad_left) pad_x = -1.0f;
                if (g_pad.dpad_right) pad_x = 1.0f;

                float engage = PAD_MOVE_THRESHOLD;
                float release = PAD_MOVE_THRESHOLD - PAD_MOVE_HYSTERESIS;
                float mag = fabsf(pad_x);
                if (!g_pad.move_active_x && mag >= engage) g_pad.move_active_x = 1;
                else if (g_pad.move_active_x && mag <= release) g_pad.move_active_x = 0;
                if (!g_pad.move_active_x) pad_x = 0.0f;
                if (fabsf(pad_x) >= PAD_MOVE_SNAP_THRESHOLD) pad_x = (pad_x > 0.0f) ? 1.0f : -1.0f;

                // merge rule: horizontal stick picks source with stronger magnitude.
                if (fabsf(pad_x) >= fabsf(sx)) sx = pad_x;

                /* Real, genuine bug fixed (kanban BP-TUNE-CP-001: "BP CONTROLLER PARITY -
                 * keyboard controll can drop down through the platforms controller cant (fall
                 * through)"). g_pad.ly was read from the real controller axis every frame
                 * (poll_controller_state) but never once merged into sy anywhere in this file --
                 * dpad_up/dpad_down were in the same boat. A controller player had no real way
                 * to set in_y at all, meaning every W/S-gated mechanic (drop-through platforms,
                 * AND every neutral-special's own "hold up + special" dispatch -- Medusa/
                 * Raccoon/Second Tree/Uncrowned/Rosie's Insert Coin all gate on p->in_y > 0.5f)
                 * was silently unreachable on a pad, not just the one drop-through case the card
                 * itself named. Real, deliberate design: no hysteresis engage/release state
                 * machine here unlike the horizontal merge above -- pad_x smooths continuous
                 * LOCOMOTION, but in_y is only ever read as a threshold gesture (> 0.5f / < -0.6f
                 * for "hold up"/"hold down"), so a plain deadzone + snap is the right shape,
                 * matching how the keyboard's own sy is a flat +/-1.0f with no smoothing either.
                 * SDL's own real axis convention: SDL_CONTROLLER_AXIS_LEFTY is positive when the
                 * stick is pushed DOWN -- negated here so pad_y matches sy's own "hold up is
                 * positive" convention (k[SDL_SCANCODE_W]) sy += 1.0f above). */
                float pad_y = -g_pad.ly;
                if (fabsf(pad_y) <= PAD_STICK_DEADZONE) pad_y = 0.0f;
                if (g_pad.dpad_up) pad_y = 1.0f;
                if (g_pad.dpad_down) pad_y = -1.0f;
                if (fabsf(pad_y) >= PAD_MOVE_SNAP_THRESHOLD) pad_y = (pad_y > 0.0f) ? 1.0f : -1.0f;
                if (fabsf(pad_y) >= fabsf(sy)) sy = pad_y;

                jump = jump || g_pad.a;
                attack = attack || g_pad.x || (g_pad.rt > PAD_TRIGGER_THRESHOLD);
                shield = shield || g_pad.lb || (g_pad.lt > PAD_TRIGGER_THRESHOLD);
                special = special || g_pad.b || g_pad.rb;

                if (g_pad.start) app_state = STATE_LOBBY;

                if (g_pad_debug) {
                    unsigned int now = SDL_GetTicks();
                    if (now - g_last_pad_debug_log_ms > 250) {
                        g_last_pad_debug_log_ms = now;
                        printf("[PAD] lx_raw=%.2f lx=%.2f lt=%.2f rt=%.2f A:%d B:%d X:%d Y:%d D:%d%d%d%d\n",
                            g_pad.lx, sx, g_pad.lt, g_pad.rt,
                            g_pad.a, g_pad.b, g_pad.x, g_pad.y,
                            g_pad.dpad_up, g_pad.dpad_down, g_pad.dpad_left, g_pad.dpad_right);
                    }
                }
            }
            
            // --- UPDATE ---
            if (local_state.match_over) {
                if (winner_id == -1) {
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        PlayerState *p = &local_state.players[i];
                        if (p->active && p->stocks > 0) {
                            winner_id = i;
                            break;
                        }
                    }
                }
                app_state = STATE_RESULTS;
            }

            if (app_state == STATE_GAME_NET) {
                UserCmd cmd = {0};
                cmd.stick_x = sx; cmd.stick_y = sy;
                if(jump) cmd.buttons |= BTN_JUMP;
                if(attack) cmd.buttons |= BTN_ATTACK;
                if(shield) cmd.buttons |= BTN_SHIELD;
                if(special) cmd.buttons |= BTN_SPECIAL;
                net_send_cmd(cmd);
                net_tick(); // Receive snapshots; see net_tick's own doc comment for the real reconciliation approach
                // TODO(net): apply server-authoritative stage_id from welcome/snapshot before simulation.
                if (g_net_client_id >= 0) {
                    /* Real per-client input routing (S248-00): the local prediction slot must be
                       whichever slot the SERVER actually assigned us via PACKET_WELCOME, not
                       always slot 0 -- the server never assigns real network clients slot 0 (its
                       own PACKET_CONNECT handler starts scanning at i=1), so local_update's own
                       hardcoded "always slot 0" input path would silently drive the wrong
                       PlayerState for every real networked match. local_set_player_input already
                       exists for exactly this (TIPJAR's own second-local-player path) --
                       reused here, then local_update is called with all-zero direct args so it
                       doesn't also stomp slot 0 with zeroed input. */
                    local_set_player_input(g_net_client_id, sx, sy, jump, attack, shield, special);
                    local_update(0, 0, 0, 0, 0, 0, NULL, SDL_GetTicks());
                } else {
                    /* Not welcomed yet -- keep predicting locally on slot 0 exactly like before,
                       so the screen isn't just frozen while waiting on the server's reply. */
                    local_update(sx, sy, jump, attack, shield, special, NULL, SDL_GetTicks());
                }
            } else {
                local_update(sx, sy, jump, attack, shield, special, NULL, SDL_GetTicks());
            }

            // --- CAMERA ---
            // Track all active players
            float min_x=999, max_x=-999, min_y=999, max_y=-999;
            int count = 0;
            for(int i=0; i<MAX_CLIENTS; i++) {
                PlayerState *p = &local_state.players[i];
                if (!p->active) continue;
                if (p->state == STATE_DEAD || p->respawn_timer > 0) continue;
                if (p->x < min_x) min_x = p->x;
                if (p->x > max_x) max_x = p->x;
                if (p->y < min_y) min_y = p->y;
                if (p->y > max_y) max_y = p->y;
                count++;
            }
            if (count == 0) {
                min_x = -10.0f;
                max_x = 10.0f;
                min_y = -5.0f;
                max_y = 10.0f;
            }
            float cx = (min_x + max_x) / 2.0f;
            float cy = (min_y + max_y) / 2.0f;
            float zoom = (max_x - min_x) * 0.8f;
            if (zoom < 30.0f) zoom = 30.0f;
            if (zoom > 100.0f) zoom = 100.0f;

            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            float ar = 1280.0f/720.0f;
            glOrtho(cx - zoom*ar, cx + zoom*ar, cy - zoom, cy + zoom, -100, 100);
            glMatrixMode(GL_MODELVIEW); glLoadIdentity();

            // --- RENDER ---
            glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            draw_stage();
            draw_turnips();
            draw_edge_ko_effects();
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_state.players[i].active) draw_player(&local_state.players[i], i);
            }
            
            draw_hud(&local_state.players[0]);
            
            SDL_GL_SwapWindow(win);
        }
        SDL_Delay(16);
    }
    close_controller(&g_pad);
    close_controller(&g_pad2);
    SDL_Quit();
    return 0;
}

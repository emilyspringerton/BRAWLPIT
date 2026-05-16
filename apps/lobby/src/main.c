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
int SERVER_PORT = 6969;
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
int g_pad_debug = 0;
unsigned int g_last_pad_debug_log_ms = 0;

int sock = -1;
struct sockaddr_in server_addr;

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

void draw_player(PlayerState *p) {
    if(p->state == STATE_DEAD) return;
    
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

    if (p->character_id == CHARACTER_PETALIA) {
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
    }
}

// --- MAIN LOOP ---
int main(int argc, char* argv[]) {
    for(int i=1; i<argc; i++) {
        if(strcmp(argv[i], "--host") == 0 && i+1<argc) strncpy(SERVER_HOST, argv[++i], 63);
    }

    g_pad.instance_id = -1;
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_Window *win = SDL_CreateWindow("BRAWLPIT: 2.5D CHAOS", 100, 100, 1280, 720, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(win);
    try_open_first_controller(&g_pad);
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
                        select_confirmed[0]=select_confirmed[1]=0;
                    } // 1v1 Bot
                    if(e.key.keysym.sym == SDLK_f) {
                        last_mode = MODE_STOCK;
                        last_num_players = 2;
                        last_app_state = STATE_GAME_LOCAL;
                        app_state = STATE_CHARACTER_SELECT;
                        last_stage_id = STAGE_TIMELINE;
                        select_confirmed[0]=select_confirmed[1]=0;
                    } // 1v1 Bot (Timeline Loop)
                    if(e.key.keysym.sym == SDLK_j) {
                        last_mode = MODE_STOCK;
                        last_num_players = 2;
                        last_app_state = STATE_GAME_NET;
                        app_state = STATE_GAME_NET;
                        net_connect();
                    }
                }
                if (app_state == STATE_RESULTS && e.key.keysym.sym == SDLK_RETURN) {
                    winner_id = -1;
                    local_state.match_over = 0;
                    if (last_app_state == STATE_GAME_NET) {
                        app_state = STATE_GAME_NET;
                        net_connect();
                    } else {
                        app_state = STATE_CHARACTER_SELECT;
                        select_confirmed[0]=select_confirmed[1]=0;
                    }
                }
                if(e.key.keysym.sym == SDLK_ESCAPE) {
                    app_state = STATE_LOBBY;
                    winner_id = -1;
                }
            }
            if (e.type == SDL_CONTROLLERDEVICEADDED) {
                if (!g_pad.connected) try_open_controller_index(&g_pad, e.cdevice.which);
            }
            if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (g_pad.connected && e.cdevice.which == g_pad.instance_id) {
                    printf("[PAD] Disconnected (instance=%d)\n", g_pad.instance_id);
                    close_controller(&g_pad);
                }
            }
        }

        poll_controller_state(&g_pad);
        
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
                // Send Cmd logic (Simplified for brevity)
                UserCmd cmd = {0};
                cmd.stick_x = sx; cmd.stick_y = sy;
                if(jump) cmd.buttons |= BTN_JUMP;
                if(attack) cmd.buttons |= BTN_ATTACK;
                if(shield) cmd.buttons |= BTN_SHIELD;
                if(special) cmd.buttons |= BTN_SPECIAL;
                // net_send_cmd(cmd); 
                // net_tick(); // Receive snapshots
                // TODO(net): apply server-authoritative stage_id from welcome/snapshot before simulation.
                local_update(sx, sy, jump, attack, shield, special, NULL, SDL_GetTicks()); // Local prediction
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
                if(local_state.players[i].active) draw_player(&local_state.players[i]);
            }
            
            draw_hud(&local_state.players[0]);
            
            SDL_GL_SwapWindow(win);
        }
        SDL_Delay(16);
    }
    close_controller(&g_pad);
    SDL_Quit();
    return 0;
}

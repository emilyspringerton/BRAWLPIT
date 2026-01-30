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
#include "../../../packages/simulation/local_game.h"

#define STATE_LOBBY 0
#define STATE_GAME_NET 1
#define STATE_GAME_LOCAL 2

char SERVER_HOST[64] = "127.0.0.1";
int SERVER_PORT = 6969;
int app_state = STATE_LOBBY;
int my_client_id = -1;

int sock = -1;
struct sockaddr_in server_addr;

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

// Minimal text renderer (same as before but stripped down)
void draw_char(char c, float x, float y, float s) {
    glLineWidth(2.0f); glBegin(GL_LINES);
    // Simplified ASCII art for numbers and basic letters
    if(c=='0'){glVertex2f(x,y);glVertex2f(x+s,y);glVertex2f(x+s,y);glVertex2f(x+s,y+s);glVertex2f(x+s,y+s);glVertex2f(x,y+s);glVertex2f(x,y+s);glVertex2f(x,y);}
    else if(c=='1'){glVertex2f(x+s/2,y);glVertex2f(x+s/2,y+s);}
    else if(c=='%'){glVertex2f(x,y);glVertex2f(x+s,y+s);glVertex2f(x+s,y);glVertex2f(x,y+s);}
    else if(c=='P'){glVertex2f(x,y);glVertex2f(x,y+s);glVertex2f(x,y+s);glVertex2f(x+s,y+s);glVertex2f(x+s,y+s);glVertex2f(x+s,y+s/2);glVertex2f(x+s,y+s/2);glVertex2f(x,y+s/2);}
    // Default box
    else {glVertex2f(x,y);glVertex2f(x+s,y);glVertex2f(x,y);glVertex2f(x,y+s);}
    glEnd();
}
void draw_string(const char* str, float x, float y, float s) {
    while(*str) { draw_char(*str, x, y, s); x += s * 1.5f; str++; }
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
    
    // Color based on state
    float r=1, g=1, b=1;
    if (p->state == STATE_STUNNED) { r=1; g=1; b=0; } // Yellow Stun
    if (p->invuln_frames > 0 && (SDL_GetTicks()/50)%2==0) { r=0.5f; g=0.5f; b=0.5f; } // Flicker

    // Body (Rectangle)
    draw_rect(0, 2, 2.0f, 4.0f, r, g, b, 1);
    
    // Eye (Direction indicator)
    draw_rect(0.5f, 3.0f, 0.5f, 0.5f, 0, 0, 0, 1);

    // Shield Bubble
    if (p->state == STATE_SHIELD) {
        float alpha = p->shield_health / 100.0f;
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(0.0f, 0.5f, 1.0f, 0.5f);
        float size = 4.0f * (p->shield_health / 100.0f);
        if (size < 1.5f) size = 1.5f;
        
        glBegin(GL_POLYGON);
        for(int i=0; i<16; i++) {
            float theta = 2.0f * 3.14159f * i / 16.0f;
            glVertex3f(2.0f + size * cosf(theta), 2.0f + size * sinf(theta), 0.1f);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
    
    // Attack Hitbox Visualization
    if (p->state == STATE_ATTACK) {
        float reach = 3.5f;
        glColor3f(1, 0, 0);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        float hx = 2.0f, hy = 1.0f, hw = reach, hh = 2.5f;
        glVertex3f(hx - hw/2, hy + hh/2, 0);
        glVertex3f(hx + hw/2, hy + hh/2, 0);
        glVertex3f(hx + hw/2, hy - hh/2, 0);
        glVertex3f(hx - hw/2, hy - hh/2, 0);
        glEnd();
    }

    glPopMatrix();
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

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("BRAWLPIT: 2.5D CHAOS", 100, 100, 1280, 720, SDL_WINDOW_OPENGL);
    SDL_GL_CreateContext(win);
    net_init();
    
    local_init_match(1, 0);
    
    int running = 1;
    while(running) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) running = 0;
            if(e.type == SDL_KEYDOWN) {
                if(e.key.keysym.sym == SDLK_d) { app_state = STATE_GAME_LOCAL; local_init_match(2, MODE_STOCK); } // 1v1 Bot
                if(e.key.keysym.sym == SDLK_j) { app_state = STATE_GAME_NET; net_connect(); }
                if(e.key.keysym.sym == SDLK_ESCAPE) app_state = STATE_LOBBY;
            }
        }
        
        if (app_state == STATE_LOBBY) {
             glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
             glClear(GL_COLOR_BUFFER_BIT);
             glLoadIdentity();
             glColor3f(1, 0, 1); // Neon Pink
             draw_string("BRAWLPIT", -0.5f, 0.2f, 0.1f);
             glColor3f(0, 1, 1);
             draw_string("D: VS BOT", -0.4f, 0.0f, 0.05f);
             draw_string("J: JOIN NET", -0.4f, -0.1f, 0.05f);
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
            
            // --- UPDATE ---
            if (app_state == STATE_GAME_NET) {
                // Send Cmd logic (Simplified for brevity)
                UserCmd cmd = {0};
                cmd.stick_x = sx; cmd.stick_y = sy;
                if(jump) cmd.buttons |= BTN_JUMP;
                if(attack) cmd.buttons |= BTN_ATTACK;
                if(shield) cmd.buttons |= BTN_SHIELD;
                // net_send_cmd(cmd); 
                // net_tick(); // Receive snapshots
                local_update(sx, sy, jump, attack, shield, 0, NULL, SDL_GetTicks()); // Local prediction
            } else {
                local_update(sx, sy, jump, attack, shield, 0, NULL, SDL_GetTicks());
            }

            // --- CAMERA ---
            // Track all active players
            float min_x=999, max_x=-999, min_y=999, max_y=-999;
            int count = 0;
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_state.players[i].active) {
                    if(local_state.players[i].x < min_x) min_x = local_state.players[i].x;
                    if(local_state.players[i].x > max_x) max_x = local_state.players[i].x;
                    if(local_state.players[i].y < min_y) min_y = local_state.players[i].y;
                    if(local_state.players[i].y > max_y) max_y = local_state.players[i].y;
                    count++;
                }
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
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_state.players[i].active) draw_player(&local_state.players[i]);
            }
            
            draw_hud(&local_state.players[0]);
            
            SDL_GL_SwapWindow(win);
        }
        SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}

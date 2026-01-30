#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

#define MAX_CLIENTS 8
#define MAX_PROJECTILES 64
#define LAG_HISTORY 64

#define PACKET_CONNECT 0
#define PACKET_USERCMD 1
#define PACKET_SNAPSHOT 2
#define PACKET_WELCOME  3

/* --- BRAWLPIT: Phase 1 Platformer States --- */
#define STATE_IDLE      0
#define STATE_RUN       1
#define STATE_AIR       2
#define STATE_ATTACK    3
#define STATE_STUNNED   4
#define STATE_SHIELD    5
#define STATE_DEAD      6
#define STATE_RESPAWN   7
#define STATE_GROUNDED  10
#define STATE_AIRBORNE  11
#define STATE_JUMPING   12
#define STATE_WAVEDASH  13

#define STOCK_COUNT 4
#define SHIELD_MAX 100
#define MAX_JUMPS 2

/* --- BRAWLPIT: Phase 1 Structures --- */
typedef struct {
    float x, y;        // Camera center
    float zoom;        // Current zoom
    float target_zoom; // Target zoom
} Camera2D;

typedef struct {
    float x, y, w, h;
    int type;          // 0 = SOLID, 1 = PASSTHROUGH
} Platform2D;

typedef struct {
    unsigned char type;
    unsigned char client_id;
    unsigned short sequence;
    unsigned int timestamp;
    unsigned char entity_count; 
} NetHeader;

typedef struct {
    unsigned int sequence;
    unsigned int timestamp;
    unsigned short msec;
    float stick_x;  // -1 to 1 (Move L/R)
    float stick_y;  // -1 to 1 (Up/Down)
    unsigned int buttons;
    int weapon_idx; 
} UserCmd;

#define BTN_JUMP   1
#define BTN_ATTACK 2
#define BTN_SHIELD 4 
#define BTN_SPECIAL 8

typedef struct {
    unsigned char id; 
    float x, y;        
    float vx, vy;      
    unsigned char state;
    unsigned short damage;   
    unsigned char stocks;
    unsigned char shield;
    unsigned char facing;    
    unsigned char jump_count;
    unsigned char hit_stun;
} NetPlayer;

/* --- BOT BRAIN --- */
typedef struct {
    int version;
    float aggro;     
    float shield_freq;
    float jump_freq;
} BotGenome;

typedef struct {
    int id;
    int active;
    int is_bot;
    
    // Physics
    float x, y;
    float vx, vy;
    int on_ground;
    int facing; 
    
    // Inputs
    float in_x;
    float in_y;
    int btn_jump;
    int btn_attack;
    int btn_shield;
    
    // Combat State
    int state;
    float damage_percent;
    int stocks;
    float shield_health;
    int shield_regen_timer;
    int jumps_remaining;
    
    // Timers
    int hitstun_frames;
    int attack_cooldown;
    int invuln_frames;
    int respawn_timer;
    
    // Stats
    int kills;
    int deaths;
    
    BotGenome brain;
    unsigned int last_hit_time;
} PlayerState;

typedef struct {
    int active; unsigned int timestamp;
    float x, y;
    float vx, vy;
} LagRecord;

typedef enum { MODE_STOCK=0, MODE_TIME=1 } GameMode;

typedef struct {
    PlayerState players[MAX_CLIENTS];
    LagRecord history[MAX_CLIENTS][LAG_HISTORY];
    int server_tick;
    int game_mode;
    struct sockaddr_in clients[MAX_CLIENTS];
    int client_active[MAX_CLIENTS];
} ServerState;

#endif

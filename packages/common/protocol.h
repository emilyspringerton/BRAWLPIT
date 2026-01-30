#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_CLIENTS 8
#define MAX_PROJECTILES 64
#define LAG_HISTORY 64

#define PACKET_CONNECT 0
#define PACKET_USERCMD 1
#define PACKET_SNAPSHOT 2
#define PACKET_WELCOME  3

// --- FIGHTER STATES ---
#define STATE_IDLE      0
#define STATE_RUN       1
#define STATE_AIR       2
#define STATE_ATTACK    3
#define STATE_STUNNED   4
#define STATE_SHIELD    5
#define STATE_DEAD      6
#define STATE_RESPAWN   7

// --- TUNING CONSTANTS ---
#define STOCK_COUNT 4
#define SHIELD_MAX 100
#define MAX_JUMPS 2

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
    float stick_y;  // -1 to 1 (Up/Down / Aim)
    unsigned int buttons;
    int weapon_idx; // Unused but kept for alignment
} UserCmd;

#define BTN_JUMP   1
#define BTN_ATTACK 2
#define BTN_SHIELD 4   // Block/Air Dodge
#define BTN_SPECIAL 8  // Special Move

typedef struct {
    unsigned char id; 
    float x, y;        // 2D Pos
    float vx, vy;      // Velocity for prediction
    unsigned char state;
    unsigned short damage;   // 0 - 999%
    unsigned char stocks;
    unsigned char shield;
    unsigned char facing;    // 0: Left, 1: Right
    unsigned char jump_count;
    unsigned char hit_stun;
} NetPlayer;

// --- BOT BRAIN ---
typedef struct {
    int version;
    float aggro;     // How often to approach
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
    int facing; // -1 Left, 1 Right
    
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

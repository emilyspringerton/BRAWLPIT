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
#define MAX_EDGE_KO_EFFECTS 16

#define PACKET_CONNECT 0
#define PACKET_USERCMD 1
#define PACKET_SNAPSHOT 2
#define PACKET_WELCOME  3
/* S248-01 (server-side matchmaking queue, BP-LOBBY-001 Phase 1) -- a client sends
 * PACKET_FIND_MATCH to join the real matchmaking queue instead of PACKET_CONNECT's own
 * immediate-join (unchanged, still real and useful for direct testing/dev play, see
 * apps/server/src/main.c's own doc comment on why both paths coexist). The server holds queued
 * clients until MATCHMAKING_MAX_QUEUE have joined or MATCHMAKING_TIMEOUT_MS elapses, then
 * starts a fresh match (bot-filling any remaining slots) and replies PACKET_MATCH_FOUND to
 * every queued client -- same NetHeader.client_id convention PACKET_WELCOME already uses, so
 * client-side handling is the same code path (see apps/lobby/src/main.c's own net_tick). */
#define PACKET_FIND_MATCH  4
#define PACKET_MATCH_FOUND 5
/* S248-02 (client-side matchmaking status, BP-LOBBY-001 Phase 2) -- the server replies with a
 * real PACKET_QUEUE_STATUS every time it processes a PACKET_FIND_MATCH from a not-yet-matched
 * sender (a fresh enqueue or a client's own periodic re-poll), carrying the current queue depth
 * in NetHeader.entity_count so the waiting client can show real, live "X/N queued" feedback
 * instead of a silent wait. */
#define PACKET_QUEUE_STATUS 6

/* MATCHMAKING_MAX_QUEUE is MAX_CLIENTS - 1, not MAX_CLIENTS -- a real, pre-existing structural
 * constraint found while implementing this, not invented here: slot 0 has never been a real
 * network slot (apps/server/src/main.c's own PACKET_CONNECT handler and server_broadcast both
 * already loop `for(i=1; i<MAX_CLIENTS; i++)`, skipping it entirely -- it's reserved for the
 * boot-time local demo match). So a matchmade lobby seats up to 7 real queued humans (slots
 * 1-7) plus slot 0 always bot-filled, for BP-LOBBY-001's own literal "8 random players" as 8
 * total combatants, not 8 real network connections. */
#define MATCHMAKING_MAX_QUEUE (MAX_CLIENTS - 1)
/* Real, tunable default -- 20s gives a solo tester (or a thin trickle of real players) a
 * reasonable wait before bots fill the rest, without leaving a single queued player waiting
 * indefinitely for 7 others who may never come. */
#define MATCHMAKING_TIMEOUT_MS 20000

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
#define STATE_UPB       14
#define STATE_ROSIE_DASH 15 /* real, new state (kanban BP-TUNE-0033): Rosie's own side-B, a real
                             * timed dash with a real hit at the start, real invulnerability in
                             * the middle (SSB dodge-style i-frames), and a real hit at the end. */

#define STOCK_COUNT 4
#define SHIELD_MAX 60
#define MAX_JUMPS 2
#define MAX_TURNIPS 16

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
    unsigned char character_id;
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
    int btn_jump_prev;
    int btn_attack;
    int btn_attack_prev;
    int btn_shield;
    int btn_shield_prev;
    int btn_special;
    int btn_special_prev;
    
    // Combat State
    int state;
    float damage_percent;
    int stocks;
    float shield_health;
    int shield_regen_timer;
    int shield_stun_frames;
    int shield_drop_timer;
    /* shield_windup_frames / shield_overpowered_frames -- real, new 3-phase shield deploy
     * (kanban BPSW-1212..1217, "BRAWLPIT shield rework"). Set together on a fresh shield_press
     * (see physics.h's own per-frame shield block): shield_windup_frames counts down first
     * (SHIELD_WINDUP_FRAMES, real, deliberate vulnerability window -- a hit landed during this
     * window uses the exact same unmitigated resolution a completely unshielded hit would, the
     * real, intentional cost of raising your shield), then shield_overpowered_frames counts
     * down (SHIELD_OVERPOWERED_FRAMES, the shield is untouchable and punishes the ATTACKER
     * instead of taking damage). Once both reach 0 the shield is "normal" -- real, bounded
     * pushback into shield_health, matching Smash Melee's own real shield paradigm, not the old
     * unbounded-knockback bug BPSW-1212 reported. Both real, honest client-visible state (a
     * future client render pass reads shield_overpowered_frames > 0 to draw the real, named
     * "shiny Halo-shield-style" shader -- not built in this same pass, see CHANGELOG.md). */
    int shield_windup_frames;
    int shield_overpowered_frames;
    int jumps_remaining;
    int ground_platform_type;
    int drop_through_timer;
    int wavedash_frames;
    /* GOTCHA: shared by every dash-shaped action a character has (wavedash, Raccoon's Scavenger's
     * Dash/Play Dead, Rosie's High Score Rush) -- one budget across moves on purpose. */
    int dash_cooldown;
    /* GOTCHA: shared by whichever neutral-B/down-B pair a character has, not just turnip throws
     * -- also gates Medusa, Second Tree, Uncrowned, and Vexar's own specials. See
     * dispatch_special_move in physics.h. */
    int special_b_cooldown;
    int umbrella_open;

    int upb_frame;
    int upb_landing_lag;
    int parasol_rehit_timer;
    int rosie_dash_frame; /* real, new (kanban BP-TUNE-0033): 0 when not dashing, counts up each
                           * real frame of Rosie's own side-B (High Score Rush) while active --
                           * drives the real begin-hit/mid-invuln/end-hit phase timing. */
    int hitlag_frames;
    int hit_flash_timer;
    int hit_flash_multihit;
    
    // Timers
    int hitstun_frames;
    int attack_cooldown;
    int attack_timer;
    int parry_timer;
    int smash_charge_timer;
    int smash_active_timer;
    float smash_charge_level;
    int launch_delay_frames;
    float pending_kb_x;
    float pending_kb_y;
    int smash_release_timer;
    int smash_flash_timer;
    int invuln_frames;
    int respawn_timer;

    // Stats
    int kills;
    int deaths;
    
    BotGenome brain;
    unsigned int last_hit_time;
} PlayerState;

typedef struct {
    int active;
    float x, y;
    float vx, vy;
    int owner_id;
    int ttl_frames;
    unsigned char style; /* 0 bloom orb, 1 plasma pulse */
} Turnip;

typedef struct {
    int active;
    float x, y;
    float intensity;
    int timer;
} EdgeKOEffect;

typedef struct {
    int active; unsigned int timestamp;
    float x, y;
    float vx, vy;
} LagRecord;

/* MODE_SANDBOX (S248-03, BP-LOBBY-001 Phase 3, founder: "combat abilities work but dont damage
 * other characters") -- real, distinct game_mode value, not a separate ServerState field. The
 * northstar's own text names "New ServerState.mode flag" but ServerState's only real mode-like
 * field is this existing game_mode -- reused rather than adding a parallel one, since one real
 * axis (what happens when you land a hit) is enough; MODE_STOCK/MODE_TIME's own real distinction
 * (how a match ends) is orthogonal and unaffected. */
typedef enum { MODE_STOCK=0, MODE_TIME=1, MODE_SANDBOX=2 } GameMode;

typedef struct {
    PlayerState players[MAX_CLIENTS];
    Turnip turnips[MAX_TURNIPS];
    EdgeKOEffect edge_kos[MAX_EDGE_KO_EFFECTS];
    LagRecord history[MAX_CLIENTS][LAG_HISTORY];
    int server_tick;
    int game_mode;
    int match_over;
    struct sockaddr_in clients[MAX_CLIENTS];
    int client_active[MAX_CLIENTS];
} ServerState;

#endif

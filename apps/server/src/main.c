#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define usleep(x) Sleep((x)/1000)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

#include "../../../packages/common/protocol.h"
#include "../../../packages/common/physics.h"
#include "../../../packages/simulation/local_game.h"

int sock = -1;
struct sockaddr_in bind_addr;

/* S248-01 (server-side matchmaking queue, BP-LOBBY-001 Phase 1) -- real queue state, mirroring
 * ECOWAR's own real matchmaker model (apps/matchmaker/src/main.c's wait_queue) at the scale this
 * repo's single-persistent-process architecture actually supports: one queue, one match at a
 * time, no per-match process spawning (BRAWLPIT never adopted that model -- see
 * BP_LOBBY_MATCHMAKING_NORTHSTAR.md's own Phase 0 section). */
struct sockaddr_in mm_queue[MATCHMAKING_MAX_QUEUE];
int mm_queue_count = 0;
unsigned int mm_queue_started_at_ms = 0;

unsigned int get_server_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void server_net_init() {
    #ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    #endif
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    #ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
    #else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(6969); 
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr));
    printf("BRAWLPIT SERVER PORT 6969\n");
}

static int mm_addr_eq(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return memcmp(&a->sin_addr, &b->sin_addr, sizeof(struct in_addr)) == 0 &&
           a->sin_port == b->sin_port;
}

static int mm_already_queued(const struct sockaddr_in *addr) {
    for (int i = 0; i < mm_queue_count; i++) {
        if (mm_addr_eq(&mm_queue[i], addr)) return 1;
    }
    return 0;
}

/* mm_init_slot -- one real player slot's full init, factored out of the old PACKET_CONNECT
 * handler (which used to inline this exact same field list) so both the direct-connect path and
 * the new matchmaking match-start path share it verbatim rather than drifting apart. is_human
 * distinguishes a real, network-driven slot (client_active[i]=1, a real sockaddr, is_bot=0) from
 * a bot-filled one (is_bot=1, no client_active, bot_think drives it every tick like the existing
 * local single-player mode already does). */
static void mm_init_slot(int i, CharacterId character, int is_human, const struct sockaddr_in *addr) {
    local_state.players[i].active = 1;
    local_state.players[i].id = i;
    local_state.players[i].character_id = character;
    local_state.players[i].stocks = STOCK_COUNT;
    local_state.players[i].shield_health = SHIELD_MAX;
    local_state.players[i].damage_percent = 0;
    local_state.players[i].respawn_timer = 0;
    local_state.players[i].ground_platform_type = -1;
    local_state.players[i].drop_through_timer = 0;
    local_state.players[i].wavedash_frames = 0;
    local_state.players[i].dash_cooldown = 0;
    local_state.players[i].btn_special = 0;
    local_state.players[i].is_bot = is_human ? 0 : 1;
    /* Real spread instead of the old 2-player "-10/+10" formula -- up to 8 combatants need
       real, non-overlapping spawn points around the stage rather than two facing columns. */
    local_state.players[i].x = -21.0f + (float)i * 6.0f;
    phys_respawn(&local_state.players[i], get_server_time());
    if (is_human) {
        local_state.client_active[i] = 1;
        local_state.clients[i] = *addr;
    }
}

/* mm_start_match -- fires once MATCHMAKING_MAX_QUEUE (7) real players have queued, or
 * MATCHMAKING_TIMEOUT_MS have elapsed since the first one did (whichever comes first). A real,
 * fresh match: resets local_state entirely (any players from the old PACKET_CONNECT
 * direct-join path are dropped when this fires -- a real, known interaction named honestly in
 * BRAWLPIT/CHANGELOG.md rather than silently allowed to corrupt a matchmade lobby), seats every
 * queued real human into slots 1..queue_count (never slot 0 -- see MATCHMAKING_MAX_QUEUE's own
 * doc comment for why), and bot-fills the rest (slot 0 always included) using the exact same
 * bot_think this repo's own local single-player mode already relies on -- no new AI, per the
 * northstar's own explicit instruction. Sends a real PACKET_MATCH_FOUND to every seated human,
 * carrying their own assigned client_id the same way PACKET_WELCOME already does. */
static void mm_start_match(void) {
    memset(&local_state, 0, sizeof(ServerState));
    stage_set_active(STAGE_FD);
    /* S248-03: the founder's own original ask was one, unified request -- "8 random players...
       no lives... combat abilities work but dont damage other characters" describes the
       matchmaking flow itself, not a separate opt-in. Every matchmade match is real, always
       MODE_SANDBOX -- there's no "ranked matchmaking" mode in this game today for it to be an
       alternative to. */
    local_state.game_mode = MODE_SANDBOX;
    local_state.match_over = 0;

    for (int i = 0; i < mm_queue_count; i++) {
        int slot = i + 1; /* slots 1..mm_queue_count -- slot 0 stays bot-filled, see above */
        CharacterId character = (CharacterId)(slot % CHARACTER_COUNT);
        mm_init_slot(slot, character, 1, &mm_queue[i]);
    }
    for (int slot = 0; slot < MAX_CLIENTS; slot++) {
        if (local_state.players[slot].active) continue; /* already a real human above */
        CharacterId character = (CharacterId)(slot % CHARACTER_COUNT);
        mm_init_slot(slot, character, 0, NULL);
    }

    for (int i = 0; i < mm_queue_count; i++) {
        int slot = i + 1;
        NetHeader h;
        memset(&h, 0, sizeof(h));
        h.type = PACKET_MATCH_FOUND;
        h.client_id = (unsigned char)slot;
        sendto(sock, (char*)&h, sizeof(NetHeader), 0, (struct sockaddr*)&mm_queue[i], sizeof(struct sockaddr_in));
    }
    printf("MATCH STARTED: %d real player(s), %d bot(s)\n", mm_queue_count, MAX_CLIENTS - mm_queue_count);

    mm_queue_count = 0;
    mm_queue_started_at_ms = 0;
}

/* mm_tick -- called once per server frame (main()'s own loop) to fire the real timeout path.
 * The queue-full path fires immediately and synchronously from server_handle_packet itself
 * (below) the instant the 7th real player queues, so this only ever needs to check the clock. */
static void mm_tick(unsigned int now) {
    if (mm_queue_count > 0 && (now - mm_queue_started_at_ms) >= MATCHMAKING_TIMEOUT_MS) {
        mm_start_match();
    }
}

void server_handle_packet(struct sockaddr_in *sender, char *buffer, int size) {
    if (size < sizeof(NetHeader)) return;
    NetHeader *head = (NetHeader*)buffer;
    
    int client_id = -1;
    for(int i=1; i<MAX_CLIENTS; i++) {
        if (local_state.client_active[i] && 
            memcmp(&local_state.clients[i].sin_addr, &sender->sin_addr, sizeof(struct in_addr)) == 0 &&
            local_state.clients[i].sin_port == sender->sin_port) {
            client_id = i;
            break;
        }
    }
    
    if (client_id == -1 && head->type == PACKET_CONNECT) {
        for(int i=1; i<MAX_CLIENTS; i++) {
            if (!local_state.client_active[i]) {
                local_state.client_active[i] = 1;
                local_state.clients[i] = *sender;
                local_state.players[i].active = 1;
                local_state.players[i].stocks = STOCK_COUNT;
                local_state.players[i].id = i;
                local_state.players[i].shield_health = SHIELD_MAX;
                local_state.players[i].damage_percent = 0;
                local_state.players[i].respawn_timer = 0;
                local_state.players[i].ground_platform_type = -1;
                local_state.players[i].drop_through_timer = 0;
                local_state.players[i].wavedash_frames = 0;
                local_state.players[i].dash_cooldown = 0;
                local_state.players[i].btn_special = 0;
                phys_respawn(&local_state.players[i], get_server_time());
                printf("FIGHTER %d JOINED\n", i);
                
                NetHeader h;
                h.type = PACKET_WELCOME; h.client_id = i;
                // TODO(net): include server-authoritative stage_id in PACKET_WELCOME payload. 
                sendto(sock, (char*)&h, sizeof(NetHeader), 0, (struct sockaddr*)sender, sizeof(struct sockaddr_in));
                break;
            }
        }
    }

    /* S248-01: real matchmaking queue entry. Only reachable for a sender not already an active
       client (matches PACKET_CONNECT's own "client_id == -1" guard above) -- a client that
       already joined directly has no reason to also queue. */
    if (client_id == -1 && head->type == PACKET_FIND_MATCH) {
        if (!mm_already_queued(sender) && mm_queue_count < MATCHMAKING_MAX_QUEUE) {
            if (mm_queue_count == 0) mm_queue_started_at_ms = get_server_time();
            mm_queue[mm_queue_count++] = *sender;
            printf("MATCHMAKING: %d/%d queued\n", mm_queue_count, MATCHMAKING_MAX_QUEUE);
        }
        /* S248-02: a real status ack every time -- whether this call freshly enqueued, or was
           just a client's own periodic re-poll while already waiting (mm_already_queued case).
           Skipped only when mm_start_match() below actually fires this same call -- that resets
           mm_queue_count to 0 and sends real PACKET_MATCH_FOUND replies instead. */
        if (mm_queue_count < MATCHMAKING_MAX_QUEUE) {
            NetHeader status;
            memset(&status, 0, sizeof(status));
            status.type = PACKET_QUEUE_STATUS;
            status.entity_count = (unsigned char)mm_queue_count;
            sendto(sock, (char*)&status, sizeof(NetHeader), 0, (struct sockaddr*)sender, sizeof(struct sockaddr_in));
        } else {
            mm_start_match();
        }
    }

    if (client_id != -1 && head->type == PACKET_USERCMD) {
        int cursor = sizeof(NetHeader) + 1; 
        if(size >= cursor + sizeof(UserCmd)) {
             UserCmd *cmd = (UserCmd*)(buffer + cursor);
             PlayerState *p = &local_state.players[client_id];
             p->in_x = cmd->stick_x;
             p->in_y = cmd->stick_y;
             p->btn_jump = (cmd->buttons & BTN_JUMP);
             p->btn_attack = (cmd->buttons & BTN_ATTACK);
             p->btn_shield = (cmd->buttons & BTN_SHIELD);
             p->btn_special = (cmd->buttons & BTN_SPECIAL);
        }
    }
}

void server_broadcast() {
    char buffer[4096];
    int cursor = 0;
    NetHeader head;
    head.type = PACKET_SNAPSHOT; head.client_id = 0;
    // TODO(net): include stage_id in snapshots if stage swaps are supported mid-match. 
    head.timestamp = get_server_time();
    
    unsigned char count = 0;
    for(int i=0; i<MAX_CLIENTS; i++) if (local_state.players[i].active) count++;
    head.entity_count = count;
    
    memcpy(buffer + cursor, &head, sizeof(NetHeader)); cursor += sizeof(NetHeader);
    memcpy(buffer + cursor, &count, 1); cursor += 1;
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        PlayerState *p = &local_state.players[i];
        if (p->active) {
            NetPlayer np;
            np.id = (unsigned char)i;
            np.x = p->x; np.y = p->y;
            np.vx = p->vx; np.vy = p->vy;
            np.state = (unsigned char)p->state;
            np.damage = (unsigned short)p->damage_percent;
            np.stocks = (unsigned char)p->stocks;
            np.shield = (unsigned char)p->shield_health;
            np.facing = (p->facing > 0);
            memcpy(buffer + cursor, &np, sizeof(NetPlayer)); cursor += sizeof(NetPlayer);
        }
    }
    
    for(int i=1; i<MAX_CLIENTS; i++) {
        if (local_state.client_active[i]) {
            sendto(sock, buffer, cursor, 0, (struct sockaddr*)&local_state.clients[i], sizeof(struct sockaddr_in));
        }
    }
}

int main() {
    server_net_init();
    local_init_match(1, 0, STAGE_FD, CHARACTER_PETALIA, CHARACTER_VEXAR); 
    
    while(1) {
        char buffer[1024];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);
        int len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender, &slen);
        while (len > 0) {
            server_handle_packet(&sender, buffer, len);
            len = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&sender, &slen);
        }
        
        // Tick
        mm_tick(get_server_time()); // S248-01: real matchmaking timeout check
        local_update(0,0,0,0,0,0, NULL, get_server_time());
        server_broadcast();
        
        #ifdef _WIN32
        Sleep(16);
        #else
        usleep(16000);
        #endif
    }
    return 0;
}

#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

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
#include "../../../packages/common/level_registry.h"
#include "../../../packages/simulation/local_game.h"

int sock = -1;
struct sockaddr_in bind_addr;

/* g_server_stage_id (S421-03, founder real-time: "can we train on the level called THREE from
 * the registry?") -- which real stage local_init_match uses, at boot AND on every
 * PACKET_RESET_MATCH. Defaults to STAGE_FD (today's unchanged behavior). When --level <name>
 * names a real level in the public registry, main() resolves it once at startup via
 * fetch_registry_list/fetch_registry_level (packages/common/level_registry.h, the same real
 * client already uses for its own level browser) and loads it with
 * stage_set_active_from_leveldata -- this is set to STAGE_CUSTOM_MEMORY afterward so
 * local_init_match's own internal stage_set_active(g_server_stage_id) call is the real, documented
 * no-op that preserves it (see physics.h's own STAGE_CUSTOM_MEMORY doc comment) rather than
 * re-loading (or worse, silently reverting to STAGE_FD) on every reset. */
int g_server_stage_id = STAGE_FD;

/* S248-01 (server-side matchmaking queue, BP-LOBBY-001 Phase 1) -- real queue state, mirroring
 * ECOWAR's own real matchmaker model (apps/matchmaker/src/main.c's wait_queue) at the scale this
 * repo's single-persistent-process architecture actually supports: one queue, one match at a
 * time, no per-match process spawning (BRAWLPIT never adopted that model -- see
 * BP_LOBBY_MATCHMAKING_NORTHSTAR.md's own Phase 0 section). */
struct sockaddr_in mm_queue[MATCHMAKING_MAX_QUEUE];
int mm_queue_count = 0;
unsigned int mm_queue_started_at_ms = 0;

/* BPMM-1202020 -- real, separate 1v1 queue, see protocol.h's own MATCHMAKING_1V1_* doc comment
 * for why this isn't just the FFA queue with different numbers. */
struct sockaddr_in mm_queue_1v1[MATCHMAKING_1V1_MAX_QUEUE];
int mm_queue_1v1_count = 0;
unsigned int mm_queue_1v1_started_at_ms = 0;

unsigned int get_server_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void server_net_init(int port) {
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
    /* BPMM-12441/12442 root cause: this used to be 6969, the same literal port SHANKPIT's own
     * live apps2/server-go binds on this exact host (confirmed live via `ss -ulnp` -- shank_server
     * already owns 0.0.0.0:6969 whenever it's running, which is effectively always). bind() below
     * never checked its return value, so on a real collision this server silently kept running
     * with an UNBOUND socket -- every client PACKET_FIND_MATCH sent to 6969 was delivered to
     * SHANKPIT's process instead (a different wire protocol, so it was just silently dropped),
     * which is exactly why matchmaking "didn't get me in a game" at 1 client, 2 clients, or any
     * count -- no real PACKET_QUEUE_STATUS/PACKET_MATCH_FOUND could ever reach a client no matter
     * how long they waited. Moved to 6978 (real, verified free on this host, no other repo in the
     * monorepo claims it) to end the collision permanently, and the bind() call is now checked and
     * fatal on failure so a future port fight fails loudly at startup instead of silently eating
     * every future connection.
     *
     * S419-11: `port` is now a real parameter (--port CLI flag, default 6978 -- an existing
     * deploy launching this binary with no flags is completely unaffected) rather than a
     * hardcoded constant -- scripts/rl_train_packet.py's own orchestrator needs THREE of these
     * running simultaneously (one dedicated server per league archetype), which is impossible
     * with one fixed port. */
    bind_addr.sin_port = htons((uint16_t)port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        perror("BRAWLPIT SERVER: bind() failed");
        exit(1);
    }
    printf("BRAWLPIT SERVER PORT %d\n", port);
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

/* BPMM-1202020 -- 1v1 queue's own membership check, mirroring mm_already_queued exactly but
 * against mm_queue_1v1 instead. Kept as a separate function (not a generalized "which queue"
 * parameter added to the one above) since every other real matchmaking helper in this file
 * (mm_init_slot aside, which both modes already share) is about to grow its own 1v1 sibling too
 * -- consistent with how mm_start_match itself stays FFA-only below, not parameterized. */
static int mm_already_queued_1v1(const struct sockaddr_in *addr) {
    for (int i = 0; i < mm_queue_1v1_count; i++) {
        if (mm_addr_eq(&mm_queue_1v1[i], addr)) return 1;
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

/* mm_start_match_1v1 (BPMM-1202020) -- fires once MATCHMAKING_1V1_MAX_QUEUE (2) real players
 * have queued, or MATCHMAKING_1V1_TIMEOUT_MS have elapsed since the first one did. A real,
 * separate match shape from mm_start_match's own 8-player sandbox FFA: exactly 2 combatants
 * (slots 1 and 2 -- slot 0 stays reserved for the boot-time local demo, same convention as the
 * FFA path), real MODE_STOCK (lives-on-the-line, not sandbox -- a 1v1 duel is the real game,
 * not the FFA's own explicit "no lives" mode), a bot seated in slot 2 the instant a second real
 * human hasn't queued too. Same real, known "drops any in-progress match" interaction as
 * mm_start_match (a fresh `memset(&local_state, ...)`), named honestly there and true here too.
 */
static void mm_start_match_1v1(void) {
    memset(&local_state, 0, sizeof(ServerState));
    stage_set_active(STAGE_FD);
    local_state.game_mode = MODE_STOCK;
    local_state.match_over = 0;

    for (int i = 0; i < mm_queue_1v1_count; i++) {
        int slot = i + 1; /* slots 1-2 -- slot 0 stays reserved, see above */
        CharacterId character = (CharacterId)(slot % CHARACTER_COUNT);
        mm_init_slot(slot, character, 1, &mm_queue_1v1[i]);
    }
    /* Bot-fill only up to 2 total combatants (slot 2 at most) -- unlike mm_start_match, this
       deliberately does NOT fill every remaining slot up to MAX_CLIENTS; a 1v1 stays 1v1. */
    for (int slot = 1; slot <= 2; slot++) {
        if (local_state.players[slot].active) continue; /* already a real human above */
        CharacterId character = (CharacterId)(slot % CHARACTER_COUNT);
        mm_init_slot(slot, character, 0, NULL);
    }

    for (int i = 0; i < mm_queue_1v1_count; i++) {
        int slot = i + 1;
        NetHeader h;
        memset(&h, 0, sizeof(h));
        h.type = PACKET_MATCH_FOUND;
        h.client_id = (unsigned char)slot;
        sendto(sock, (char*)&h, sizeof(NetHeader), 0, (struct sockaddr*)&mm_queue_1v1[i], sizeof(struct sockaddr_in));
    }
    printf("1v1 MATCH STARTED: %d real player(s), %d bot(s)\n", mm_queue_1v1_count, 2 - mm_queue_1v1_count);

    mm_queue_1v1_count = 0;
    mm_queue_1v1_started_at_ms = 0;
}

/* mm_tick -- called once per server frame (main()'s own loop) to fire the real timeout path for
 * both queues. The queue-full path fires immediately and synchronously from server_handle_packet
 * itself (below) the instant the last needed real player queues, so this only ever needs to
 * check the clock for both. */
static void mm_tick(unsigned int now) {
    if (mm_queue_count > 0 && (now - mm_queue_started_at_ms) >= MATCHMAKING_TIMEOUT_MS) {
        mm_start_match();
    }
    if (mm_queue_1v1_count > 0 && (now - mm_queue_1v1_started_at_ms) >= MATCHMAKING_1V1_TIMEOUT_MS) {
        mm_start_match_1v1();
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
       already joined directly has no reason to also queue.
       BPMM-1202020: the request's own NetHeader.entity_count now carries which queue this
       targets -- MATCHMAKING_MODE_1V1 (1) or MATCHMAKING_MODE_FFA (0, also the default for any
       older/unrecognized value, matching this field's real zero-value before this feature
       existed -- a pre-existing client that never set it still gets the original FFA behavior
       unchanged). */
    if (client_id == -1 && head->type == PACKET_FIND_MATCH) {
        if (head->entity_count == MATCHMAKING_MODE_1V1) {
            if (!mm_already_queued_1v1(sender) && mm_queue_1v1_count < MATCHMAKING_1V1_MAX_QUEUE) {
                if (mm_queue_1v1_count == 0) mm_queue_1v1_started_at_ms = get_server_time();
                mm_queue_1v1[mm_queue_1v1_count++] = *sender;
                printf("1v1 MATCHMAKING: %d/%d queued\n", mm_queue_1v1_count, MATCHMAKING_1V1_MAX_QUEUE);
            }
            if (mm_queue_1v1_count < MATCHMAKING_1V1_MAX_QUEUE) {
                NetHeader status;
                memset(&status, 0, sizeof(status));
                status.type = PACKET_QUEUE_STATUS;
                status.entity_count = (unsigned char)mm_queue_1v1_count;
                sendto(sock, (char*)&status, sizeof(NetHeader), 0, (struct sockaddr*)sender, sizeof(struct sockaddr_in));
            } else {
                mm_start_match_1v1();
            }
        } else {
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

    if (client_id != -1 && head->type == PACKET_RESET_MATCH) {
        /* S419-07 -- see protocol.h's own doc comment for the full real rationale. Save this
         * one sender's own network binding across the reset (local_init_match's own
         * memset(&local_state, 0, ...) would otherwise drop every real connection, exactly the
         * same real interaction mm_start_match's own doc comment already names for a different
         * reason), then re-seat it into the exact same slot.
         *
         * Real, found-live bug caught (and fixed here) while writing this handler's own live
         * verification test: local_init_match(1, ...) -- note num_players=1 -- only initializes
         * PLAYER SLOT 0 (PETALIA); slot 1+ have never gotten their stocks/shield/spawn from
         * local_init_match at all, only from the PACKET_CONNECT handler's own separate init
         * block above. Re-running just local_init_match and manually flipping active/is_bot (an
         * earlier version of this fix) left the client's own slot with zeroed-out stocks/shield
         * from the memset -- reusing mm_init_slot (the exact same real per-slot init connect
         * already shares with matchmaking) is the correct, complete fix, not a smaller patch. */
        struct sockaddr_in saved_addr = local_state.clients[client_id];
        local_init_match(1, 0, g_server_stage_id, CHARACTER_PETALIA, CHARACTER_VEXAR);
        mm_init_slot(client_id, CHARACTER_VEXAR, 1, &saved_addr);

        NetHeader ack;
        memset(&ack, 0, sizeof(ack));
        ack.type = PACKET_RESET_ACK;
        ack.client_id = (unsigned char)client_id;
        sendto(sock, (char*)&ack, sizeof(NetHeader), 0, (struct sockaddr*)sender, sizeof(struct sockaddr_in));
        printf("MATCH RESET (requested by client %d)\n", client_id);
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
            /* Real, found-live bug (S419, while building the packet-level RL training pipeline):
             * jump_count/hit_stun were NEVER assigned here -- `NetPlayer np;` is an uninitialized
             * stack local, so these two of NetPlayer's 12 real fields have been shipping raw
             * stack garbage over the wire in every snapshot this server has ever sent. Harmless to
             * human play so far (no client code reads them -- confirmed via grep), but would have
             * silently poisoned two real observation fields for any packet-level RL/analysis
             * consumer. Clamped to the wire's own real ranges (jump_count is unsigned char,
             * MAX_JUMPS is small; hit_stun as a real frame count, clamped so a value that would
             * overflow the field doesn't wrap into a misleadingly small number on the wire).
             */
            np.jump_count = (unsigned char)(p->jumps_remaining < 0 ? 0 : p->jumps_remaining > 255 ? 255 : p->jumps_remaining);
            np.hit_stun = (unsigned char)(p->hitstun_frames < 0 ? 0 : p->hitstun_frames > 255 ? 255 : p->hitstun_frames);
            memcpy(buffer + cursor, &np, sizeof(NetPlayer)); cursor += sizeof(NetPlayer);
        }
    }
    
    for(int i=1; i<MAX_CLIENTS; i++) {
        if (local_state.client_active[i]) {
            sendto(sock, buffer, cursor, 0, (struct sockaddr*)&local_state.clients[i], sizeof(struct sockaddr_in));
        }
    }
}

int main(int argc, char **argv) {
    /* --fast-forward (S419, founder real-time: "build a training pipeline reinforcement
     * learning on the packet level... take the ability to FAST FORWARD" from ECOWAR's own
     * real apps/arena_server precedent, BACKLOG.md SECTION 377). Defaults to today's unchanged
     * real-time-paced 16ms behavior -- an existing deploy launching this binary with no flags
     * is completely unaffected. Simply skips the real-time usleep pacing so ticks run back-to-
     * back as fast as the CPU allows, for real, networked (actual UDP wire protocol) bot-vs-bot
     * training data generation, distinct from a from-scratch in-process training harness.
     *
     * Real, honest, named scope cut from ECOWAR's own sibling flag pair: no --tick-ms here.
     * ECOWAR's arena_update(dt_ms) takes an explicit simulated-time-per-tick parameter;
     * BRAWLPIT's own local_update has no such parameter at all (see local_game.h) -- its
     * physics stepping assumes a fixed real tick internally. Changing that would mean touching
     * core physics timing, a real, separate, riskier change than what --fast-forward alone
     * needs to deliver (raw wall-clock training throughput), so it's left undone rather than
     * forced through here.
     *
     * Also, unlike ECOWAR's own ARENA_PHASE_WAITING/LIVE split, BRAWLPIT's server has no
     * "waiting for a real UDP handshake before the sim starts" phase to preserve real-time
     * pacing for -- local_init_match runs once at boot regardless of client connections, so
     * there's no equivalent gotcha to guard against here. */
    int fast_forward = 0;
    int port = 6978; /* real, existing default -- see server_net_init's own doc comment */
    const char *level_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fast-forward") == 0) fast_forward = 1;
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) level_name = argv[++i];
    }

    /* --level <name> (S421-03, founder real-time: "can we train on the level called THREE from
     * the registry?") -- resolves a real level by NAME from the public registry (the same one
     * BRAWLPIT's own client-side level browser already reads, packages/common/level_registry.h)
     * and loads it BEFORE local_init_match, matching stage_set_active_from_leveldata's own real,
     * documented ordering requirement. A real, honest degrade on any failure (name not found,
     * registry unreachable, malformed level) -- falls back to STAGE_FD rather than failing to
     * start, matching this repo's own established "a bad/missing resource never corrupts what's
     * already working" convention. */
    if (level_name) {
        RegistryEntry entries[MAX_REGISTRY_ENTRIES];
        int count = fetch_registry_list(entries, MAX_REGISTRY_ENTRIES);
        int found_id = -1;
        for (int i = 0; i < count; i++) {
            if (strcmp(entries[i].name, level_name) == 0) {
                found_id = entries[i].id;
                break;
            }
        }
        if (found_id < 0) {
            printf("--level '%s' not found in the registry (%d level(s) listed) -- using STAGE_FD\n", level_name, count);
        } else {
            LevelData lvl;
            if (fetch_registry_level(found_id, &lvl)) {
                stage_set_active_from_leveldata(&lvl);
                g_server_stage_id = STAGE_CUSTOM_MEMORY;
                printf("--level '%s' (id=%d) loaded from the registry\n", level_name, found_id);
            } else {
                printf("--level '%s' (id=%d) found but failed to fetch/parse -- using STAGE_FD\n", level_name, found_id);
            }
        }
    }

    server_net_init(port);
    local_init_match(1, 0, g_server_stage_id, CHARACTER_PETALIA, CHARACTER_VEXAR);

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

        if (!fast_forward) {
            #ifdef _WIN32
            Sleep(16);
            #else
            usleep(16000);
            #endif
        }
    }
    return 0;
}

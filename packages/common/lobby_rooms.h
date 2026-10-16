/* lobby_rooms.h -- BRAWLPIT lobby rooms ("servers" in the Among-Us sense): a player creates a room,
 * others join by code, everyone chats, non-hosts press Ready, and the host presses Start.
 *
 * Pure state machine: no sockets, no clock, no randomness, no malloc (fixed arrays only).
 * The caller passes `now` (seconds) and a 64-bit random seed where one is needed, so this is
 * deterministic and unit-testable (tests/test_lobby_rooms.c).
 *
 * Identity is a per-player token (hex string). PELLEGRINO has no cookies, so the web pages carry it
 * in the URL / a hidden form field; whoever holds the token is that player.
 */
#ifndef BRAWLPIT_LOBBY_ROOMS_H
#define BRAWLPIT_LOBBY_ROOMS_H

#include <stdint.h>
#include <string.h>

#define LOBBY_MAX_ROOMS    32
#define LOBBY_MAX_PLAYERS  8
#define LOBBY_MIN_TO_START 2
#define LOBBY_CHAT_LINES   24
#define LOBBY_NAME_LEN     17   /* 16 chars + NUL */
#define LOBBY_TITLE_LEN    25   /* 24 chars + NUL */
#define LOBBY_MSG_LEN      121  /* 120 chars + NUL */
#define LOBBY_TOKEN_LEN    17   /* 16 hex chars + NUL */
#define LOBBY_CODE_LEN     5    /* 4 letters + NUL */
#define LOBBY_IDLE_DROP_S  180  /* a player not heard from in this long is dropped */
#define LOBBY_ROOM_REAP_S  1800 /* a started room is forgotten this long after it started */
#define LOBBY_CHAT_GAP_S   1    /* min seconds between one player's chat lines */

typedef enum {
    LOBBY_OK = 0,
    LOBBY_ERR_BAD_NAME,
    LOBBY_ERR_NO_ROOM,
    LOBBY_ERR_FULL,
    LOBBY_ERR_STARTED,
    LOBBY_ERR_NAME_TAKEN,
    LOBBY_ERR_NO_SPACE,
    LOBBY_ERR_NOT_LEADER,
    LOBBY_ERR_NOT_READY,
    LOBBY_ERR_TOO_FEW,
    LOBBY_ERR_BAD_TOKEN,
    LOBBY_ERR_EMPTY_MSG,
    LOBBY_ERR_TOO_FAST,
    LOBBY_ERR_IS_LEADER
} LobbyErr;

typedef struct {
    int      used;
    char     name[LOBBY_NAME_LEN];
    char     token[LOBBY_TOKEN_LEN];
    int      ready;
    long     last_seen;
    long     last_chat;
} LobbyPlayer;

typedef struct {
    int  seq;                       /* monotonically increasing line number */
    char who[LOBBY_NAME_LEN];       /* empty = system line */
    char text[LOBBY_MSG_LEN];
} LobbyLine;

typedef struct {
    int         used;
    char        code[LOBBY_CODE_LEN];
    char        title[LOBBY_TITLE_LEN];
    int         leader;             /* index into players[] */
    int         started;
    long        started_at;
    long        created_at;
    LobbyPlayer players[LOBBY_MAX_PLAYERS];
    LobbyLine   chat[LOBBY_CHAT_LINES]; /* ring, oldest overwritten */
    int         chat_next_seq;      /* next seq to assign; line i lives at chat[i % LOBBY_CHAT_LINES] */
} LobbyRoom;

typedef struct {
    LobbyRoom rooms[LOBBY_MAX_ROOMS];
    uint64_t  rng;                  /* xorshift state, mixed from caller-supplied entropy */
} Lobbies;

/* ---- small helpers ---- */

static inline uint64_t lobby_next_rand(Lobbies *L) {
    uint64_t x = L->rng ? L->rng : 0x9E3779B97F4A7C15ULL;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    L->rng = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static inline void lobby_init(Lobbies *L, uint64_t entropy) {
    memset(L, 0, sizeof *L);
    L->rng = entropy ? entropy : 0x9E3779B97F4A7C15ULL;
}

/* Keep printable ASCII only, collapse nothing, trim ends. Returns length. */
static inline int lobby_clean(char *dst, int cap, const char *src) {
    int n = 0;
    if (!src) { dst[0] = 0; return 0; }
    while (*src == ' ') src++;
    for (; *src && n < cap - 1; src++) {
        unsigned char c = (unsigned char)*src;
        if (c < 32 || c > 126) continue;
        dst[n++] = (char)c;
    }
    while (n > 0 && dst[n - 1] == ' ') n--;
    dst[n] = 0;
    return n;
}

static inline int lobby_streq_nocase(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

static inline int lobby_count_players(const LobbyRoom *r) {
    int n = 0;
    for (int i = 0; i < LOBBY_MAX_PLAYERS; i++) n += r->players[i].used;
    return n;
}

static inline void lobby_say(LobbyRoom *r, const char *who, const char *text) {
    LobbyLine *ln = &r->chat[r->chat_next_seq % LOBBY_CHAT_LINES];
    ln->seq = r->chat_next_seq++;
    lobby_clean(ln->who, LOBBY_NAME_LEN, who ? who : "");
    lobby_clean(ln->text, LOBBY_MSG_LEN, text);
}

static inline LobbyRoom *lobby_find_room(Lobbies *L, const char *code) {
    if (!code) return 0;
    for (int i = 0; i < LOBBY_MAX_ROOMS; i++)
        if (L->rooms[i].used && lobby_streq_nocase(L->rooms[i].code, code))
            return &L->rooms[i];
    return 0;
}

/* Look a token up. Fills *room_out / *pidx_out. Returns 1 if found. Refreshes last_seen. */
static inline int lobby_find_player(Lobbies *L, const char *token, long now,
                                    LobbyRoom **room_out, int *pidx_out) {
    if (!token || !token[0]) return 0;
    for (int i = 0; i < LOBBY_MAX_ROOMS; i++) {
        LobbyRoom *r = &L->rooms[i];
        if (!r->used) continue;
        for (int p = 0; p < LOBBY_MAX_PLAYERS; p++) {
            if (r->players[p].used && strcmp(r->players[p].token, token) == 0) {
                r->players[p].last_seen = now;
                if (room_out) *room_out = r;
                if (pidx_out) *pidx_out = p;
                return 1;
            }
        }
    }
    return 0;
}

static inline void lobby_make_token(Lobbies *L, char out[LOBBY_TOKEN_LEN]) {
    static const char hex[] = "0123456789abcdef";
    uint64_t v = lobby_next_rand(L);
    for (int i = 0; i < 16; i++) { out[i] = hex[v & 15]; v >>= 4; }
    out[16] = 0;
}

static inline void lobby_make_code(Lobbies *L, char out[LOBBY_CODE_LEN]) {
    static const char alpha[] = "BCDFGHJKLMNPQRSTVWXZ"; /* no vowels: no accidental words */
    for (int attempt = 0; attempt < 64; attempt++) {
        uint64_t v = lobby_next_rand(L);
        for (int i = 0; i < 4; i++) { out[i] = alpha[v % 20]; v /= 20; }
        out[4] = 0;
        if (!lobby_find_room(L, out)) return;
    }
}

static inline void lobby_leave_idx(LobbyRoom *r, int p);

/* Drop idle players and stale rooms. Cheap enough to call on every request. */
static inline void lobby_reap(Lobbies *L, long now) {
    for (int i = 0; i < LOBBY_MAX_ROOMS; i++) {
        LobbyRoom *r = &L->rooms[i];
        if (!r->used) continue;
        for (int p = 0; p < LOBBY_MAX_PLAYERS; p++) {
            if (r->players[p].used && now - r->players[p].last_seen > LOBBY_IDLE_DROP_S) {
                lobby_leave_idx(r, p);
                if (!r->used) break;
            }
        }
        if (r->used && r->started && now - r->started_at > LOBBY_ROOM_REAP_S)
            memset(r, 0, sizeof *r);
    }
}

/* ---- actions ---- */

static inline LobbyErr lobby_create(Lobbies *L, const char *name_in, const char *title_in,
                                    long now, char token_out[LOBBY_TOKEN_LEN],
                                    char code_out[LOBBY_CODE_LEN]) {
    char name[LOBBY_NAME_LEN], title[LOBBY_TITLE_LEN];
    if (lobby_clean(name, sizeof name, name_in) < 1) return LOBBY_ERR_BAD_NAME;
    if (lobby_clean(title, sizeof title, title_in) < 1) {
        strcpy(title, name);
        strncat(title, "'s lobby", sizeof title - strlen(title) - 1);
    }
    for (int i = 0; i < LOBBY_MAX_ROOMS; i++) {
        LobbyRoom *r = &L->rooms[i];
        if (r->used) continue;
        memset(r, 0, sizeof *r);
        r->used = 1;
        lobby_make_code(L, r->code);
        strcpy(r->title, title);
        r->created_at = now;
        r->leader = 0;
        r->players[0].used = 1;
        strcpy(r->players[0].name, name);
        lobby_make_token(L, r->players[0].token);
        r->players[0].last_seen = now;
        strcpy(token_out, r->players[0].token);
        strcpy(code_out, r->code);
        lobby_say(r, "", "Lobby created. Share the code and wait for players.");
        return LOBBY_OK;
    }
    return LOBBY_ERR_NO_SPACE;
}

static inline LobbyErr lobby_join(Lobbies *L, const char *code, const char *name_in, long now,
                                  char token_out[LOBBY_TOKEN_LEN]) {
    char name[LOBBY_NAME_LEN];
    char msg[LOBBY_MSG_LEN];
    LobbyRoom *r = lobby_find_room(L, code);
    if (lobby_clean(name, sizeof name, name_in) < 1) return LOBBY_ERR_BAD_NAME;
    if (!r) return LOBBY_ERR_NO_ROOM;
    if (r->started) return LOBBY_ERR_STARTED;
    for (int p = 0; p < LOBBY_MAX_PLAYERS; p++)
        if (r->players[p].used && lobby_streq_nocase(r->players[p].name, name))
            return LOBBY_ERR_NAME_TAKEN;
    for (int p = 0; p < LOBBY_MAX_PLAYERS; p++) {
        if (r->players[p].used) continue;
        memset(&r->players[p], 0, sizeof r->players[p]);
        r->players[p].used = 1;
        strcpy(r->players[p].name, name);
        lobby_make_token(L, r->players[p].token);
        r->players[p].last_seen = now;
        strcpy(token_out, r->players[p].token);
        strcpy(msg, name); strncat(msg, " joined.", sizeof msg - strlen(msg) - 1);
        lobby_say(r, "", msg);
        return LOBBY_OK;
    }
    return LOBBY_ERR_FULL;
}

static inline void lobby_leave_idx(LobbyRoom *r, int p) {
    char msg[LOBBY_MSG_LEN];
    if (!r->players[p].used) return;
    strcpy(msg, r->players[p].name); strncat(msg, " left.", sizeof msg - strlen(msg) - 1);
    memset(&r->players[p], 0, sizeof r->players[p]);
    if (lobby_count_players(r) == 0) { memset(r, 0, sizeof *r); return; }
    lobby_say(r, "", msg);
    if (r->leader == p) {
        for (int q = 0; q < LOBBY_MAX_PLAYERS; q++) {
            if (r->players[q].used) {
                r->leader = q;
                r->players[q].ready = 0;
                strcpy(msg, r->players[q].name);
                strncat(msg, " is now the host.", sizeof msg - strlen(msg) - 1);
                lobby_say(r, "", msg);
                break;
            }
        }
    }
}

static inline LobbyErr lobby_leave(Lobbies *L, const char *token, long now) {
    LobbyRoom *r; int p;
    if (!lobby_find_player(L, token, now, &r, &p)) return LOBBY_ERR_BAD_TOKEN;
    lobby_leave_idx(r, p);
    return LOBBY_OK;
}

static inline LobbyErr lobby_toggle_ready(Lobbies *L, const char *token, long now) {
    LobbyRoom *r; int p; char msg[LOBBY_MSG_LEN];
    if (!lobby_find_player(L, token, now, &r, &p)) return LOBBY_ERR_BAD_TOKEN;
    if (r->started) return LOBBY_ERR_STARTED;
    if (r->leader == p) return LOBBY_ERR_IS_LEADER;
    r->players[p].ready = !r->players[p].ready;
    strcpy(msg, r->players[p].name);
    strncat(msg, r->players[p].ready ? " is ready." : " is not ready.", sizeof msg - strlen(msg) - 1);
    lobby_say(r, "", msg);
    return LOBBY_OK;
}

static inline LobbyErr lobby_chat(Lobbies *L, const char *token, const char *text_in, long now) {
    LobbyRoom *r; int p; char text[LOBBY_MSG_LEN];
    if (!lobby_find_player(L, token, now, &r, &p)) return LOBBY_ERR_BAD_TOKEN;
    if (lobby_clean(text, sizeof text, text_in) < 1) return LOBBY_ERR_EMPTY_MSG;
    if (r->players[p].last_chat && now - r->players[p].last_chat < LOBBY_CHAT_GAP_S)
        return LOBBY_ERR_TOO_FAST;
    r->players[p].last_chat = now;
    lobby_say(r, r->players[p].name, text);
    return LOBBY_OK;
}

/* Everyone but the host must be ready; need at least LOBBY_MIN_TO_START players. */
static inline LobbyErr lobby_can_start(const LobbyRoom *r) {
    if (lobby_count_players(r) < LOBBY_MIN_TO_START) return LOBBY_ERR_TOO_FEW;
    for (int p = 0; p < LOBBY_MAX_PLAYERS; p++)
        if (r->players[p].used && p != r->leader && !r->players[p].ready) return LOBBY_ERR_NOT_READY;
    return LOBBY_OK;
}

static inline LobbyErr lobby_start(Lobbies *L, const char *token, long now) {
    LobbyRoom *r; int p; LobbyErr e;
    if (!lobby_find_player(L, token, now, &r, &p)) return LOBBY_ERR_BAD_TOKEN;
    if (r->started) return LOBBY_ERR_STARTED;
    if (r->leader != p) return LOBBY_ERR_NOT_LEADER;
    e = lobby_can_start(r);
    if (e != LOBBY_OK) return e;
    r->started = 1;
    r->started_at = now;
    lobby_say(r, "", "The host started the match. Good luck!");
    return LOBBY_OK;
}

static inline const char *lobby_err_text(LobbyErr e) {
    switch (e) {
    case LOBBY_OK: return "ok";
    case LOBBY_ERR_BAD_NAME: return "Pick a name first (letters and numbers, up to 16).";
    case LOBBY_ERR_NO_ROOM: return "That lobby no longer exists.";
    case LOBBY_ERR_FULL: return "That lobby is full.";
    case LOBBY_ERR_STARTED: return "That match has already started.";
    case LOBBY_ERR_NAME_TAKEN: return "Someone in that lobby already uses that name.";
    case LOBBY_ERR_NO_SPACE: return "The server has no free lobby slots right now.";
    case LOBBY_ERR_NOT_LEADER: return "Only the host can start the match.";
    case LOBBY_ERR_NOT_READY: return "Everyone else has to press Ready first.";
    case LOBBY_ERR_TOO_FEW: return "You need at least two players to start.";
    case LOBBY_ERR_BAD_TOKEN: return "You are not in a lobby (it may have timed out).";
    case LOBBY_ERR_EMPTY_MSG: return "Nothing to send.";
    case LOBBY_ERR_TOO_FAST: return "Easy there, one message per second.";
    case LOBBY_ERR_IS_LEADER: return "The host does not need to ready up.";
    }
    return "error";
}

#endif

/* lobby_pages.h -- renders the BRAWLPIT lobby as plain HTML + CSS for the PELLEGRINO browser
 * (PELLEGRINO/README.md lists exactly what its engine supports; this file stays inside that subset:
 * block boxes with border/padding/margin/width, display:flex rows with gap, custom properties,
 * GET/POST forms with text + submit controls, <a href>, h1-h3, <p>, <ul>).
 *
 * No JavaScript (PELLEGRINO has none), no cookies (identity is the `t` token in the URL / a hidden
 * field), nothing wider than 24 form controls per page, text values <= 32 bytes per control.
 * Output goes to a caller-owned fixed buffer; nothing here allocates.
 */
#ifndef BRAWLPIT_LOBBY_PAGES_H
#define BRAWLPIT_LOBBY_PAGES_H

#include <stdio.h>
#include <string.h>
#include "lobby_rooms.h"

typedef struct {
    char *buf;
    int   cap;
    int   len;
    int   overflow;
} LobbyBuf;

static inline void lb_raw(LobbyBuf *b, const char *s) {
    int n = (int)strlen(s);
    if (b->len + n >= b->cap) { b->overflow = 1; return; }
    memcpy(b->buf + b->len, s, (size_t)n);
    b->len += n;
    b->buf[b->len] = 0;
}

static inline void lb_esc(LobbyBuf *b, const char *s) {
    for (; *s; s++) {
        switch (*s) {
        case '&': lb_raw(b, "&amp;"); break;
        case '<': lb_raw(b, "&lt;"); break;
        case '>': lb_raw(b, "&gt;"); break;
        case '"': lb_raw(b, "&quot;"); break;
        case '\'': lb_raw(b, "&#39;"); break;
        default: {
            char c[2] = { *s, 0 };
            lb_raw(b, c);
        }
        }
    }
}

static inline void lb_int(LobbyBuf *b, long v) {
    char t[24];
    snprintf(t, sizeof t, "%ld", v);
    lb_raw(b, t);
}

static const char LOBBY_CSS[] =
    ":root{--bg:#120f0b;--panel:#1d1812;--edge:#4a3b24;--amber:#ffb000;--ink:#f3e8d2;"
    "--dim:#8d8168;--ok:#7be39a;--warn:#ff7a66;--blue:#7cb8ff}"
    "html{background:var(--bg)}"
    "body{background:var(--bg);color:var(--ink);margin:0;padding:18px 36px}"
    ".wrap{max-width:960px;margin:0 auto}"
    "h1{color:var(--amber);margin:0}"
    "h2{color:var(--amber)}"
    "h3{color:var(--ink)}"
    ".tag{color:var(--dim)}"
    ".card{background:var(--panel);border:1px solid var(--edge);padding:12px 18px;margin:9px 0}"
    ".cols{display:flex;gap:18px}"
    ".ok{color:var(--ok)}"
    ".warn{color:var(--warn)}"
    ".dim{color:var(--dim)}"
    ".amber{color:var(--amber)}"
    ".code{color:var(--amber);margin:0}"
    ".cols .card{margin:0}"
    "form{margin:9px 0}"
    ".foot{margin:18px 0 0}"
    ".banner{background:#2b1813;border:1px solid var(--warn);color:var(--warn);padding:6px 18px;margin:9px 0}"
    ".go{background:#12281a;border:1px solid var(--ok);color:var(--ok);padding:9px 18px;margin:9px 0}"
    "a{color:var(--blue)}"
    "input{color:var(--ink)}"
    "p{margin:0}";

static inline void lp_head(LobbyBuf *b, const char *title) {
    lb_raw(b, "<!doctype html><html><head><meta charset=\"utf-8\"><title>");
    lb_esc(b, title);
    lb_raw(b, " - BRAWLPIT</title><style>");
    lb_raw(b, LOBBY_CSS);
    lb_raw(b, "</style></head><body><div class=\"wrap\">");
    lb_raw(b, "<h1>BRAWLPIT</h1><p class=\"tag\">lobbies &middot; pick a lobby, ready up, fight</p>");
}

static inline void lp_foot(LobbyBuf *b) {
    lb_raw(b, "<p class=\"dim foot\">Lobbies live in server memory and idle players are dropped after ");
    lb_int(b, LOBBY_IDLE_DROP_S);
    lb_raw(b, " seconds. Use Refresh to see new players and chat.</p></div></body></html>");
}

static inline void lp_banner(LobbyBuf *b, LobbyErr e) {
    if (e == LOBBY_OK) return;
    lb_raw(b, "<div class=\"banner\">");
    lb_esc(b, lobby_err_text(e));
    lb_raw(b, "</div>");
}

/* ---- home: create form + list of open lobbies ---- */
static inline void lobby_page_home(LobbyBuf *b, Lobbies *L, LobbyErr err, const char *game_addr) {
    int shown = 0;
    lp_head(b, "Lobbies");
    lp_banner(b, err);

    lb_raw(b, "<div class=\"card\"><h2>Create a lobby</h2>"
              "<form method=\"post\" action=\"/create\">"
              "<p>Your name <input type=\"text\" name=\"name\" value=\"\"></p>"
              "<p>Lobby title <input type=\"text\" name=\"title\" value=\"\"></p>"
              "<p><input type=\"submit\" value=\"Create lobby\"></p>"
              "</form></div>");

    lb_raw(b, "<div class=\"card\"><h2>Open lobbies</h2>");
    for (int i = 0; i < LOBBY_MAX_ROOMS; i++) {
        LobbyRoom *r = &L->rooms[i];
        if (!r->used || r->started) continue;
        shown++;
        lb_raw(b, "<p><a href=\"/join?room=");
        lb_esc(b, r->code);
        lb_raw(b, "\">");
        lb_esc(b, r->title);
        lb_raw(b, "</a> <span class=\"dim\">code ");
        lb_esc(b, r->code);
        lb_raw(b, " &middot; ");
        lb_int(b, lobby_count_players(r));
        lb_raw(b, "/");
        lb_int(b, LOBBY_MAX_PLAYERS);
        lb_raw(b, " players &middot; host ");
        lb_esc(b, r->players[r->leader].name);
        lb_raw(b, "</span></p>");
    }
    if (!shown) lb_raw(b, "<p class=\"dim\">No open lobbies yet. Make the first one.</p>");
    lb_raw(b, "<p class=\"foot\"><a href=\"/\">Refresh</a></p></div>");

    if (game_addr && game_addr[0]) {
        lb_raw(b, "<p class=\"dim\">Game server: ");
        lb_esc(b, game_addr);
        lb_raw(b, "</p>");
    }
    lp_foot(b);
}

/* ---- join: ask for a name ---- */
static inline void lobby_page_join(LobbyBuf *b, Lobbies *L, const char *code, LobbyErr err) {
    LobbyRoom *r = lobby_find_room(L, code);
    lp_head(b, "Join");
    lp_banner(b, err);
    if (!r) {
        lp_banner(b, LOBBY_ERR_NO_ROOM);
        lb_raw(b, "<p><a href=\"/\">Back to the lobby list</a></p>");
        lp_foot(b);
        return;
    }
    lb_raw(b, "<div class=\"card\"><h2>Join ");
    lb_esc(b, r->title);
    lb_raw(b, "</h2><p class=\"dim\">host ");
    lb_esc(b, r->players[r->leader].name);
    lb_raw(b, " &middot; ");
    lb_int(b, lobby_count_players(r));
    lb_raw(b, "/");
    lb_int(b, LOBBY_MAX_PLAYERS);
    lb_raw(b, " players</p><form method=\"post\" action=\"/join\">"
              "<p>Your name <input type=\"text\" name=\"name\" value=\"\"></p>"
              "<input type=\"hidden\" name=\"room\" value=\"");
    lb_esc(b, r->code);
    lb_raw(b, "\"><p><input type=\"submit\" value=\"Join lobby\"></p></form></div>"
              "<p><a href=\"/\">Back to the lobby list</a></p>");
    lp_foot(b);
}

/* ---- the lobby itself: players, ready, chat, start ---- */
static inline void lobby_page_room(LobbyBuf *b, Lobbies *L, const char *token, long now,
                                   LobbyErr err, const char *game_addr) {
    LobbyRoom *r; int me;
    if (!lobby_find_player(L, token, now, &r, &me)) {
        lp_head(b, "Lobby");
        lp_banner(b, LOBBY_ERR_BAD_TOKEN);
        lb_raw(b, "<p><a href=\"/\">Back to the lobby list</a></p>");
        lp_foot(b);
        return;
    }
    int am_leader = (r->leader == me);
    LobbyErr can = lobby_can_start(r);

    lp_head(b, r->title);
    lp_banner(b, err);

    lb_raw(b, "<div class=\"card\"><h2>");
    lb_esc(b, r->title);
    lb_raw(b, "</h2><p class=\"dim\">Lobby code</p><h2 class=\"code\">");
    lb_esc(b, r->code);
    lb_raw(b, "</h2><p class=\"dim\">Friends join from the lobby list or at /join?room=");
    lb_esc(b, r->code);
    lb_raw(b, "</p></div>");

    if (r->started) {
        lb_raw(b, "<div class=\"go\"><h3>Match started</h3>");
        if (game_addr && game_addr[0]) {
            lb_raw(b, "<p>Open BRAWLPIT and connect to <span class=\"amber\">");
            lb_esc(b, game_addr);
            lb_raw(b, "</span></p>");
        } else {
            lb_raw(b, "<p>The host started the match. Open BRAWLPIT and queue.</p>");
        }
        lb_raw(b, "</div>");
    }

    lb_raw(b, "<div class=\"cols\">");

    /* players column */
    lb_raw(b, "<div class=\"card\"><h3>Players ");
    lb_int(b, lobby_count_players(r));
    lb_raw(b, "/");
    lb_int(b, LOBBY_MAX_PLAYERS);
    lb_raw(b, "</h3>");
    for (int p = 0; p < LOBBY_MAX_PLAYERS; p++) {
        LobbyPlayer *pl = &r->players[p];
        if (!pl->used) continue;
        lb_raw(b, "<p>");
        if (p == r->leader) lb_raw(b, "<span class=\"amber\">HOST</span> ");
        else if (pl->ready) lb_raw(b, "<span class=\"ok\">READY</span> ");
        else lb_raw(b, "<span class=\"dim\">waiting</span> ");
        lb_esc(b, pl->name);
        if (p == me) lb_raw(b, " <span class=\"dim\">(you)</span>");
        lb_raw(b, "</p>");
    }

    if (!r->started) {
        if (!am_leader) {
            lb_raw(b, "<form method=\"post\" action=\"/ready\"><input type=\"hidden\" name=\"t\" value=\"");
            lb_esc(b, token);
            lb_raw(b, "\"><p><input type=\"submit\" value=\"");
            lb_raw(b, r->players[me].ready ? "Not ready" : "Ready");
            lb_raw(b, "\"></p></form>");
        } else {
            lb_raw(b, "<form method=\"post\" action=\"/start\"><input type=\"hidden\" name=\"t\" value=\"");
            lb_esc(b, token);
            lb_raw(b, "\"><p><input type=\"submit\" value=\"Start game\"></p></form>");
            if (can == LOBBY_OK)
                lb_raw(b, "<p class=\"ok\">Everyone is ready.</p>");
            else {
                lb_raw(b, "<p class=\"warn\">");
                lb_esc(b, lobby_err_text(can));
                lb_raw(b, "</p>");
            }
        }
    }
    lb_raw(b, "</div>");

    /* chat column */
    lb_raw(b, "<div class=\"card\"><h3>Chat</h3>");
    {
        int first = r->chat_next_seq - LOBBY_CHAT_LINES;
        if (first < 0) first = 0;
        for (int s = first; s < r->chat_next_seq; s++) {
            LobbyLine *ln = &r->chat[s % LOBBY_CHAT_LINES];
            lb_raw(b, "<p>");
            if (ln->who[0]) {
                lb_raw(b, "<span class=\"amber\">");
                lb_esc(b, ln->who);
                lb_raw(b, "</span> ");
                lb_esc(b, ln->text);
            } else {
                lb_raw(b, "<span class=\"dim\">* ");
                lb_esc(b, ln->text);
                lb_raw(b, "</span>");
            }
            lb_raw(b, "</p>");
        }
    }
    lb_raw(b, "<form method=\"post\" action=\"/say\"><p><input type=\"text\" name=\"say\" value=\"\"> "
              "<input type=\"submit\" value=\"Send\"></p><input type=\"hidden\" name=\"t\" value=\"");
    lb_esc(b, token);
    lb_raw(b, "\">");
    lb_raw(b, "</form>");
    lb_raw(b, "</div></div>");

    lb_raw(b, "<p class=\"foot\"><a href=\"/l?t=");
    lb_esc(b, token);
    lb_raw(b, "\">Refresh</a> &middot; <a href=\"/leave?t=");
    lb_esc(b, token);
    lb_raw(b, "\">Leave lobby</a></p>");
    lp_foot(b);
}

#endif

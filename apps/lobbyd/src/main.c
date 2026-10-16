/* brawlpit_lobbyd -- the BRAWLPIT lobby service ("servers" you create, chat in, ready up in).
 *
 * A tiny single-threaded HTTP/1.0 server whose pages are written for the PELLEGRINO browser
 * (HTML + CSS only: no JS, no cookies). All room logic is packages/common/lobby_rooms.h, all markup is
 * packages/common/lobby_pages.h. Fixed arrays only, no malloc. State is in memory.
 *
 *   brawlpit_lobbyd [--port 8765] [--game host:port]
 *   pellegrino http://127.0.0.1:8765/
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../../packages/common/lobby_rooms.h"
#include "../../../packages/common/lobby_pages.h"

#define REQ_MAX  4096
#define PAGE_MAX 32768

static Lobbies g_lobbies;
static char    g_page[PAGE_MAX];
static char    g_game_addr[64] = "";

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Find key in an application/x-www-form-urlencoded string; decode into out. 1 if present. */
static int form_get(const char *qs, const char *key, char *out, int cap) {
    size_t kl = strlen(key);
    const char *p = qs;
    out[0] = 0;
    while (p && *p) {
        const char *amp = strchr(p, '&');
        size_t seglen = amp ? (size_t)(amp - p) : strlen(p);
        if (seglen > kl && strncmp(p, key, kl) == 0 && p[kl] == '=') {
            const char *v = p + kl + 1, *end = p + seglen;
            int n = 0;
            while (v < end && n < cap - 1) {
                if (*v == '+') { out[n++] = ' '; v++; }
                else if (*v == '%' && v + 2 < end + 0 + 1 && hexval(v[1]) >= 0 && hexval(v[2]) >= 0) {
                    out[n++] = (char)(hexval(v[1]) * 16 + hexval(v[2]));
                    v += 3;
                } else out[n++] = *v++;
            }
            out[n] = 0;
            return 1;
        }
        p = amp ? amp + 1 : 0;
    }
    return 0;
}

static void send_all(int fd, const char *s, int n) {
    while (n > 0) {
        ssize_t w = send(fd, s, (size_t)n, MSG_NOSIGNAL);
        if (w <= 0) return;
        s += w; n -= (int)w;
    }
}

static void send_page(int fd, int code, const char *status, const char *body, int len) {
    char h[256];
    int n = snprintf(h, sizeof h,
        "HTTP/1.0 %d %s\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", code, status, len);
    send_all(fd, h, n);
    send_all(fd, body, len);
}

static void send_redirect(int fd, const char *loc) {
    char h[320];
    int n = snprintf(h, sizeof h,
        "HTTP/1.0 303 See Other\r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", loc);
    send_all(fd, h, n);
}

static void redirect_room(int fd, const char *token, LobbyErr e) {
    char loc[96];
    if (e == LOBBY_OK) snprintf(loc, sizeof loc, "/l?t=%s", token);
    else snprintf(loc, sizeof loc, "/l?t=%s&e=%d", token, (int)e);
    send_redirect(fd, loc);
}

static void render(int fd, int kind, const char *a, const char *b2, long now, LobbyErr err) {
    LobbyBuf lb = { g_page, PAGE_MAX, 0, 0 };
    g_page[0] = 0;
    if (kind == 0) lobby_page_home(&lb, &g_lobbies, err, g_game_addr);
    else if (kind == 1) lobby_page_join(&lb, &g_lobbies, a, err);
    else lobby_page_room(&lb, &g_lobbies, a, now, err, g_game_addr);
    (void)b2;
    send_page(fd, lb.overflow ? 500 : 200, lb.overflow ? "Too Big" : "OK", g_page, lb.len);
}

static void handle(int fd) {
    char req[REQ_MAX + 1];
    int have = 0, hdr_end = -1, body_len = 0;
    time_t t0 = time(0);
    while (have < REQ_MAX && time(0) - t0 < 2) {
        fd_set rf; struct timeval tv = { 0, 300000 };
        FD_ZERO(&rf); FD_SET(fd, &rf);
        if (select(fd + 1, &rf, 0, 0, &tv) <= 0) continue;
        ssize_t n = recv(fd, req + have, (size_t)(REQ_MAX - have), 0);
        if (n <= 0) break;
        have += (int)n;
        req[have] = 0;
        if (hdr_end < 0) {
            char *e = strstr(req, "\r\n\r\n");
            if (e) {
                hdr_end = (int)(e - req) + 4;
                const char *cl = strstr(req, "Content-Length:");
                if (!cl) cl = strstr(req, "content-length:");
                if (cl) body_len = atoi(cl + 15);
                if (body_len < 0 || body_len > 1500) body_len = 0;
            }
        }
        if (hdr_end >= 0 && have >= hdr_end + body_len) break;
    }
    req[have] = 0;
    if (hdr_end < 0) { send_page(fd, 400, "Bad Request", "bad request", 11); return; }

    char method[8] = "", target[512] = "";
    if (sscanf(req, "%7s %511s", method, target) != 2) {
        send_page(fd, 400, "Bad Request", "bad request", 11); return;
    }
    char *query = strchr(target, '?');
    if (query) *query++ = 0; else query = target + strlen(target);
    const char *body = req + hdr_end;
    long now = (long)time(0);
    lobby_reap(&g_lobbies, now);

    char t[LOBBY_TOKEN_LEN + 8], a[96], c[96], tok[LOBBY_TOKEN_LEN], code[LOBBY_CODE_LEN];
    int is_post = strcmp(method, "POST") == 0;
    const char *form = is_post ? body : query;
    LobbyErr e;

    if (!strcmp(target, "/") || !strcmp(target, "/index.html")) {
        char es[8]; int ev = form_get(query, "e", es, sizeof es) ? atoi(es) : 0;
        render(fd, 0, 0, 0, now, (LobbyErr)ev);
    } else if (!strcmp(target, "/health")) {
        send_page(fd, 200, "OK", "ok\n", 3);
    } else if (!strcmp(target, "/create") && is_post) {
        form_get(form, "name", a, sizeof a);
        form_get(form, "title", c, sizeof c);
        e = lobby_create(&g_lobbies, a, c, now, tok, code);
        if (e != LOBBY_OK) render(fd, 0, 0, 0, now, e);
        else redirect_room(fd, tok, LOBBY_OK);
    } else if (!strcmp(target, "/join")) {
        if (is_post) {
            form_get(form, "room", c, sizeof c);
            form_get(form, "name", a, sizeof a);
            e = lobby_join(&g_lobbies, c, a, now, tok);
            if (e != LOBBY_OK) render(fd, 1, c, 0, now, e);
            else redirect_room(fd, tok, LOBBY_OK);
        } else {
            form_get(query, "room", c, sizeof c);
            render(fd, 1, c, 0, now, LOBBY_OK);
        }
    } else if (!strcmp(target, "/l")) {
        char es[8]; int ev = form_get(query, "e", es, sizeof es) ? atoi(es) : 0;
        form_get(query, "t", t, sizeof t);
        render(fd, 2, t, 0, now, (LobbyErr)ev);
    } else if (!strcmp(target, "/ready") && is_post) {
        form_get(form, "t", t, sizeof t);
        e = lobby_toggle_ready(&g_lobbies, t, now);
        redirect_room(fd, t, e);
    } else if (!strcmp(target, "/say") && is_post) {
        char msg[LOBBY_MSG_LEN + 8];
        form_get(form, "t", t, sizeof t);
        form_get(form, "say", msg, sizeof msg);
        e = lobby_chat(&g_lobbies, t, msg, now);
        redirect_room(fd, t, e == LOBBY_ERR_EMPTY_MSG ? LOBBY_OK : e);
    } else if (!strcmp(target, "/start") && is_post) {
        form_get(form, "t", t, sizeof t);
        e = lobby_start(&g_lobbies, t, now);
        redirect_room(fd, t, e);
    } else if (!strcmp(target, "/leave")) {
        form_get(form, "t", t, sizeof t);
        lobby_leave(&g_lobbies, t, now);
        send_redirect(fd, "/");
    } else {
        const char *nf = "<!doctype html><html><body><p>Not found. <a href=\"/\">Lobby list</a></p></body></html>";
        send_page(fd, 404, "Not Found", nf, (int)strlen(nf));
    }
}

int main(int argc, char **argv) {
    int port = 8765;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) {
            strncpy(g_game_addr, argv[++i], sizeof g_game_addr - 1);
        }
    }
    signal(SIGPIPE, SIG_IGN);

    uint64_t seed = (uint64_t)time(0) * 6364136223846793005ULL ^ (uint64_t)getpid();
    FILE *ur = fopen("/dev/urandom", "rb");
    if (ur) { uint64_t r; if (fread(&r, sizeof r, 1, ur) == 1) seed ^= r; fclose(ur); }
    lobby_init(&g_lobbies, seed);

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons((uint16_t)port);
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) < 0) { perror("lobbyd: bind"); return 1; }
    if (listen(ls, 16) < 0) { perror("lobbyd: listen"); return 1; }
    fprintf(stderr, "brawlpit_lobbyd: listening on :%d (game=%s)\n", port, g_game_addr[0] ? g_game_addr : "-");

    for (;;) {
        int fd = accept(ls, 0, 0);
        if (fd < 0) { if (errno == EINTR) continue; break; }
        handle(fd);
        close(fd);
    }
    return 0;
}

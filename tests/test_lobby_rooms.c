/* tests/test_lobby_rooms.c -- lobby state machine + page rendering. Build: see scripts/build.sh */
#include <stdio.h>
#include <string.h>
#include "../packages/common/lobby_rooms.h"
#include "../packages/common/lobby_pages.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static Lobbies L;
static char page[32768];

int main(void) {
    char ta[LOBBY_TOKEN_LEN], tb[LOBBY_TOKEN_LEN], tc[LOBBY_TOKEN_LEN], code[LOBBY_CODE_LEN];
    lobby_init(&L, 12345);

    CHECK(lobby_create(&L, "  ", "x", 1, ta, code) == LOBBY_ERR_BAD_NAME);
    CHECK(lobby_create(&L, "Ann", "", 1, ta, code) == LOBBY_OK);
    CHECK(strlen(code) == 4 && strlen(ta) == 16);
    LobbyRoom *r = lobby_find_room(&L, code);
    CHECK(r && strcmp(r->title, "Ann's lobby") == 0);

    /* join, duplicate name, unknown room */
    CHECK(lobby_join(&L, code, "ann", 2, tb) == LOBBY_ERR_NAME_TAKEN);
    CHECK(lobby_join(&L, "ZZZZ", "Bob", 2, tb) == LOBBY_ERR_NO_ROOM);
    CHECK(lobby_join(&L, code, "Bob", 2, tb) == LOBBY_OK);

    /* start gating */
    CHECK(lobby_start(&L, tb, 3) == LOBBY_ERR_NOT_LEADER);
    CHECK(lobby_start(&L, ta, 3) == LOBBY_ERR_NOT_READY);
    CHECK(lobby_toggle_ready(&L, ta, 3) == LOBBY_ERR_IS_LEADER);
    CHECK(lobby_toggle_ready(&L, tb, 3) == LOBBY_OK);
    CHECK(lobby_toggle_ready(&L, tb, 4) == LOBBY_OK);   /* un-ready */
    CHECK(lobby_start(&L, ta, 4) == LOBBY_ERR_NOT_READY);
    CHECK(lobby_toggle_ready(&L, tb, 5) == LOBBY_OK);

    /* chat: cleaned, rate-limited, bad token */
    CHECK(lobby_chat(&L, tb, "hello \x01 world", 6) == LOBBY_OK);
    CHECK(lobby_chat(&L, tb, "again", 6) == LOBBY_ERR_TOO_FAST);
    CHECK(lobby_chat(&L, tb, "   ", 9) == LOBBY_ERR_EMPTY_MSG);
    CHECK(lobby_chat(&L, "nope", "x", 9) == LOBBY_ERR_BAD_TOKEN);
    CHECK(strcmp(r->chat[(r->chat_next_seq - 1) % LOBBY_CHAT_LINES].text, "hello  world") == 0);

    /* pages escape user text and stay in budget */
    CHECK(lobby_join(&L, code, "<b>x</b>", 6, tc) == LOBBY_OK);
    LobbyBuf lb = { page, sizeof page, 0, 0 };
    page[0] = 0;
    lobby_page_room(&lb, &L, ta, 7, LOBBY_OK, "game.example:6978");
    CHECK(!lb.overflow);
    CHECK(strstr(page, "&lt;b&gt;x&lt;/b&gt;") != 0);
    CHECK(strstr(page, "<b>x</b>") == 0);
    CHECK(strstr(page, "Start game") != 0);
    CHECK(strstr(page, "Everyone is ready") == 0);       /* tc is not ready yet */

    CHECK(lobby_toggle_ready(&L, tc, 8) == LOBBY_OK);
    CHECK(lobby_start(&L, ta, 9) == LOBBY_OK);
    CHECK(lobby_start(&L, ta, 9) == LOBBY_ERR_STARTED);
    CHECK(lobby_join(&L, code, "Late", 9, tb) == LOBBY_ERR_STARTED);
    lb.len = 0; page[0] = 0;
    lobby_page_room(&lb, &L, ta, 10, LOBBY_OK, "game.example:6978");
    CHECK(strstr(page, "Match started") != 0 && strstr(page, "game.example:6978") != 0);

    /* host leaves -> leadership passes, room survives; last leaves -> room gone */
    {
        char t1[LOBBY_TOKEN_LEN], t2[LOBBY_TOKEN_LEN], c2[LOBBY_CODE_LEN];
        CHECK(lobby_create(&L, "Hostie", "Room two", 20, t1, c2) == LOBBY_OK);
        CHECK(lobby_join(&L, c2, "Guest", 20, t2) == LOBBY_OK);
        LobbyRoom *r2 = lobby_find_room(&L, c2);
        CHECK(lobby_leave(&L, t1, 21) == LOBBY_OK);
        CHECK(r2->used && strcmp(r2->players[r2->leader].name, "Guest") == 0);
        CHECK(lobby_leave(&L, t2, 22) == LOBBY_OK);
        CHECK(lobby_find_room(&L, c2) == 0);
    }

    /* idle players are dropped; started rooms are reaped */
    {
        char t1[LOBBY_TOKEN_LEN], t2[LOBBY_TOKEN_LEN], c3[LOBBY_CODE_LEN];
        CHECK(lobby_create(&L, "Idle", "x", 100, t1, c3) == LOBBY_OK);
        CHECK(lobby_join(&L, c3, "Active", 100, t2) == LOBBY_OK);
        lobby_find_player(&L, t2, 100 + LOBBY_IDLE_DROP_S, 0, 0);   /* Active keeps seen fresh */
        lobby_reap(&L, 100 + LOBBY_IDLE_DROP_S + 5);
        LobbyRoom *r3 = lobby_find_room(&L, c3);
        CHECK(r3 && lobby_count_players(r3) == 1 && strcmp(r3->players[r3->leader].name, "Active") == 0);
        lobby_reap(&L, 9 + LOBBY_ROOM_REAP_S + 1000);
        CHECK(lobby_find_room(&L, code) == 0);
    }

    /* the room table can fill up */
    {
        Lobbies F; int ok = 0; char t[LOBBY_TOKEN_LEN], c[LOBBY_CODE_LEN];
        lobby_init(&F, 99);
        for (int i = 0; i < LOBBY_MAX_ROOMS + 3; i++)
            if (lobby_create(&F, "P", "T", 1, t, c) == LOBBY_OK) ok++;
        CHECK(ok == LOBBY_MAX_ROOMS);
    }

    printf(fails ? "test_lobby_rooms: %d FAILED\n" : "test_lobby_rooms: all passed\n", fails);
    return fails ? 1 : 0;
}

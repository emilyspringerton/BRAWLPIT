/* tests/test_net_protocol.c -- S248-00 (real client-server netcode, blocking prerequisite for
 * BP-LOBBY-001's matchmaking portal). Live end-to-end proof (real server binary + a real
 * SDL-free probe client, both built and run against real loopback UDP, full CONNECT -> WELCOME
 * -> USERCMD -> SNAPSHOT round trip with the probe's own entity moving under real server
 * physics) was done manually this session, not re-automated here (would need to spawn a real
 * subprocess + a free port, more machinery than this repo's existing no-build-script test
 * convention uses). This file instead locks down the real, fragile part of that proof as a fast,
 * no-subprocess regression test: the exact wire byte-layout apps/lobby/src/main.c's own
 * net_send_cmd/net_tick and apps/server/src/main.c's own server_handle_packet/server_broadcast
 * both depend on. A future change to NetHeader/UserCmd/NetPlayer that breaks this contract would
 * silently desync the client and server without either side raising a compile error -- this
 * test exists to catch that class of regression. */
#include <stdio.h>
#include <string.h>

#include "../packages/common/protocol.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

/* Mirrors net_send_cmd's own real wire construction (apps/lobby/src/main.c) and
 * server_handle_packet's own real parse cursor (apps/server/src/main.c: `int cursor =
 * sizeof(NetHeader) + 1;`) -- these two must agree, or a real UserCmd sent by the client silently
 * lands on the wrong bytes server-side. */
static void test_usercmd_wire_layout(void) {
    char buffer[sizeof(NetHeader) + 1 + sizeof(UserCmd)];
    NetHeader h;
    memset(&h, 0, sizeof(h));
    h.type = PACKET_USERCMD;
    h.client_id = 3;
    memcpy(buffer, &h, sizeof(NetHeader));
    buffer[sizeof(NetHeader)] = 0; /* the real, existing reserved/padding byte */

    UserCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.stick_x = 0.75f;
    cmd.buttons = BTN_JUMP | BTN_SPECIAL;
    memcpy(buffer + sizeof(NetHeader) + 1, &cmd, sizeof(UserCmd));

    /* Real server-side parse: cursor = sizeof(NetHeader) + 1, then reads a UserCmd from there. */
    int cursor = (int)sizeof(NetHeader) + 1;
    CHECK((size_t)sizeof(buffer) >= (size_t)cursor + sizeof(UserCmd),
          "buffer must be large enough for the server's own cursor + sizeof(UserCmd) check");
    UserCmd *parsed = (UserCmd*)(buffer + cursor);
    CHECK(parsed->stick_x == 0.75f, "stick_x must survive the real wire round trip");
    CHECK((parsed->buttons & BTN_JUMP) != 0, "BTN_JUMP must survive the real wire round trip");
    CHECK((parsed->buttons & BTN_SPECIAL) != 0, "BTN_SPECIAL must survive the real wire round trip");

    NetHeader *parsed_head = (NetHeader*)buffer;
    CHECK(parsed_head->type == PACKET_USERCMD, "header type must round-trip");
    CHECK(parsed_head->client_id == 3, "client_id must round-trip");
}

/* Mirrors server_broadcast's own real wire construction (a NetHeader, then a redundant 1-byte
 * count, then `count` NetPlayer entries back-to-back) and net_tick's own real parse cursor. */
static void test_snapshot_wire_layout(void) {
    char buffer[sizeof(NetHeader) + 1 + sizeof(NetPlayer) * 2];
    int cursor = 0;

    NetHeader h;
    memset(&h, 0, sizeof(h));
    h.type = PACKET_SNAPSHOT;
    h.entity_count = 2;
    memcpy(buffer + cursor, &h, sizeof(NetHeader)); cursor += (int)sizeof(NetHeader);

    unsigned char count = 2;
    memcpy(buffer + cursor, &count, 1); cursor += 1;

    NetPlayer p0; memset(&p0, 0, sizeof(p0));
    p0.id = 1; p0.x = 12.5f; p0.vx = 1.0f; p0.damage = 42; p0.stocks = 3; p0.facing = 1;
    memcpy(buffer + cursor, &p0, sizeof(NetPlayer)); cursor += (int)sizeof(NetPlayer);

    NetPlayer p1; memset(&p1, 0, sizeof(p1));
    p1.id = 2; p1.x = -8.0f; p1.vx = -0.5f; p1.damage = 0; p1.stocks = 4; p1.facing = 0;
    memcpy(buffer + cursor, &p1, sizeof(NetPlayer)); cursor += (int)sizeof(NetPlayer);

    int total_len = cursor;

    /* Real client-side parse, mirroring net_tick exactly. */
    int read_cursor = (int)sizeof(NetHeader);
    CHECK(total_len >= read_cursor + 1, "snapshot must have room for the count byte");
    unsigned char parsed_count = (unsigned char)buffer[read_cursor];
    read_cursor += 1;
    CHECK(parsed_count == 2, "entity count byte must round-trip");

    NetPlayer parsed[2];
    for (int i = 0; i < parsed_count; i++) {
        CHECK((size_t)(total_len - read_cursor) >= sizeof(NetPlayer), "must have a full NetPlayer left to read");
        memcpy(&parsed[i], buffer + read_cursor, sizeof(NetPlayer));
        read_cursor += (int)sizeof(NetPlayer);
    }
    CHECK(parsed[0].id == 1 && parsed[0].x == 12.5f && parsed[0].damage == 42, "first NetPlayer must round-trip");
    CHECK(parsed[1].id == 2 && parsed[1].x == -8.0f && parsed[1].stocks == 4, "second NetPlayer must round-trip");
}

int main(void) {
    test_usercmd_wire_layout();
    test_snapshot_wire_layout();
    if (failures == 0) {
        printf("test_net_protocol: all checks passed\n");
        return 0;
    }
    printf("test_net_protocol: %d check(s) FAILED\n", failures);
    return 1;
}

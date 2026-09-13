#!/usr/bin/env python3
"""
scripts/test_rl_env_packet.py (S419) -- real, offline unit tests for rl_env_packet.py's own wire
encode/decode, observation vector, and reward functions. No socket, no running server, no
gymnasium needed -- mirrors tests/test_net_protocol.c's own real C-side wire-layout proof, just
exercised from the Python side that actually has to agree with it byte-for-byte.

Run: python3 scripts/test_rl_env_packet.py
"""

import ctypes
import unittest

from rl_env_packet import (
    BTN_ATTACK,
    BTN_JUMP,
    NetHeader,
    NetPlayer,
    PACKET_CONNECT,
    PACKET_SNAPSHOT,
    PACKET_USERCMD,
    PACKET_WELCOME,
    UserCmd,
    build_observation,
    compute_reward,
    decode_snapshot,
    decode_welcome,
    encode_connect,
    encode_usercmd,
    find_self_and_opponent,
)


def make_player(id_, x=0.0, y=0.0, vx=0.0, vy=0.0, state=0, damage=0, stocks=4, shield=60, facing=1):
    return NetPlayer(id=id_, x=x, y=y, vx=vx, vy=vy, state=state, damage=damage,
                      stocks=stocks, shield=shield, facing=facing, jump_count=2, hit_stun=0)


class TestStructSizes(unittest.TestCase):
    """Locks down the exact real byte sizes this session verified live against the actual
    compiled C toolchain (a throwaway sizeof/offsetof probe) -- see this module's own assert
    statements at import time for the same check; this test just makes the expectation explicit
    and independently visible in test output."""

    def test_net_header_is_12_bytes(self):
        self.assertEqual(ctypes.sizeof(NetHeader), 12)

    def test_user_cmd_is_28_bytes(self):
        self.assertEqual(ctypes.sizeof(UserCmd), 28)

    def test_net_player_is_32_bytes(self):
        self.assertEqual(ctypes.sizeof(NetPlayer), 32)


class TestConnectHandshake(unittest.TestCase):
    def test_encode_connect_is_a_bare_header(self):
        data = encode_connect()
        self.assertEqual(len(data), ctypes.sizeof(NetHeader))
        h = NetHeader.from_buffer_copy(data)
        self.assertEqual(h.type, PACKET_CONNECT)

    def test_decode_welcome_extracts_client_id(self):
        h = NetHeader(type=PACKET_WELCOME, client_id=5, sequence=0, timestamp=0, entity_count=0)
        self.assertEqual(decode_welcome(bytes(h)), 5)

    def test_decode_welcome_rejects_wrong_packet_type(self):
        h = NetHeader(type=PACKET_SNAPSHOT, client_id=5, sequence=0, timestamp=0, entity_count=0)
        self.assertIsNone(decode_welcome(bytes(h)))

    def test_decode_welcome_rejects_short_buffer(self):
        self.assertIsNone(decode_welcome(b"\x00\x01"))


class TestUsercmdWireLayout(unittest.TestCase):
    """Mirrors tests/test_net_protocol.c's own test_usercmd_wire_layout exactly -- the same real
    server-side parse cursor (`sizeof(NetHeader) + 1`) applied here from the Python encoder's own
    output, proving the two sides agree without needing a live server."""

    def test_usercmd_round_trips_through_the_real_server_side_cursor(self):
        pkt = encode_usercmd(client_id=3, sequence=7, stick_x=0.75, stick_y=-0.25,
                              buttons=BTN_JUMP, timestamp_ms=1000)
        header = NetHeader.from_buffer_copy(pkt[: ctypes.sizeof(NetHeader)])
        self.assertEqual(header.type, PACKET_USERCMD)
        self.assertEqual(header.client_id, 3)

        cursor = ctypes.sizeof(NetHeader) + 1  # the real server_handle_packet cursor
        self.assertGreaterEqual(len(pkt), cursor + ctypes.sizeof(UserCmd))
        cmd = UserCmd.from_buffer_copy(pkt[cursor: cursor + ctypes.sizeof(UserCmd)])
        self.assertAlmostEqual(cmd.stick_x, 0.75, places=5)
        self.assertAlmostEqual(cmd.stick_y, -0.25, places=5)
        self.assertTrue(cmd.buttons & BTN_JUMP)
        self.assertFalse(cmd.buttons & BTN_ATTACK)


class TestSnapshotWireLayout(unittest.TestCase):
    """Mirrors tests/test_net_protocol.c's own test_snapshot_wire_layout: builds a real snapshot
    buffer BY HAND the exact way server_broadcast does (header, redundant count byte, N
    NetPlayers) and confirms decode_snapshot parses it back correctly -- the two sides of this
    contract, proven from the Python side."""

    def test_decodes_multiple_players_in_order(self):
        h = NetHeader(type=PACKET_SNAPSHOT, client_id=0, sequence=0, timestamp=1234, entity_count=2)
        p0 = make_player(1, x=12.5, vx=1.0, damage=42, stocks=3, facing=1)
        p1 = make_player(2, x=-8.0, vx=-0.5, damage=0, stocks=4, facing=0)
        buf = bytes(h) + bytes([2]) + bytes(p0) + bytes(p1)

        header, players = decode_snapshot(buf)
        self.assertEqual(header.type, PACKET_SNAPSHOT)
        self.assertEqual(len(players), 2)
        self.assertEqual(players[0].id, 1)
        self.assertAlmostEqual(players[0].x, 12.5, places=4)
        self.assertEqual(players[0].damage, 42)
        self.assertEqual(players[1].id, 2)
        self.assertAlmostEqual(players[1].x, -8.0, places=4)
        self.assertEqual(players[1].stocks, 4)

    def test_rejects_non_snapshot_packet(self):
        h = NetHeader(type=PACKET_WELCOME, client_id=1, sequence=0, timestamp=0, entity_count=0)
        header, players = decode_snapshot(bytes(h))
        self.assertIsNone(header)
        self.assertEqual(players, [])

    def test_truncated_packet_returns_only_the_complete_players(self):
        h = NetHeader(type=PACKET_SNAPSHOT, client_id=0, sequence=0, timestamp=0, entity_count=2)
        p0 = make_player(1)
        buf = bytes(h) + bytes([2]) + bytes(p0)  # claims 2 players, only ships 1
        header, players = decode_snapshot(buf)
        self.assertEqual(len(players), 1, "a truncated snapshot must degrade to what's actually there, not crash")


class TestFindSelfAndOpponent(unittest.TestCase):
    def test_splits_by_wire_id(self):
        players = [make_player(1), make_player(2)]
        own, opp = find_self_and_opponent(players, self_id=2)
        self.assertEqual(own.id, 2)
        self.assertEqual(opp.id, 1)

    def test_missing_self_returns_none_own(self):
        players = [make_player(2)]
        own, opp = find_self_and_opponent(players, self_id=1)
        self.assertIsNone(own)

    def test_missing_opponent_returns_none_opp(self):
        players = [make_player(1)]
        own, opp = find_self_and_opponent(players, self_id=1)
        self.assertIsNone(opp)


class TestBuildObservation(unittest.TestCase):
    def test_observation_has_the_fixed_documented_size(self):
        own = make_player(1, x=0.0)
        opp = make_player(2, x=-10.0)
        obs = build_observation(own, opp)
        from rl_env_packet import OBS_SIZE
        self.assertEqual(len(obs), OBS_SIZE)

    def test_observation_is_finite_at_stage_center(self):
        own = make_player(1, x=0.0, y=0.0)
        opp = make_player(2, x=0.0, y=0.0)
        obs = build_observation(own, opp)
        self.assertTrue(all(abs(v) < 10.0 for v in obs), f"expected a bounded observation, got {obs}")


class TestComputeReward(unittest.TestCase):
    def test_dealing_damage_is_positive(self):
        prev_own, prev_opp = make_player(1, damage=0), make_player(2, damage=0)
        cur_own, cur_opp = make_player(1, damage=0), make_player(2, damage=20)
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        self.assertGreater(r, 0.0)

    def test_taking_damage_is_negative(self):
        prev_own, prev_opp = make_player(1, damage=0), make_player(2, damage=0)
        cur_own, cur_opp = make_player(1, damage=20), make_player(2, damage=0)
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        self.assertLess(r, 0.0)

    def test_taking_a_stock_is_a_large_positive_reward(self):
        prev_own, prev_opp = make_player(1, stocks=4), make_player(2, stocks=4)
        cur_own, cur_opp = make_player(1, stocks=4), make_player(2, stocks=3)
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        self.assertGreaterEqual(r, 5.0)

    def test_losing_a_stock_is_a_large_negative_reward(self):
        prev_own, prev_opp = make_player(1, stocks=4), make_player(2, stocks=4)
        cur_own, cur_opp = make_player(1, stocks=3), make_player(2, stocks=4)
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        # REWARD_ALIVE_PER_TICK's own small positive per-tick bonus still applies here (a
        # fighter losing a stock isn't dead-dead, just down one) -- assert against the real
        # dominant stock-loss magnitude, not an exact -5.0 that ignores that other real term.
        self.assertLessEqual(r, -4.9)

    def test_winning_the_match_adds_the_terminal_bonus(self):
        prev_own, prev_opp = make_player(1, stocks=1), make_player(2, stocks=1)
        cur_own, cur_opp = make_player(1, stocks=1), make_player(2, stocks=0)
        r_not_done = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_done = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True)
        self.assertGreater(r_done, r_not_done)

    def test_losing_the_match_subtracts_the_terminal_penalty(self):
        prev_own, prev_opp = make_player(1, stocks=1), make_player(2, stocks=1)
        cur_own, cur_opp = make_player(1, stocks=0), make_player(2, stocks=1)
        r_not_done = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_done = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True)
        self.assertLess(r_done, r_not_done)


if __name__ == "__main__":
    unittest.main()

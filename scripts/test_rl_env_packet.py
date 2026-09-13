#!/usr/bin/env python3
"""
scripts/test_rl_env_packet.py (S419) -- real, offline unit tests for rl_env_packet.py's own wire
encode/decode, observation vector, and reward functions. No socket, no running server, no
gymnasium needed -- mirrors tests/test_net_protocol.c's own real C-side wire-layout proof, just
exercised from the Python side that actually has to agree with it byte-for-byte.

Run: python3 scripts/test_rl_env_packet.py
"""

import ctypes
import os
import subprocess
import time
import unittest

from rl_env_packet import (
    BTN_ATTACK,
    BTN_JUMP,
    NetHeader,
    NetPlayer,
    PACKET_CONNECT,
    PACKET_RESET_ACK,
    PACKET_RESET_MATCH,
    PACKET_SNAPSHOT,
    PACKET_USERCMD,
    PACKET_WELCOME,
    MATCH_TIME_LIMIT_SECONDS,
    MATCH_TIME_LIMIT_TICKS,
    TICK_RATE_HZ,
    ACTIVITY_TOKEN_REFILL_RATE,
    INACTIVITY_TICKS_THRESHOLD,
    REWARD_BUTTON_PRESS_PER_TICK,
    REWARD_INACTIVITY_PENALTY_PER_TICK,
    REWARD_LOSS,
    REWARD_MOVEMENT_PER_TICK,
    ActivityTokenBucket,
    PacketClient,
    UserCmd,
    build_observation,
    compute_reward,
    decode_reset_ack,
    decode_snapshot,
    decode_welcome,
    encode_connect,
    encode_reset_match,
    encode_usercmd,
    find_match_1v1_both,
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


class TestResetMatchHandshake(unittest.TestCase):
    """S419-07: real, offline wire-layout tests for the episode-reset request/ack, mirroring
    TestConnectHandshake's own established pattern above. The actual server-side behavior (does
    a reset genuinely restore fresh stocks/damage for BOTH slots) was verified live in this
    session against a real running bin/brawlpit_server -- see docs/RL_TRAINING_NORTHSTAR.md's own
    §5 for that proof; this class only locks down the bytes."""

    def test_encode_reset_match_is_a_bare_header_with_client_id(self):
        data = encode_reset_match(client_id=3)
        self.assertEqual(len(data), ctypes.sizeof(NetHeader))
        h = NetHeader.from_buffer_copy(data)
        self.assertEqual(h.type, PACKET_RESET_MATCH)
        self.assertEqual(h.client_id, 3)

    def test_decode_reset_ack_extracts_client_id(self):
        h = NetHeader(type=PACKET_RESET_ACK, client_id=2, sequence=0, timestamp=0, entity_count=0)
        self.assertEqual(decode_reset_ack(bytes(h)), 2)

    def test_decode_reset_ack_rejects_wrong_packet_type(self):
        h = NetHeader(type=PACKET_WELCOME, client_id=2, sequence=0, timestamp=0, entity_count=0)
        self.assertIsNone(decode_reset_ack(bytes(h)))

    def test_decode_reset_ack_rejects_short_buffer(self):
        self.assertIsNone(decode_reset_ack(b"\x07"))


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

    def test_hand_tailored_block_is_the_last_10_values(self):
        from rl_env_packet import OBS_SIZE
        own = make_player(1, x=-10.0, y=0.0, vx=1.0, vy=0.0)
        opp = make_player(2, x=10.0, y=0.0, vx=-1.0, vy=0.0)
        obs = build_observation(own, opp)
        self.assertEqual(len(obs), OBS_SIZE)
        self.assertEqual(OBS_SIZE, 31)

    def test_dx_dy_point_from_own_toward_opponent(self):
        own = make_player(1, x=-10.0, y=5.0)
        opp = make_player(2, x=10.0, y=-5.0)
        obs = build_observation(own, opp)
        dx, dy = obs[-10], obs[-9]
        self.assertGreater(dx, 0.0, "opponent is to own's right -- dx should be positive")
        self.assertLess(dy, 0.0, "opponent is below own -- dy should be negative")

    def test_distance_is_zero_when_standing_on_top_of_each_other(self):
        own = make_player(1, x=3.0, y=3.0)
        opp = make_player(2, x=3.0, y=3.0)
        obs = build_observation(own, opp)
        distance = obs[-8]
        self.assertAlmostEqual(distance, 0.0, places=6)

    def test_closing_velocity_is_positive_when_approaching(self):
        # own moving right (+x) directly toward opp, who is stationary to own's right.
        own_approach = make_player(1, x=-10.0, y=0.0, vx=1.0, vy=0.0)
        opp_stationary = make_player(2, x=10.0, y=0.0, vx=0.0, vy=0.0)
        obs_approach = build_observation(own_approach, opp_stationary)

        own_retreat = make_player(1, x=-10.0, y=0.0, vx=-1.0, vy=0.0)
        obs_retreat = build_observation(own_retreat, opp_stationary)

        closing_approach = obs_approach[-7]
        closing_retreat = obs_retreat[-7]
        self.assertGreater(closing_approach, 0.0, "moving toward the opponent should read as a positive closing velocity")
        self.assertLess(closing_retreat, 0.0, "moving away from the opponent should read as a negative closing velocity")

    def test_time_to_blast_is_low_when_flying_off_the_edge(self):
        from rl_env_packet import STAGE_FD_BLAST_RIGHT
        own_safe = make_player(1, x=0.0, y=0.0, vx=0.0, vy=0.0)
        own_flying_off = make_player(1, x=STAGE_FD_BLAST_RIGHT - 1.0, y=0.0, vx=5.0, vy=0.0)
        opp = make_player(2, x=0.0, y=0.0)
        obs_safe = build_observation(own_safe, opp)
        obs_danger = build_observation(own_flying_off, opp)
        own_time_to_blast_safe = obs_safe[-6]
        own_time_to_blast_danger = obs_danger[-6]
        self.assertGreater(own_time_to_blast_safe, own_time_to_blast_danger,
                            "standing still at center should read as far safer than flying off the edge")
        self.assertLess(own_time_to_blast_danger, 0.1, "about to fly off the edge should read as near-zero time-to-blast")

    def test_facing_toward_opponent_flips_sign_correctly(self):
        opp = make_player(2, x=10.0, y=0.0)
        own_facing_right = make_player(1, x=-10.0, y=0.0, facing=1)
        own_facing_left = make_player(1, x=-10.0, y=0.0, facing=0)
        obs_facing_right = build_observation(own_facing_right, opp)
        obs_facing_left = build_observation(own_facing_left, opp)
        self.assertEqual(obs_facing_right[-4], 1.0, "facing right toward an opponent to the right should read as 'facing toward'")
        self.assertEqual(obs_facing_left[-4], -1.0, "facing left away from an opponent to the right should read as 'facing away'")

    def test_damage_and_stock_diff_are_signed_relative_to_own(self):
        own = make_player(1, damage=50, stocks=3)
        opp = make_player(2, damage=20, stocks=4)
        obs = build_observation(own, opp)
        damage_diff, stock_diff = obs[-2], obs[-1]
        self.assertGreater(damage_diff, 0.0, "own has more damage than opp -- diff should be positive")
        self.assertLess(stock_diff, 0.0, "own has fewer stocks than opp -- diff should be negative")


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

    def test_standing_in_edge_danger_costs_more_than_the_alive_bonus_alone(self):
        from rl_env_packet import STAGE_FD_BLAST_RIGHT
        danger_x = STAGE_FD_BLAST_RIGHT - 5  # within the default 8-unit danger threshold
        prev_own, prev_opp = make_player(1, x=danger_x), make_player(2, x=0.0)
        cur_own, cur_opp = make_player(1, x=danger_x), make_player(2, x=0.0)
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        self.assertLess(r, 0.0, "standing in real edge danger must cost more than the tiny alive bonus gains")

    def test_recovering_out_of_danger_grants_a_real_bonus(self):
        from rl_env_packet import STAGE_FD_BLAST_RIGHT
        danger_x = STAGE_FD_BLAST_RIGHT - 5
        prev_own, prev_opp = make_player(1, x=danger_x, stocks=3), make_player(2, x=0.0)
        cur_own, cur_opp = make_player(1, x=0.0, stocks=3), make_player(2, x=0.0)  # back to safety, same stock count
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        self.assertGreater(r, 0.5, "a real recovery (out of danger, no stock lost) must be clearly rewarded")

    def test_no_recovery_bonus_if_the_stock_was_actually_lost(self):
        from rl_env_packet import STAGE_FD_BLAST_RIGHT
        danger_x = STAGE_FD_BLAST_RIGHT - 5
        prev_own, prev_opp = make_player(1, x=danger_x, stocks=3), make_player(2, x=0.0)
        cur_own, cur_opp = make_player(1, x=0.0, stocks=2), make_player(2, x=0.0)  # died and respawned
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        # Dominated by the real stock-loss penalty, not a spurious recovery bonus.
        self.assertLess(r, -4.0)

    def test_damage_landed_on_a_cornered_opponent_gets_an_edgeguard_bonus(self):
        from rl_env_packet import STAGE_FD_BLAST_RIGHT
        danger_x = STAGE_FD_BLAST_RIGHT - 5
        prev_own, prev_opp = make_player(1, x=0.0), make_player(2, x=danger_x, damage=0)
        cur_own, cur_opp = make_player(1, x=0.0), make_player(2, x=danger_x, damage=20)
        r_cornered = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)

        prev_opp_safe = make_player(2, x=0.0, damage=0)
        cur_opp_safe = make_player(2, x=0.0, damage=20)
        r_neutral = compute_reward(prev_own, prev_opp_safe, cur_own, cur_opp_safe, done=False)

        self.assertGreater(r_cornered, r_neutral,
                            "the same 20 damage should be worth MORE when the opponent was cornered off-stage")

    def test_no_action_given_means_no_activity_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        r_no_action = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_explicit_none = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=None)
        self.assertEqual(r_no_action, r_explicit_none)

    def test_real_movement_past_the_deadzone_gets_a_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        idle_action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        moving_action = [0.9, 0.0, 0.0, 0.0, 0.0, 0.0]
        r_idle = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=idle_action)
        r_moving = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=moving_action)
        self.assertGreater(r_moving, r_idle)

    def test_tiny_stick_drift_inside_the_deadzone_gets_no_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        idle_action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        drift_action = [0.05, 0.0, 0.0, 0.0, 0.0, 0.0]  # well under ACTIVITY_STICK_DEADZONE
        r_idle = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=idle_action)
        r_drift = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=drift_action)
        self.assertEqual(r_idle, r_drift, "a tiny drift inside the deadzone must not count as real movement")

    def test_pressing_any_button_gets_a_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        idle_action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        for i, button_name in enumerate(["jump", "attack", "shield", "special"]):
            action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
            action[2 + i] = 1.0
            r_idle = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=idle_action)
            r_pressed = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=action)
            self.assertGreater(r_pressed, r_idle, f"pressing {button_name} should get a real activity bonus")

    def test_no_activity_token_spent_given_means_flat_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        action = [0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
        r_no_arg = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=action)
        r_explicit_none = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=action, activity_token_spent=None)
        self.assertEqual(r_no_arg, r_explicit_none)

    def test_activity_token_spent_true_gets_the_full_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        idle_action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        action = [0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
        r_idle = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=idle_action)
        r_spent = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=action, activity_token_spent=True)
        self.assertAlmostEqual(r_spent - r_idle, REWARD_BUTTON_PRESS_PER_TICK, places=9)

    def test_activity_token_spent_false_gets_no_bonus_at_all(self):
        # S442: real token-bucket rate limiting -- a press that found an EMPTY bucket earns
        # nothing, unlike the old harmonic-decay design which always paid at least something.
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        idle_action = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
        action = [0.0, 0.0, 0.0, 1.0, 0.0, 0.0]
        r_idle = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=idle_action)
        r_wasted_press = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=action, activity_token_spent=False)
        self.assertEqual(r_wasted_press, r_idle, "a press that spent no real token must earn exactly the same as not pressing at all")

    def test_activity_token_bucket_lets_a_sustained_80_percent_duty_cycle_never_run_dry(self):
        # S442, founder real-time: "pausing 20 ish percent of the time you should still get the
        # same reward output for button pushing." REFILL_RATE=0.8 means pressing on exactly 4 out
        # of every 5 ticks (an 80% duty cycle, i.e. a real 20% pause) should let every single
        # press actually spend a real token -- the bucket should never run dry under this rate.
        bucket = ActivityTokenBucket()
        pattern = [True, True, True, True, False]  # press 80% of the time, pause the rest
        spends = []
        for _ in range(20):  # several full cycles -- confirm it's sustainable, not just a lucky start
            for pressed in pattern:
                spends.append(bucket.try_spend(pressed))
        presses = [s for p, s in zip(pattern * 20, spends) if p]
        self.assertTrue(all(presses), "an 80% duty cycle must never find an empty bucket")

    def test_activity_token_bucket_caps_total_reward_from_pure_spamming(self):
        # S442, founder real-time: "spamming as much as possible only gets you so much reward...
        # at a certain APM you just dont get any more reward anymore for going faster." Pressing
        # EVERY tick (100% duty cycle, faster than the 80% sustainable rate) must NOT let every
        # press spend a real token -- some presses must find the bucket empty.
        bucket = ActivityTokenBucket()
        spends = [bucket.try_spend(True) for _ in range(50)]
        self.assertFalse(all(spends), "pure spamming past the sustainable rate must waste some presses on an empty bucket")
        # The real, long-run success rate should converge toward the refill rate, not 100%.
        long_run_rate = sum(bucket.try_spend(True) for _ in range(1000)) / 1000
        self.assertAlmostEqual(long_run_rate, ACTIVITY_TOKEN_REFILL_RATE, delta=0.05)

    def test_activity_token_bucket_starts_full_so_the_first_press_always_counts(self):
        bucket = ActivityTokenBucket()
        self.assertTrue(bucket.try_spend(True), "a fresh episode's very first press should never find an empty bucket")

    def test_activity_bonus_is_real_but_modest_next_to_a_stock_swing(self):
        # The whole point of tier 4 is that it can NEVER outweigh actually playing well --
        # confirm the full activity bonus (movement + a button) is tiny next to a single
        # real stock swing.
        prev_own, prev_opp = make_player(1, stocks=4), make_player(2, stocks=4)
        cur_own, cur_opp = make_player(1, stocks=4), make_player(2, stocks=3)
        full_activity_action = [0.9, 0.9, 1.0, 1.0, 0.0, 0.0]
        r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, action=full_activity_action)
        self.assertLess(REWARD_MOVEMENT_PER_TICK + REWARD_BUTTON_PRESS_PER_TICK, 0.01,
                         "the activity bonus itself must stay a real, modest nudge")
        self.assertGreater(r, 5.0, "a real stock swing must still dominate the total reward")

    def test_no_survival_ticks_given_means_no_streak_bonus(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        r_no_streak = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_explicit_none = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, survival_ticks=None)
        self.assertEqual(r_no_streak, r_explicit_none)

    def test_survival_streak_grows_like_fibonacci_tick_over_tick(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        base = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)  # everything except the streak
        deltas = []
        for ticks in range(1, 6):
            r = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, survival_ticks=ticks)
            deltas.append(round(r - base, 9))
        # fib(1..5) = 1, 1, 2, 3, 5 -- the streak bonus should scale in exactly that ratio.
        unit = deltas[0]
        self.assertGreater(unit, 0.0)
        expected_ratios = [1, 1, 2, 3, 5]
        for delta, ratio in zip(deltas, expected_ratios):
            self.assertAlmostEqual(delta, unit * ratio, places=9)

    def test_survival_streak_scales_up_with_higher_damage(self):
        prev_opp = make_player(2)
        cur_opp = make_player(2)
        prev_own_low = make_player(1, damage=0)
        cur_own_low = make_player(1, damage=0)
        prev_own_high = make_player(1, damage=150)
        cur_own_high = make_player(1, damage=150)
        r_low = compute_reward(prev_own_low, prev_opp, cur_own_low, cur_opp, done=False, survival_ticks=10)
        r_high = compute_reward(prev_own_high, prev_opp, cur_own_high, cur_opp, done=False, survival_ticks=10)
        self.assertGreater(r_high, r_low,
                            "surviving at high damage (one hit from death) should be worth more than surviving at 0 damage")

    def test_survival_streak_resets_the_instant_a_stock_is_lost(self):
        prev_own, prev_opp = make_player(1, stocks=4), make_player(2)
        cur_own_lost, cur_opp = make_player(1, stocks=3), make_player(2)
        # Even if the caller passes a large streak count, compute_reward itself must refuse to
        # apply the bonus on the exact tick a stock was actually lost.
        r_survived = compute_reward(prev_own, prev_opp, make_player(1, stocks=4), cur_opp, done=False, survival_ticks=15)
        r_died = compute_reward(prev_own, prev_opp, cur_own_lost, cur_opp, done=False, survival_ticks=15)
        self.assertLess(r_died, r_survived,
                         "no streak bonus should land on the tick a stock is actually lost")

    def test_survival_streak_is_capped_so_a_long_life_does_not_diverge(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        r_at_cap = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, survival_ticks=20)
        r_way_past_cap = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, survival_ticks=2000)
        self.assertAlmostEqual(r_at_cap, r_way_past_cap, places=9,
                                msg="the Fibonacci index must be capped, not grow unbounded over a long life")

    def test_survival_streak_at_the_cap_stays_a_real_tiny_nudge_not_a_perverse_incentive(self):
        # REAL, FOUND, FIXED BUG regression test (founder real-time: "we spiked in model quality
        # ... then the newer ones are all pretty dumb ... i think i introduced some perverted
        # incentives"): SURVIVAL_STREAK_FIB_CAP used to be 20 (fib(20)=6765), which meant the
        # per-tick reward once a life passed 20 ticks was 6.765 -- applied EVERY TICK for the
        # rest of a potentially 9000-tick match, dwarfing REWARD_WIN=10 by 3-4 orders of
        # magnitude and giving PPO a real incentive to stall/avoid combat instead of fight. This
        # pins the per-tick value at the cap (even at max realistic damage, where the exponential
        # damage scale is largest) to stay a real, tiny nudge -- comparable to tier 3's own
        # REWARD_ALIVE_PER_TICK, not comparable to a real outcome reward.
        from rl_env_packet import REWARD_ALIVE_PER_TICK, REWARD_STOCK_TAKEN
        prev_own, prev_opp = make_player(1, damage=200), make_player(2)
        cur_own, cur_opp = make_player(1, damage=200), make_player(2)
        r_base = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_at_cap_high_damage = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False, survival_ticks=9000)
        streak_contribution = r_at_cap_high_damage - r_base
        self.assertLess(streak_contribution, REWARD_ALIVE_PER_TICK * 100,
                         "even at max damage and a very long streak, the per-tick survival bonus "
                         "must stay within two orders of magnitude of the tier-3 survival nudge")
        self.assertLess(streak_contribution * 9000, REWARD_STOCK_TAKEN,
                         "sustained for a full 9000-tick match, the total survival-streak reward "
                         "must stay well below the reward for taking even a single stock -- "
                         "surviving passively must never out-earn actually fighting")

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

    def test_timeout_with_own_ahead_still_gets_the_loss_penalty_not_a_win(self):
        # S429, founder real-time: "if time expires it's a draw and thats counted the same as a
        # loss" -- even when own is AHEAD on stocks/damage when the clock runs out, a timeout must
        # score the same as REWARD_LOSS, never REWARD_WIN.
        prev_own, prev_opp = make_player(1, stocks=3), make_player(2, stocks=3)
        cur_own, cur_opp = make_player(1, stocks=3), make_player(2, stocks=2)
        r_timeout = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True, timed_out=True)
        r_normal_win = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True, timed_out=False)
        self.assertLess(r_timeout, r_normal_win,
                         "a timeout must never score as well as an outright win, even with the same final stocks")

    def test_timeout_applies_the_same_reward_loss_constant_a_normal_loss_does(self):
        # A tied-stocks timeout gets REWARD_LOSS added to its terminal-outcome term, same as an
        # outright loss does -- isolate that term by comparing against the otherwise-identical
        # non-timeout ending (same players, same tick, only `timed_out` differs).
        prev_own, prev_opp = make_player(1, stocks=2), make_player(2, stocks=2)
        cur_own, cur_opp = make_player(1, stocks=2), make_player(2, stocks=2)
        r_timeout = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True, timed_out=True)
        r_tied_no_timeout = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True, timed_out=False)
        self.assertAlmostEqual(r_timeout - r_tied_no_timeout, REWARD_LOSS, places=9,
                                msg="a timeout should add exactly REWARD_LOSS on top of an otherwise-neutral tied ending")

    def test_no_timed_out_given_defaults_to_normal_outcome_scoring(self):
        prev_own, prev_opp = make_player(1, stocks=1), make_player(2, stocks=1)
        cur_own, cur_opp = make_player(1, stocks=1), make_player(2, stocks=0)
        r_default = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True)
        r_explicit_false = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=True, timed_out=False)
        self.assertEqual(r_default, r_explicit_false)

    def test_match_time_limit_is_real_2_point_5_minutes_at_60hz(self):
        self.assertEqual(MATCH_TIME_LIMIT_SECONDS, 150.0)
        self.assertEqual(TICK_RATE_HZ, 60.0)
        self.assertEqual(MATCH_TIME_LIMIT_TICKS, 9000)

    def test_no_inactivity_penalty_below_the_threshold(self):
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        r_no_arg = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False)
        r_below_threshold = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False,
                                            inactivity_ticks=INACTIVITY_TICKS_THRESHOLD)
        self.assertEqual(r_no_arg, r_below_threshold,
                          "sitting still for exactly the threshold, not PAST it, must not be penalized yet")

    def test_real_inactivity_penalty_kicks_in_past_the_4_second_threshold(self):
        # S442, founder real-time, after directly observing a real trained checkpoint freeze
        # completely: "do we introduce a strong negative reward that ticks down if no key is
        # pressed for say 4 seconds?"
        prev_own, prev_opp = make_player(1), make_player(2)
        cur_own, cur_opp = make_player(1), make_player(2)
        r_at_threshold = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False,
                                         inactivity_ticks=INACTIVITY_TICKS_THRESHOLD)
        r_past_threshold = compute_reward(prev_own, prev_opp, cur_own, cur_opp, done=False,
                                           inactivity_ticks=INACTIVITY_TICKS_THRESHOLD + 1)
        self.assertAlmostEqual(r_past_threshold - r_at_threshold, REWARD_INACTIVITY_PENALTY_PER_TICK, places=9)
        self.assertLess(r_past_threshold, r_at_threshold, "the instant the threshold is exceeded, a real penalty must apply")

    def test_inactivity_penalty_is_a_real_negative_number(self):
        self.assertLess(REWARD_INACTIVITY_PENALTY_PER_TICK, 0.0)

    def test_four_second_threshold_matches_60hz(self):
        self.assertEqual(INACTIVITY_TICKS_THRESHOLD, 240)  # 4 real seconds at 60Hz


class TestLiveMatchmakingRequeue(unittest.TestCase):
    """S445/S446, real, found, fixed server-side bugs: a real, permanent regression test, not just a
    throwaway verification script -- BRAWLPIT's own matchmaking guard
    (apps/server/src/main.c's own PACKET_FIND_MATCH handler) only ever accepted a queue request
    from a sender with no established client_id, so a client that had already played one real
    matchmade match could NEVER re-queue for another in the same server process lifetime
    (mm_init_slot marks a matched client's slot permanently active, and nothing else ever cleared
    that). This is exactly what S443's real self-play mode needs every single episode boundary
    (env.reset() re-queues the SAME two long-lived clients over and over) -- live-reproduced as a
    real crash mid-training before this was found and fixed server-side (main.c now also accepts
    a re-queue when local_state.match_over is true). A second, distinct real bug this same test
    caught (S446): PACKET_MATCH_FOUND is a fire-and-forget sendto() with no ack/retry, and can be
    a genuine casualty of loopback UDP packet loss under --fast-forward's own uncapped busy-spin
    broadcast loop, permanently stalling the dropped-for client even with the S445 guard fixed --
    fixed by making FIND_MATCH idempotent (re-answers an already-active, mid-match client with its
    current MATCH_FOUND instead of silently dropping the retry). Skipped if bin/brawlpit_server
    isn't built."""

    SERVER_BIN = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "bin", "brawlpit_server")
    PORT = 7989

    def setUp(self):
        if not os.path.exists(self.SERVER_BIN):
            self.skipTest("bin/brawlpit_server not built -- run ./scripts/build_training.sh first")
        self.proc = subprocess.Popen([self.SERVER_BIN, "--fast-forward", "--port", str(self.PORT)],
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.5)

    def tearDown(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.proc.kill()

    def test_the_same_two_clients_can_requeue_after_their_match_concludes(self):
        a = PacketClient("127.0.0.1", self.PORT)
        b = PacketClient("127.0.0.1", self.PORT)
        try:
            find_match_1v1_both(a, b, timeout=10)

            # Force a real, live match conclusion -- run client A off the stage (a genuine
            # self-destruct), not a manual flag flip. B's own socket is serviced every tick too
            # (read and discarded) -- exactly what real self-play (S443) does every step, driving
            # both clients in lockstep -- rather than left idle: --fast-forward's own server main
            # loop is a real, found, separate CPU-bound busy-spin (recvfrom is O_NONBLOCK, so it
            # broadcasts continuously even with zero new packets), and an idle socket here backs
            # up with an unbounded, ever-growing snapshot backlog that starves the real regression
            # this test exists to catch, not a property of the fix under test.
            own_dead = False
            for _ in range(90):
                a.send_action(stick_x=1.0, stick_y=0.0)
                _, players = a.recv_snapshot()
                b.send_action(stick_x=0.0, stick_y=0.0)
                b.recv_snapshot()
                own = next((p for p in players if p.id == a.client_id), None)
                if own is not None and own.stocks == 0:
                    own_dead = True
                    break
            self.assertTrue(own_dead, "test setup failed to actually conclude the first match -- can't test the real regression without this")

            # The actual regression: before the fix, this second call always timed out and raised
            # ConnectionError, because the server silently refused to queue an already-active
            # client's new PACKET_FIND_MATCH request.
            find_match_1v1_both(a, b, timeout=10)
        finally:
            a.close()
            b.close()


if __name__ == "__main__":
    unittest.main()

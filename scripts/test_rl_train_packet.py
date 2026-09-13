#!/usr/bin/env python3
"""
scripts/test_rl_train_packet.py (S431) -- real, offline unit tests for rl_train_packet.py's own
pure registry-lookup logic (_find_latest_registry_checkpoint's is_disabled filtering,
_is_checkpoint_disabled's live-pause check). No network, no running server needed --
list_checkpoints is mocked, matching test_rl_bot_pool.py's own established convention.

Run: python3 scripts/test_rl_train_packet.py
"""

import os
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rl_league import LeagueRole  # noqa: E402
from rl_train_packet import (  # noqa: E402
    ROLE_BASE_PORTS,
    _check_role_server_alive,
    _find_latest_registry_checkpoint,
    _is_checkpoint_disabled,
    _pick_opponent_checkpoint,
    make_vec_env,
)


class FakeProc:
    """A minimal stand-in for subprocess.Popen -- _check_role_server_alive only ever calls
    .poll(), so a real subprocess isn't needed for this test."""

    def __init__(self, exit_code=None):
        self._exit_code = exit_code

    def poll(self):
        return self._exit_code


class TestCheckRoleServerAlive(unittest.TestCase):
    def test_does_nothing_when_all_servers_are_still_running(self):
        # poll() returns None for a still-running process -- must not raise.
        _check_role_server_alive(LeagueRole.MAIN, [FakeProc(exit_code=None), FakeProc(exit_code=None)])

    def test_raises_loudly_when_the_server_has_died(self):
        # S432: the real fix for "stuck no idea whats going on" -- a dead server must be caught
        # immediately, not silently retried for hours.
        with self.assertRaises(RuntimeError) as ctx:
            _check_role_server_alive(LeagueRole.MAIN, [FakeProc(exit_code=1)])
        self.assertIn("main", str(ctx.exception))
        self.assertIn("died", str(ctx.exception))

    def test_raises_when_any_one_of_several_parallel_servers_has_died(self):
        # S440: with --num-envs > 1, a role has MULTIPLE servers -- one dying must still be
        # caught even if the others are fine.
        with self.assertRaises(RuntimeError) as ctx:
            _check_role_server_alive(LeagueRole.MAIN, [FakeProc(exit_code=None), FakeProc(exit_code=1), FakeProc(exit_code=None)])
        self.assertIn("env 1", str(ctx.exception))


class TestFindLatestRegistryCheckpoint(unittest.TestCase):
    def test_picks_highest_generation_for_the_role(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [
                {"id": 1, "generation": 0, "role": "main"},
                {"id": 2, "generation": 3, "role": "main"},
                {"id": 3, "generation": 1, "role": "main"},
            ]
            latest = _find_latest_registry_checkpoint("http://unused.invalid", "main")
            self.assertEqual(latest["id"], 2)

    def test_skips_disabled_checkpoints_even_if_newest(self):
        # S428/S431: a disabled checkpoint must never be picked as a resume target, even when
        # it's the real newest generation for its role.
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [
                {"id": 1, "generation": 0, "role": "main", "is_disabled": False},
                {"id": 2, "generation": 3, "role": "main", "is_disabled": True},
                {"id": 3, "generation": 1, "role": "main", "is_disabled": False},
            ]
            latest = _find_latest_registry_checkpoint("http://unused.invalid", "main")
            self.assertEqual(latest["id"], 3, "the newest NON-disabled checkpoint should win")

    def test_returns_none_when_everything_for_the_role_is_disabled(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [
                {"id": 1, "generation": 0, "role": "main", "is_disabled": True},
                {"id": 2, "generation": 1, "role": "main", "is_disabled": True},
            ]
            latest = _find_latest_registry_checkpoint("http://unused.invalid", "main")
            self.assertIsNone(latest)

    def test_returns_none_when_the_role_has_no_checkpoints_at_all(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = []
            self.assertIsNone(_find_latest_registry_checkpoint("http://unused.invalid", "main"))


class TestIsCheckpointDisabled(unittest.TestCase):
    def test_true_when_the_specific_checkpoint_is_disabled(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [
                {"id": 5, "role": "main", "is_disabled": True},
                {"id": 6, "role": "main", "is_disabled": False},
            ]
            self.assertTrue(_is_checkpoint_disabled("http://unused.invalid", "main", 5))

    def test_false_when_the_specific_checkpoint_is_enabled(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [
                {"id": 5, "role": "main", "is_disabled": True},
                {"id": 6, "role": "main", "is_disabled": False},
            ]
            self.assertFalse(_is_checkpoint_disabled("http://unused.invalid", "main", 6))

    def test_fails_open_false_if_the_checkpoint_id_is_gone_from_the_registry(self):
        with patch("rl_train_packet.list_checkpoints") as mock_list:
            mock_list.return_value = [{"id": 6, "role": "main", "is_disabled": False}]
            self.assertFalse(_is_checkpoint_disabled("http://unused.invalid", "main", 999))

    def test_fails_open_false_on_a_registry_error_rather_than_stall_training(self):
        with patch("rl_train_packet.list_checkpoints", side_effect=RuntimeError("registry down")):
            self.assertFalse(_is_checkpoint_disabled("http://unused.invalid", "main", 5))


class TestPickOpponentCheckpoint(unittest.TestCase):
    """S444, founder real-time, correcting S443's initial 'everyone self-plays their own past
    self' design: 'the main agent job is to train against itself and the league... the main
    exploiter is only job is to find main's weakness and the league exploiter whouse job it is
    to find strategies that work well against the league.'"""

    def _prev_paths(self):
        return {
            LeagueRole.MAIN: "/tmp/main.zip",
            LeagueRole.MAIN_EXPLOITER: "/tmp/main_exploiter.zip",
            LeagueRole.LEAGUE_EXPLOITER: "/tmp/league_exploiter.zip",
        }

    def test_main_self_plays_against_its_own_past_self(self):
        prev = self._prev_paths()
        self.assertEqual(_pick_opponent_checkpoint(LeagueRole.MAIN, prev), prev[LeagueRole.MAIN])

    def test_main_exploiter_always_fights_mains_own_checkpoint_not_its_own_past_self(self):
        prev = self._prev_paths()
        self.assertEqual(_pick_opponent_checkpoint(LeagueRole.MAIN_EXPLOITER, prev), prev[LeagueRole.MAIN])

    def test_league_exploiter_fights_a_real_sample_of_the_league(self):
        prev = self._prev_paths()
        picked = _pick_opponent_checkpoint(LeagueRole.LEAGUE_EXPLOITER, prev)
        self.assertIn(picked, prev.values(), "league exploiter must fight one of the real, current league members")

    def test_league_exploiter_pick_is_a_real_distribution_not_hardcoded_to_one_role(self):
        # Real, live check that random.choice is actually exercising all 3 candidates over many
        # draws, not silently always returning the same one.
        prev = self._prev_paths()
        picks = {_pick_opponent_checkpoint(LeagueRole.LEAGUE_EXPLOITER, prev) for _ in range(200)}
        self.assertEqual(picks, set(prev.values()), "200 draws should see every real league member picked at least once")

    def test_main_exploiter_gets_none_when_main_has_no_checkpoint_yet(self):
        self.assertIsNone(_pick_opponent_checkpoint(LeagueRole.MAIN_EXPLOITER, {}))

    def test_league_exploiter_gets_none_when_the_league_is_completely_empty(self):
        self.assertIsNone(_pick_opponent_checkpoint(LeagueRole.LEAGUE_EXPLOITER, {}))

    def test_main_gets_none_on_its_own_first_generation(self):
        self.assertIsNone(_pick_opponent_checkpoint(LeagueRole.MAIN, {}))


class TestRolePorts(unittest.TestCase):
    def test_role_base_ports_never_collide_even_at_a_large_num_envs(self):
        # S440: each role's own reserved block must be wide enough that a real --num-envs run
        # for one role can never step on another role's own ports.
        bases = sorted(ROLE_BASE_PORTS.values())
        for a, b in zip(bases, bases[1:]):
            self.assertGreaterEqual(b - a, 100, "each role needs a real, wide reserved port block")


class TestMakeVecEnv(unittest.TestCase):
    def test_a_single_port_returns_the_plain_env_not_a_vec_env(self):
        # S440: --num-envs 1 (the default) must stay byte-for-byte equivalent to this pipeline's
        # own pre-S440 behavior -- no subprocess/IPC overhead for zero real parallelism benefit.
        from rl_env_packet import BrawlpitPacketEnv
        env = make_vec_env("127.0.0.1", [7978])
        try:
            self.assertIsInstance(env, BrawlpitPacketEnv)
        finally:
            env.close()

    def test_multiple_ports_returns_a_real_subprocess_vec_env(self):
        from stable_baselines3.common.vec_env import SubprocVecEnv
        env = make_vec_env("127.0.0.1", [7978, 7979, 7980])
        try:
            self.assertIsInstance(env, SubprocVecEnv)
            self.assertEqual(env.num_envs, 3)
        finally:
            env.close()


if __name__ == "__main__":
    unittest.main()

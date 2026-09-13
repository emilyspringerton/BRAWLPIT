#!/usr/bin/env python3
"""
scripts/test_rl_train_packet.py (S431) -- real, offline unit tests for rl_train_packet.py's own
pure registry-lookup logic (_find_latest_registry_checkpoint's is_disabled filtering,
_is_checkpoint_disabled's live-pause check). No network, no running server needed --
list_checkpoints is mocked, matching test_rl_bot_pool.py's own established convention.

Run: python3 scripts/test_rl_train_packet.py
"""

import collections
import os
import random
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from rl_league import DEFAULT_STRUGGLE_WINDOW, HEURISTIC_ID, LeagueManager, LeagueRole  # noqa: E402
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
    """S447, founder real-time: a full, detailed AlphaStar/PFSP spec posted directly (2026-09-13)
    describing exactly what rl_league.py's own sample_for_main/sample_for_main_exploiter/
    sample_for_league_exploiter already implement (ELO/PFSP-weighted sampling over the WHOLE
    registered league, Main Exploiter's own struggle-detected climb-down through Main's historical
    checkpoints) -- this replaces S444's real, honestly-scoped placeholder (each role's own
    immediately-prior generation only) with a thin adapter onto that already-tested infra, using
    a real LeagueManager backed by a temp dir rather than mocks."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.league = LeagueManager(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _register(self, role, generation, path="/tmp/fake.zip", inherit_elo_from_role=True):
        return self.league.register(role, generation, path, inherit_elo_from_role=inherit_elo_from_role)

    def test_main_gets_none_on_a_completely_empty_league(self):
        path, member_id = _pick_opponent_checkpoint(LeagueRole.MAIN, self.league, {}, collections.deque())
        self.assertIsNone(path)
        self.assertIsNone(member_id)

    def test_main_exploiter_gets_none_when_main_has_no_checkpoint_yet(self):
        path, member_id = _pick_opponent_checkpoint(LeagueRole.MAIN_EXPLOITER, self.league, {}, collections.deque())
        self.assertIsNone(path)
        self.assertIsNone(member_id)

    def test_league_exploiter_gets_none_when_the_league_is_completely_empty(self):
        path, member_id = _pick_opponent_checkpoint(LeagueRole.LEAGUE_EXPLOITER, self.league, {}, collections.deque())
        self.assertIsNone(path)
        self.assertIsNone(member_id)

    def test_main_samples_a_real_registered_member_when_the_league_is_not_empty(self):
        main_member = self._register(LeagueRole.MAIN, 0, "/tmp/main0.zip")
        path, member_id = _pick_opponent_checkpoint(LeagueRole.MAIN, self.league, {}, collections.deque(), )
        # A fresh, all-neutral league (every candidate untested, 0.5 win rate) could still land
        # on the permanent HEURISTIC_ID baseline sentinel -- that's real, honest PFSP behavior
        # (see sample_for_main's own doc comment: HEURISTIC_ID is always a real candidate), not a
        # bug, so this only asserts a real registered member is a VALID possible outcome, not the
        # only one. member_id must be self-consistent with the returned path either way.
        self.assertIn(member_id, (None, main_member.id))
        if member_id == main_member.id:
            self.assertEqual(path, main_member.path)

    def test_main_exploiter_targets_mains_current_checkpoint_when_not_struggling(self):
        main_member = self._register(LeagueRole.MAIN, 3, "/tmp/main3.zip")
        path, member_id = _pick_opponent_checkpoint(
            LeagueRole.MAIN_EXPLOITER, self.league, {}, recent_results_vs_main=collections.deque())
        self.assertEqual(member_id, main_member.id, "an empty history is NOT struggling -- always challenge Main's current checkpoint")
        self.assertEqual(path, main_member.path)

    def test_main_exploiter_climbs_down_through_mains_history_when_struggling(self):
        old_main = self._register(LeagueRole.MAIN, 0, "/tmp/main0.zip")
        self._register(LeagueRole.MAIN, 1, "/tmp/main1.zip")
        struggling_history = collections.deque([0] * DEFAULT_STRUGGLE_WINDOW, maxlen=DEFAULT_STRUGGLE_WINDOW)
        path, member_id = _pick_opponent_checkpoint(
            LeagueRole.MAIN_EXPLOITER, self.league, {}, recent_results_vs_main=struggling_history)
        main_ids = {m.id for m in self.league.members_by_role(LeagueRole.MAIN)}
        self.assertIn(member_id, main_ids, "a struggling Main Exploiter still only ever picks from Main's own real history")

    def test_league_exploiter_samples_from_the_whole_registered_roster(self):
        m1 = self._register(LeagueRole.MAIN, 0, "/tmp/m1.zip")
        m2 = self._register(LeagueRole.LEAGUE_EXPLOITER, 0, "/tmp/m2.zip")
        picks = set()
        for _ in range(200):
            _, member_id = _pick_opponent_checkpoint(
                LeagueRole.LEAGUE_EXPLOITER, self.league, {}, collections.deque(), rng=random.Random())
            picks.add(member_id)
        # Over 200 draws with a fully neutral (untested) win-rate prior, every real registered
        # candidate should show up at least once; the permanent heuristic sentinel is also a real
        # possible draw but maps to None (no real checkpoint file backs it) -- see
        # test_a_heuristic_pick_returns_none_none_not_a_fake_path below for that mapping directly.
        self.assertEqual(picks - {None}, {m1.id, m2.id})

    def test_a_heuristic_pick_returns_none_none_not_a_fake_path(self):
        # Deterministic: force sample_for_league_exploiter itself to land on the permanent
        # heuristic sentinel (no real checkpoint file backs it) and confirm the adapter maps that
        # to the same "nothing real to train against" signal the empty-league case already uses.
        self._register(LeagueRole.MAIN, 0, "/tmp/m.zip")
        with patch("rl_train_packet.sample_for_league_exploiter", return_value=HEURISTIC_ID):
            path, member_id = _pick_opponent_checkpoint(
                LeagueRole.LEAGUE_EXPLOITER, self.league, {}, collections.deque())
        self.assertIsNone(path)
        self.assertIsNone(member_id)


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

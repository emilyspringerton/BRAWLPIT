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
from rl_train_packet import _check_role_server_alive, _find_latest_registry_checkpoint, _is_checkpoint_disabled  # noqa: E402


class FakeProc:
    """A minimal stand-in for subprocess.Popen -- _check_role_server_alive only ever calls
    .poll(), so a real subprocess isn't needed for this test."""

    def __init__(self, exit_code=None):
        self._exit_code = exit_code

    def poll(self):
        return self._exit_code


class TestCheckRoleServerAlive(unittest.TestCase):
    def test_does_nothing_when_the_server_is_still_running(self):
        # poll() returns None for a still-running process -- must not raise.
        _check_role_server_alive(LeagueRole.MAIN, FakeProc(exit_code=None))

    def test_raises_loudly_when_the_server_has_died(self):
        # S432: the real fix for "stuck no idea whats going on" -- a dead server must be caught
        # immediately, not silently retried for hours.
        with self.assertRaises(RuntimeError) as ctx:
            _check_role_server_alive(LeagueRole.MAIN, FakeProc(exit_code=1))
        self.assertIn("main", str(ctx.exception))
        self.assertIn("died", str(ctx.exception))


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


if __name__ == "__main__":
    unittest.main()

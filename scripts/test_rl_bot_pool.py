#!/usr/bin/env python3
"""
scripts/test_rl_bot_pool.py -- real, pure-logic tests for rl_bot_pool.py's BotMatchmaker
(S422 follow-up, founder real-time: "use matchmaking queues to manage load as the bot pool grows
really low elos get looong queue times because we dont really need data on a shit bot fighting
against anything really").

BotMatchmaker._refresh_elo hits the real network registry, so these tests stub it out and drive
`pick_pair`/`release_pair` directly -- exactly the same "test the real, pure logic; don't require
a live server for a unit test" discipline test_rl_env_packet.py's own reward tests already follow.
"""

import os
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# rl_bot_pool imports stable_baselines3 at PoolBot.__init__ time only, and rl_evaluate/rl_env_packet
# for pure constants/functions -- importing the module itself needs neither numpy nor SB3 to be
# installed, since PoolBot's real import is deferred to inside its own __init__.
from rl_bot_pool import BotMatchmaker  # noqa: E402


class FakeBot:
    """A minimal stand-in for PoolBot -- BotMatchmaker only ever touches `.id` and
    `.checkpoint["elo"]`, never `.model`, so a real PoolBot (which needs a real SB3 checkpoint
    file to construct) isn't needed here."""

    def __init__(self, bot_id, elo):
        self.id = bot_id
        self.checkpoint = {"id": bot_id, "elo": elo}

    @property
    def name(self):
        return f"#{self.id}"


class TestBotMatchmakerPairing(unittest.TestCase):
    def _matchmaker(self, bots):
        mm = BotMatchmaker(bots, registry_url="http://unused.invalid")
        # Real, deliberate test isolation: never let a unit test touch the network -- patch
        # _refresh_elo to a no-op so pick_pair uses exactly the Elo values the test set up.
        mm._refresh_elo = lambda: None
        return mm

    def test_picks_the_two_closest_elo_bots_first(self):
        bots = [FakeBot(1, 1500), FakeBot(2, 1510), FakeBot(3, 1900)]
        mm = self._matchmaker(bots)
        a, b = mm.pick_pair()
        self.assertEqual({a.id, b.id}, {1, 2}, "the closest-Elo pair should be picked over the outlier")

    def test_outlier_bot_only_gets_picked_once_no_closer_partner_is_free(self):
        bots = [FakeBot(1, 1500), FakeBot(2, 1510), FakeBot(3, 1900)]
        mm = self._matchmaker(bots)
        a, b = mm.pick_pair()  # consumes 1 and 2, the close pair
        self.assertEqual({a.id, b.id}, {1, 2})
        # only bot 3 (the outlier) is left free -- fewer than 2 free bots, no pair possible yet.
        self.assertIsNone(mm.pick_pair())
        mm.release_pair(a, b)
        # now that 1 and 2 are free again, bot 3 gets its closest-available partner (bot 2).
        a2, b2 = mm.pick_pair()
        self.assertIn(3, {a2.id, b2.id})

    def test_busy_bots_are_never_picked_twice_concurrently(self):
        bots = [FakeBot(i, 1500) for i in range(4)]
        mm = self._matchmaker(bots)
        first = mm.pick_pair()
        second = mm.pick_pair()
        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        first_ids = {first[0].id, first[1].id}
        second_ids = {second[0].id, second[1].id}
        self.assertEqual(len(first_ids & second_ids), 0, "no bot should be in two live matches at once")

    def test_release_pair_frees_both_bots_for_future_pairing(self):
        bots = [FakeBot(1, 1500), FakeBot(2, 1500)]
        mm = self._matchmaker(bots)
        a, b = mm.pick_pair()
        self.assertIsNone(mm.pick_pair(), "both bots are busy, no pair should be available")
        mm.release_pair(a, b)
        a2, b2 = mm.pick_pair()
        self.assertEqual({a2.id, b2.id}, {1, 2})

    def test_longest_waiting_free_bot_is_prioritized_for_fairness(self):
        # Real fairness guarantee: a bot that has been idle longest gets first pick of a partner,
        # so a bot near the pool's Elo center never gets to hog matches forever while a
        # less-central bot starves.
        bots = [FakeBot(1, 1500), FakeBot(2, 1502), FakeBot(3, 1498)]
        mm = self._matchmaker(bots)
        a1, b1 = mm.pick_pair()  # bots 1&2 or 1&3 (all close), whichever picked -- release both
        mm.release_pair(a1, b1)
        # manually mark bot 3 as having waited far longer than the others
        mm.last_played[1] = 100.0
        mm.last_played[2] = 100.0
        mm.last_played[3] = 1.0
        a2, b2 = mm.pick_pair()
        self.assertEqual(a2.id, 3, "the longest-waiting free bot should be selected first")

    def test_fewer_than_two_free_bots_returns_none(self):
        bots = [FakeBot(1, 1500)]
        mm = self._matchmaker(bots)
        self.assertIsNone(mm.pick_pair())

    def test_refresh_elo_updates_pairing_decisions(self):
        # A real, live registry read should change who gets paired -- verified here by patching
        # list_checkpoints (what _refresh_elo actually calls) rather than bypassing it.
        bots = [FakeBot(1, 1500), FakeBot(2, 1510), FakeBot(3, 1900)]
        mm = BotMatchmaker(bots, registry_url="http://unused.invalid")
        with patch("rl_bot_pool.list_checkpoints") as mock_list:
            # after a live Elo swing, bot 3 is now the closest match to bot 1
            mock_list.return_value = [
                {"id": 1, "elo": 1500},
                {"id": 2, "elo": 2200},
                {"id": 3, "elo": 1505},
            ]
            a, b = mm.pick_pair()
            self.assertEqual({a.id, b.id}, {1, 3})
            self.assertEqual(bots[1].checkpoint["elo"], 2200, "pick_pair should have refreshed the live Elo onto the bot")


if __name__ == "__main__":
    unittest.main()

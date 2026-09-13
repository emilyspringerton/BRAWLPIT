#!/usr/bin/env python3
"""
scripts/test_rl_registry.py (S420) -- real, offline unit tests for rl_registry.py's own pure
multipart encoding logic. No network, no running IDUNA server needed -- the real end-to-end HTTP
round trip (authenticate/push/list/pull) was verified live in this session against a real running
IDUNA instance; see BRAWLPIT/docs/RL_TRAINING_NORTHSTAR.md's own §9 for that proof.

Run: python3 scripts/test_rl_registry.py
"""

import unittest

from rl_registry import _multipart_body


class TestMultipartBody(unittest.TestCase):
    def test_body_contains_all_field_values(self):
        body, content_type = _multipart_body(
            {"role": "main", "generation": 3, "elo": 1550.5, "source_location": "colab"},
            "file", "main3.zip", b"fake checkpoint bytes",
        )
        text = body.decode(errors="replace")
        self.assertIn("main", text)
        self.assertIn("3", text)
        self.assertIn("1550.5", text)
        self.assertIn("colab", text)
        self.assertIn('name="role"', text)
        self.assertIn('name="file"; filename="main3.zip"', text)

    def test_body_contains_the_real_raw_file_bytes(self):
        payload = bytes(range(256))  # real, arbitrary binary content -- not just ASCII text
        body, _ = _multipart_body({"role": "main"}, "file", "weights.zip", payload)
        self.assertIn(payload, body)

    def test_content_type_carries_a_real_boundary_matching_the_body(self):
        body, content_type = _multipart_body({"role": "main"}, "file", "x.zip", b"data")
        self.assertTrue(content_type.startswith("multipart/form-data; boundary="))
        boundary = content_type.split("boundary=")[1]
        self.assertIn(boundary.encode(), body)

    def test_each_call_gets_a_fresh_boundary(self):
        _, ct1 = _multipart_body({"role": "main"}, "file", "x.zip", b"data")
        _, ct2 = _multipart_body({"role": "main"}, "file", "x.zip", b"data")
        self.assertNotEqual(ct1, ct2, "reusing the same boundary across calls risks a real collision with file content")


if __name__ == "__main__":
    unittest.main()

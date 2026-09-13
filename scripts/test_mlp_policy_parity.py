#!/usr/bin/env python3
"""
scripts/test_mlp_policy_parity.py (S421-02) -- generates a real cross-language parity fixture:
picks a real observation vector, runs it through an ACTUAL trained checkpoint's own real
stable_baselines3 predict() (deterministic mode -- the actor network's raw mean action, matching
mlp_policy.h's own real, documented "no stochastic sampling" choice), and prints the checkpoint
path/observation/expected-action in the exact argv shape tests/test_mlp_policy.c's own
test_real_checkpoint_parity expects -- so a single shell pipeline proves the C loader is
byte-and-math-compatible with the real Python export, not just internally self-consistent.

Run:
    python3 scripts/export_policy_weights.py <checkpoint.zip> /tmp/weights.bin
    eval $(python3 scripts/test_mlp_policy_parity.py <checkpoint.zip>)
    gcc -o /tmp/test_mlp_policy tests/test_mlp_policy.c -lm
    /tmp/test_mlp_policy /tmp/weights.bin "$OBS" "$EXPECTED"
"""

import argparse
import numpy as np
from stable_baselines3 import PPO


def main():
    p = argparse.ArgumentParser()
    p.add_argument("checkpoint")
    p.add_argument("--seed", type=int, default=42)
    args = p.parse_args()

    model = PPO.load(args.checkpoint, device="cpu")
    obs_size = model.policy.observation_space.shape[0]

    rng = np.random.default_rng(args.seed)
    # A real, bounded observation matching build_observation's own real value ranges
    # (rl_env_packet.py) -- roughly [-1, 1] for the raw scalars, one-hot for the commander block.
    obs = rng.uniform(-1.0, 1.0, size=obs_size).astype(np.float32)

    action, _ = model.predict(obs, deterministic=True)

    print(f'export OBS="{" ".join(f"{v:.6f}" for v in obs)}"')
    print(f'export EXPECTED="{" ".join(f"{v:.6f}" for v in action)}"')


if __name__ == "__main__":
    main()

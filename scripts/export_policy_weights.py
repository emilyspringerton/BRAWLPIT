#!/usr/bin/env python3
"""
scripts/export_policy_weights.py (S421-02) -- exports a trained stable_baselines3 PPO
checkpoint's real ACTOR (policy) network weights into a small, portable binary format BRAWLPIT's
native C client can load and run at real game speed with no Python/PyTorch dependency at
runtime. Same real "weights are data, architecture is code" split REDGARDEN's own established
scripts/export_rl_policy_to_c.py precedent uses -- adapted here as a runtime-loadable binary blob
rather than compiled-in C source, since the whole point is downloading a NEW checkpoint's weights
at game start without recompiling the game.

Real, checked (not assumed) network shape, confirmed live against an actual trained checkpoint
in this session:
    ActorCriticPolicy.mlp_extractor.policy_net: Linear(21,64) -> Tanh -> Linear(64,64) -> Tanh
    ActorCriticPolicy.action_net:               Linear(64,6)  (raw, linear output -- the real
                                                 action MEAN; BRAWLPIT's own native loader clips
                                                 to [-1,1] to match the real action space bounds,
                                                 matching model.predict(obs, deterministic=True)'s
                                                 own real behavior for a game-facing bot rather
                                                 than stochastic sampling).
21 = rl_env_packet.py's own real OBS_SIZE (8 raw scalars x2 players + 5 commander posture one-hot).
6 = [stick_x, stick_y, jump, attack, shield, special], matching BrawlpitPacketEnv's own action
space exactly.

Binary format (little-endian, versioned so a future architecture change fails loudly rather than
silently misloading):
    magic:      4 bytes, ASCII "BPMW" (BrawlPit MLP Weights)
    version:    u32 = 1
    num_layers: u32
    per layer:  in_dim u32, out_dim u32, activation u8 (0 = linear, 1 = tanh)
    then, per layer in order: W (out_dim * in_dim float32, ROW-MAJOR out x in -- matches
    PyTorch's own nn.Linear.weight layout exactly, no transpose needed) followed by b (out_dim
    float32).

Run: python3 scripts/export_policy_weights.py <checkpoint.zip> <output.bin>
"""

import argparse
import struct
import sys

MAGIC = b"BPMW"
VERSION = 1
ACTIVATION_LINEAR = 0
ACTIVATION_TANH = 1


def export_policy_weights(checkpoint_path, output_path):
    from stable_baselines3 import PPO  # imported lazily -- this script needs SB3, callers that
    # only need the binary FORMAT constants above (e.g. a test) shouldn't need it installed.

    model = PPO.load(checkpoint_path, device="cpu")
    policy_net = model.policy.mlp_extractor.policy_net
    action_net = model.policy.action_net

    # Real, direct extraction from the actual real layers found live (see this module's own doc
    # comment) -- not hardcoded shape assumptions, so a policy trained with a different net_arch
    # still exports correctly (only the ACTIVATION pattern below assumes the real, current
    # Sequential(Linear, Tanh, Linear, Tanh) shape SB3's own default net_arch produces).
    linear_layers = [m for m in policy_net if m.__class__.__name__ == "Linear"]
    linear_layers.append(action_net)

    layers = []
    for i, layer in enumerate(linear_layers):
        w = layer.weight.detach().cpu().numpy()  # shape (out, in) -- PyTorch's own real layout
        b = layer.bias.detach().cpu().numpy()  # shape (out,)
        activation = ACTIVATION_LINEAR if i == len(linear_layers) - 1 else ACTIVATION_TANH
        layers.append((w, b, activation))

    with open(output_path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<I", VERSION))
        f.write(struct.pack("<I", len(layers)))
        for w, b, activation in layers:
            out_dim, in_dim = w.shape
            f.write(struct.pack("<IIB", in_dim, out_dim, activation))
        for w, b, activation in layers:
            f.write(w.astype("<f4").tobytes())
            f.write(b.astype("<f4").tobytes())

    return len(layers)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("checkpoint", help="a real .zip saved by stable_baselines3's PPO.save()")
    p.add_argument("output", help="output .bin path")
    args = p.parse_args()

    n = export_policy_weights(args.checkpoint, args.output)
    print(f"exported {n} layers -> {args.output}")

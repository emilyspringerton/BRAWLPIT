/* tests/test_mlp_policy.c -- S421-02, real tests for the MLP policy loader/forward-pass against
 * the exact binary format scripts/export_policy_weights.py produces. Two real tiers:
 *  1. Synthetic, hand-computed layers (no file needed) -- exercises malformed-input rejection and
 *     a known-by-hand forward pass.
 *  2. A REAL exported checkpoint's weights (path passed as argv[1], optional) cross-checked
 *     against stable_baselines3's own real predict() output for the same observation -- the
 *     strongest real proof this loader is byte-and-math-compatible with the actual Python
 *     export, not just internally self-consistent. See scripts/test_mlp_policy_parity.py for the
 *     Python side that generates the reference output this test compares against.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../packages/common/mlp_policy.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
} while (0)

static void test_rejects_bad_magic(void) {
    unsigned char bad[16] = "NOTB";
    MlpPolicy p;
    CHECK(mlp_policy_load_from_memory(bad, sizeof(bad), &p) == 0, "bad magic must be rejected");
}

static void test_rejects_short_buffer(void) {
    unsigned char tiny[4] = "BPMW";
    MlpPolicy p;
    CHECK(mlp_policy_load_from_memory(tiny, sizeof(tiny), &p) == 0, "a too-short buffer must be rejected");
}

static void test_rejects_bad_version(void) {
    unsigned char buf[12];
    memcpy(buf, "BPMW", 4);
    unsigned int version = 999, layers = 1;
    memcpy(buf + 4, &version, 4);
    memcpy(buf + 8, &layers, 4);
    MlpPolicy p;
    CHECK(mlp_policy_load_from_memory(buf, sizeof(buf), &p) == 0, "an unrecognized version must be rejected");
}

/* A tiny, real, hand-built single-layer network: Linear(2,2) with a known weight matrix and
 * bias, tanh activation -- verifies the actual math, not just that loading succeeds. */
static void test_single_layer_forward_matches_hand_computation(void) {
    unsigned char buf[12 + 9 + (2 * 2 + 2) * 4];
    size_t cursor = 0;
    memcpy(buf + cursor, "BPMW", 4); cursor += 4;
    unsigned int version = 1, num_layers = 1;
    memcpy(buf + cursor, &version, 4); cursor += 4;
    memcpy(buf + cursor, &num_layers, 4); cursor += 4;
    unsigned int in_dim = 2, out_dim = 2;
    unsigned char activation = MLP_ACTIVATION_TANH;
    memcpy(buf + cursor, &in_dim, 4); cursor += 4;
    memcpy(buf + cursor, &out_dim, 4); cursor += 4;
    memcpy(buf + cursor, &activation, 1); cursor += 1;

    /* W = [[1, 0], [0, 1]] (identity), b = [0, 0] -- output should equal tanh(input). */
    float w[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float b[2] = {0.0f, 0.0f};
    memcpy(buf + cursor, w, sizeof(w)); cursor += sizeof(w);
    memcpy(buf + cursor, b, sizeof(b)); cursor += sizeof(b);

    MlpPolicy p;
    CHECK(mlp_policy_load_from_memory(buf, (long)cursor, &p) == 1, "a real, well-formed single-layer network must load");

    float obs[2] = {0.5f, -0.5f};
    float action[2];
    CHECK(mlp_policy_forward(&p, obs, 2, action, 2) == 1, "forward pass on a loaded network must succeed");
    CHECK(fabsf(action[0] - tanhf(0.5f)) < 1e-5f, "identity-weight tanh layer must match tanhf(input) exactly");
    CHECK(fabsf(action[1] - tanhf(-0.5f)) < 1e-5f, "identity-weight tanh layer must match tanhf(input) exactly (second unit)");

    mlp_policy_free(&p);
}

static void test_forward_rejects_wrong_obs_size(void) {
    unsigned char buf[12 + 9 + (2 * 2 + 2) * 4];
    size_t cursor = 0;
    memcpy(buf + cursor, "BPMW", 4); cursor += 4;
    unsigned int version = 1, num_layers = 1;
    memcpy(buf + cursor, &version, 4); cursor += 4;
    memcpy(buf + cursor, &num_layers, 4); cursor += 4;
    unsigned int in_dim = 2, out_dim = 2;
    unsigned char activation = MLP_ACTIVATION_LINEAR;
    memcpy(buf + cursor, &in_dim, 4); cursor += 4;
    memcpy(buf + cursor, &out_dim, 4); cursor += 4;
    memcpy(buf + cursor, &activation, 1); cursor += 1;
    float w[4] = {1, 0, 0, 1}, b[2] = {0, 0};
    memcpy(buf + cursor, w, sizeof(w)); cursor += sizeof(w);
    memcpy(buf + cursor, b, sizeof(b)); cursor += sizeof(b);

    MlpPolicy p;
    mlp_policy_load_from_memory(buf, (long)cursor, &p);
    float obs[3] = {1, 2, 3};
    float action[2];
    CHECK(mlp_policy_forward(&p, obs, 3, action, 2) == 0, "a mismatched obs_size must be rejected, not silently misread");
    mlp_policy_free(&p);
}

/* Real cross-language parity check against an ACTUAL exported checkpoint, when one is provided
 * (argv[1] = weights .bin, argv[2] = a real observation vector as space-separated floats,
 * argv[3] = the expected action as space-separated floats from stable_baselines3's own real
 * predict() -- see scripts/test_mlp_policy_parity.py). Skipped (not failed) if no path is given,
 * matching this repo's own "optional real-machine verification, not required for `go test`
 * everywhere" precedent for anything needing an external toolchain. */
static void test_real_checkpoint_parity(int argc, char **argv) {
    if (argc < 4) {
        printf("test_real_checkpoint_parity: skipped (no real checkpoint/obs/expected given)\n");
        return;
    }
    MlpPolicy p;
    CHECK(mlp_policy_load_from_file(argv[1], &p) == 1, "a real exported checkpoint file must load");
    if (!p.loaded) return;

    float obs[64];
    int obs_n = 0;
    char *tok = strtok(argv[2], " ");
    while (tok && obs_n < 64) { obs[obs_n++] = strtof(tok, NULL); tok = strtok(NULL, " "); }

    float expected[64];
    int exp_n = 0;
    tok = strtok(argv[3], " ");
    while (tok && exp_n < 64) { expected[exp_n++] = strtof(tok, NULL); tok = strtok(NULL, " "); }

    float action[64];
    CHECK(mlp_policy_forward(&p, obs, obs_n, action, 64) == 1, "forward pass on the real checkpoint must succeed");
    CHECK(exp_n == p.layers[p.num_layers - 1].out_dim, "expected-action length must match the real network's own output size");
    for (int i = 0; i < exp_n; i++) {
        if (fabsf(action[i] - expected[i]) > 1e-3f) {
            printf("FAIL: real checkpoint parity mismatch at index %d: C=%.6f python=%.6f\n", i, action[i], expected[i]);
            failures++;
        }
    }
    if (failures == 0) {
        printf("test_real_checkpoint_parity: C forward pass matches stable_baselines3's own real predict() output\n");
    }
    mlp_policy_free(&p);
}

int main(int argc, char **argv) {
    test_rejects_bad_magic();
    test_rejects_short_buffer();
    test_rejects_bad_version();
    test_single_layer_forward_matches_hand_computation();
    test_forward_rejects_wrong_obs_size();
    test_real_checkpoint_parity(argc, argv);

    if (failures == 0) {
        printf("test_mlp_policy: all checks passed\n");
        return 0;
    }
    printf("test_mlp_policy: %d check(s) FAILED\n", failures);
    return 1;
}

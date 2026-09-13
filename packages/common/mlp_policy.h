#ifndef BRAWLPIT_MLP_POLICY_H
#define BRAWLPIT_MLP_POLICY_H

/* mlp_policy.h — S421-02, founder real-time: "ensure that the client actually uses that model
 * (download it when the game starts...)". A real, generic, hand-written MLP forward-pass loader
 * + evaluator for the binary format scripts/export_policy_weights.py produces from a trained
 * stable_baselines3 PPO checkpoint's real actor network -- "weights are data, architecture is
 * code" (same real split REDGARDEN's own export_rl_policy_to_c.py precedent uses, adapted here
 * as a runtime-loadable blob instead of compiled-in C, since the whole point is loading a NEW
 * checkpoint's weights at game start without recompiling).
 *
 * Real, bounded, no dynamic architecture surprises: MLP_POLICY_MAX_LAYERS/MAX_UNITS below are
 * real, generous bounds matching the actual exported shape (3 layers, 64 units) with headroom,
 * not an unbounded parser that would let a malformed/hostile weights file drive an unbounded
 * allocation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define MLP_POLICY_MAX_LAYERS 8
#define MLP_POLICY_MAX_UNITS 256
#define MLP_ACTIVATION_LINEAR 0
#define MLP_ACTIVATION_TANH 1

typedef struct {
    int in_dim, out_dim;
    unsigned char activation;
    float *weights; /* out_dim * in_dim, row-major out x in -- matches PyTorch's own nn.Linear.weight layout */
    float *bias;    /* out_dim */
} MlpLayer;

typedef struct {
    int num_layers;
    MlpLayer layers[MLP_POLICY_MAX_LAYERS];
    int loaded;
} MlpPolicy;

/* mlp_policy_load_from_memory parses the real "BPMW" binary format (see
 * scripts/export_policy_weights.py's own doc comment for the exact byte layout) from an
 * in-memory buffer -- used by both a direct file load (mlp_policy_load_from_file below) and a
 * real network download (level_registry.h's own established fetch_url_to_buffer pattern).
 * Returns 1 on success, 0 on any real failure (bad magic/version, malformed layer dims, short
 * buffer) -- *out is left untouched on failure, matching this repo's own established loader
 * contract (level_load_from_file's own doc comment). */
static inline int mlp_policy_load_from_memory(const unsigned char *data, long size, MlpPolicy *out) {
    if (size < 12) return 0;
    if (memcmp(data, "BPMW", 4) != 0) return 0;
    uint32_t version, num_layers;
    memcpy(&version, data + 4, 4);
    memcpy(&num_layers, data + 8, 4);
    if (version != 1) return 0;
    if (num_layers == 0 || num_layers > MLP_POLICY_MAX_LAYERS) return 0;

    long cursor = 12;
    MlpPolicy p;
    memset(&p, 0, sizeof(p));
    p.num_layers = (int)num_layers;

    /* Header table: in_dim(u32) out_dim(u32) activation(u8) per layer. */
    for (uint32_t i = 0; i < num_layers; i++) {
        if (cursor + 9 > size) return 0;
        uint32_t in_dim, out_dim;
        unsigned char activation;
        memcpy(&in_dim, data + cursor, 4);
        memcpy(&out_dim, data + cursor + 4, 4);
        memcpy(&activation, data + cursor + 8, 1);
        cursor += 9;
        if (in_dim == 0 || in_dim > MLP_POLICY_MAX_UNITS || out_dim == 0 || out_dim > MLP_POLICY_MAX_UNITS) return 0;
        if (activation != MLP_ACTIVATION_LINEAR && activation != MLP_ACTIVATION_TANH) return 0;
        p.layers[i].in_dim = (int)in_dim;
        p.layers[i].out_dim = (int)out_dim;
        p.layers[i].activation = activation;
    }

    /* Payload: W then b per layer, in the same order as the header table. */
    for (int i = 0; i < p.num_layers; i++) {
        long w_bytes = (long)p.layers[i].out_dim * p.layers[i].in_dim * (long)sizeof(float);
        long b_bytes = (long)p.layers[i].out_dim * (long)sizeof(float);
        if (cursor + w_bytes + b_bytes > size) return 0;

        p.layers[i].weights = (float *)malloc((size_t)w_bytes);
        p.layers[i].bias = (float *)malloc((size_t)b_bytes);
        if (!p.layers[i].weights || !p.layers[i].bias) {
            /* Real, honest cleanup on a real allocation failure -- never leave a half-loaded
             * MlpPolicy that looks loaded but has NULL layer buffers past this point. */
            for (int j = 0; j <= i; j++) {
                free(p.layers[j].weights);
                free(p.layers[j].bias);
            }
            return 0;
        }
        memcpy(p.layers[i].weights, data + cursor, (size_t)w_bytes);
        cursor += w_bytes;
        memcpy(p.layers[i].bias, data + cursor, (size_t)b_bytes);
        cursor += b_bytes;
    }

    p.loaded = 1;
    *out = p;
    return 1;
}

static inline int mlp_policy_load_from_file(const char *path, MlpPolicy *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return 0;
    }
    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return 0;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    int ok = ((long)n == size) && mlp_policy_load_from_memory(buf, size, out);
    free(buf);
    return ok;
}

static inline void mlp_policy_free(MlpPolicy *p) {
    if (!p->loaded) return;
    for (int i = 0; i < p->num_layers; i++) {
        free(p->layers[i].weights);
        free(p->layers[i].bias);
    }
    memset(p, 0, sizeof(*p));
}

/* mlp_policy_forward runs the real forward pass: obs (obs_size floats) through every layer in
 * order, writing the final layer's raw output into out_action (must be at least the last layer's
 * own out_dim). Returns 1 on success, 0 if obs_size doesn't match the first layer's own real
 * in_dim (a real, checked mismatch -- e.g. a stale/mismatched export -- rather than silently
 * reading past the buffer). Deliberately NOT clamped to [-1,1] here -- BrawlpitPacketEnv's own
 * real training-time action space is the caller's own context to apply, matching
 * PARENA-runtime.h's own "the host applies domain meaning, the primitive stays generic"
 * convention. */
static inline int mlp_policy_forward(const MlpPolicy *p, const float *obs, int obs_size, float *out_action, int out_action_cap) {
    if (!p->loaded || p->num_layers == 0) return 0;
    if (obs_size != p->layers[0].in_dim) return 0;
    if (out_action_cap < p->layers[p->num_layers - 1].out_dim) return 0;

    /* Two alternating scratch buffers -- layer l writes into buf_a/buf_b by parity while
     * reading the OTHER buffer (or the original obs, for l==0) as input, so there's never a
     * read/write aliasing hazard within one layer's own loop below. */
    float buf_a[MLP_POLICY_MAX_UNITS];
    float buf_b[MLP_POLICY_MAX_UNITS];
    const float *cur_in = obs;
    int cur_in_dim = obs_size;

    for (int l = 0; l < p->num_layers; l++) {
        const MlpLayer *layer = &p->layers[l];
        float *dst = (l % 2 == 0) ? buf_a : buf_b;
        for (int o = 0; o < layer->out_dim; o++) {
            float sum = layer->bias[o];
            const float *w_row = layer->weights + (size_t)o * layer->in_dim;
            for (int i = 0; i < cur_in_dim; i++) {
                sum += w_row[i] * cur_in[i];
            }
            dst[o] = (layer->activation == MLP_ACTIVATION_TANH) ? tanhf(sum) : sum;
        }
        cur_in = dst;
        cur_in_dim = layer->out_dim;
    }

    memcpy(out_action, cur_in, (size_t)cur_in_dim * sizeof(float));
    return 1;
}

#endif

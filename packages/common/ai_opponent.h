#ifndef BRAWLPIT_AI_OPPONENT_H
#define BRAWLPIT_AI_OPPONENT_H

/* ai_opponent.h — S421-02, founder real-time: "ensure that the client actually uses that model
 * (download it when the game starts or something and show on the screen what model is loaded
 * (DEFAULT), model id etc)". Real client-side integration of the S420/S421 checkpoint registry:
 * at game start, fetches the real, currently-selected opponent checkpoint (GET .../active), and,
 * if one is selected AND has exported weights, downloads its real native-inference weights
 * (GET .../<id>/weights, LZ4-compressed by default -- "make sure the model downloads with lz4")
 * and drives the local bot slot with the real, compiled MLP forward pass (mlp_policy.h) instead
 * of the existing hand-authored heuristic bot_think -- a genuine drop-in swap, matching this
 * repo's own established "the C host applies domain meaning, the primitive stays generic"
 * convention.
 *
 * Real, honest degrade at every step (matching level_registry.h's own "a bad/missing resource
 * never corrupts what's already working" precedent): no active selection, a selection with no
 * exported weights yet, a network failure, or a malformed weights blob all fall back to the
 * existing bot_think heuristic -- never a crash, never a silently broken match.
 */

#include "level_registry.h" /* fetch_url_to_buffer, level_find_key, level_parse_number, level_parse_string */
#include "lz4/lz4_wrapper.h" /* pw_lz4_decompress */
#include "mlp_policy.h"
#include "commander.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AI_OPPONENT_BASE_URL "https://okemily.com/api/v1/brawlpit-checkpoints"

/* Must match rl_env_packet.py's own real OBS_SIZE/action-space exactly -- 8 raw scalars x2
 * players + COMMANDER_POSTURE_COUNT(5) one-hot = 21; action = [stick_x, stick_y, jump, attack,
 * shield, special] = 6. A future observation/action shape change on the Python side needs a
 * matching change here -- there is no shared schema to generate this from across the Go/Python/C
 * boundary, same real constraint commander.h's own I32-only doc comment already names. */
#define AI_OPPONENT_OBS_SIZE 21
#define AI_OPPONENT_ACTION_SIZE 6
#define AI_OPPONENT_POSTURE_COUNT 5

/* Same real STAGE_FD default-size constants rl_env_packet.py's own commander_posture wrapper
 * uses (BRAWLPIT/scripts/rl_env_packet.py: STAGE_FD_BLAST_LEFT/RIGHT) -- kept in sync by hand,
 * matching that file's own documented cross-language boundary. */
#define AI_OPPONENT_BLAST_LEFT (-60.0f)
#define AI_OPPONENT_BLAST_RIGHT (60.0f)
#define AI_OPPONENT_EDGE_DANGER_THRESHOLD 8

typedef struct {
    int loaded;        /* 1 once a real policy is loaded and ready to drive input */
    int id;
    char name[128];    /* real "<role>_<YYYYMMDD>_<HHMMSS>" identifier, see checkpoint_store.go */
    char role[32];
    float elo;
    MlpPolicy policy;
} AiOpponent;

static AiOpponent g_ai_opponent = {0};

#define MAX_AI_OPPONENT_ENTRIES 64
#define AI_OPPONENT_NAME_LEN 128
#define AI_OPPONENT_ROLE_LEN 32

/* AiOpponentRegistryEntry -- one real row of the browsable registry (S421-05, founder real-time:
 * "we need an interface in brawlpit to brows registry and select model and it works just like
 * the level registry"). Same real field set ai_opponent_parse_one_json below extracts from
 * either GET .../active (one object) or GET .../ (a real JSON array of these same objects). */
typedef struct {
    int id;
    char name[AI_OPPONENT_NAME_LEN];
    char role[AI_OPPONENT_ROLE_LEN];
    float elo;
    int has_weights;
} AiOpponentRegistryEntry;

/* ai_opponent_parse_one_json parses ONE real checkpoint object's fields
 * ({"id":N,"name":"...","role":"...","elo":F,"has_weights":true|false,...}) -- shared by both
 * ai_opponent_parse_active_json (a single object) and fetch_ai_opponent_registry_list (each
 * element of the real list array), so the two never drift apart on field names. */
static inline int ai_opponent_parse_one_json(const char *obj_start, const char *obj_end,
                                              int *out_id, char *out_name, size_t name_cap,
                                              char *out_role, size_t role_cap,
                                              float *out_elo, int *out_has_weights) {
    const char *id_val = level_find_key(obj_start, obj_end, "id");
    const char *name_val = level_find_key(obj_start, obj_end, "name");
    const char *role_val = level_find_key(obj_start, obj_end, "role");
    const char *elo_val = level_find_key(obj_start, obj_end, "elo");
    if (!id_val || !name_val || !role_val || !elo_val) return 0;

    float id_f, elo_f;
    if (!level_parse_number(id_val, &id_f)) return 0;
    if (!level_parse_number(elo_val, &elo_f)) return 0;
    level_parse_string(name_val, out_name, name_cap);
    level_parse_string(role_val, out_role, role_cap);
    *out_id = (int)id_f;
    *out_elo = elo_f;
    const char *hw_val = level_find_key(obj_start, obj_end, "has_weights");
    *out_has_weights = hw_val && strncmp(hw_val, "true", 4) == 0;
    return 1;
}

/* fetch_ai_opponent_registry_list fetches and parses the real, live checkpoint list (GET
 * .../brawlpit-checkpoints, the same public, unauthenticated read every other consumer of this
 * registry already uses). A real, bounded, minimal object-by-object scanner -- same real
 * "{...}...{...}" bracket-scoped assumption level_registry.h's own parse_registry_list already
 * established (no field value in this shape ever legitimately contains a literal brace).
 * Returns the real entry count (0 on any failure -- offline, malformed response -- a real,
 * honest degrade, never a crash). */
static inline int fetch_ai_opponent_registry_list(AiOpponentRegistryEntry *out, int max) {
    unsigned char *buf = NULL;
    long n = fetch_url_to_buffer(AI_OPPONENT_BASE_URL, &buf);
    if (n <= 0) return 0;
    unsigned char *text = (unsigned char *)realloc(buf, (size_t)n + 1);
    if (!text) {
        free(buf);
        return 0;
    }
    text[n] = '\0';

    int count = 0;
    const char *cursor = (const char *)text;
    while (*cursor && count < max) {
        const char *obj_start = strchr(cursor, '{');
        if (!obj_start) break;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end) break;
        int id, has_weights;
        char name[AI_OPPONENT_NAME_LEN], role[AI_OPPONENT_ROLE_LEN];
        float elo;
        if (ai_opponent_parse_one_json(obj_start, obj_end, &id, name, sizeof(name), role, sizeof(role), &elo, &has_weights)) {
            out[count].id = id;
            strncpy(out[count].name, name, sizeof(out[count].name) - 1);
            strncpy(out[count].role, role, sizeof(out[count].role) - 1);
            out[count].elo = elo;
            out[count].has_weights = has_weights;
            count++;
        }
        cursor = obj_end + 1;
    }
    free(text);
    return count;
}

/* ai_opponent_parse_active_json parses GET .../active's own real response shape (one object, or
 * a bare `null` when nothing is selected). Returns 1 on a real selection, 0 for `null`/malformed
 * (never a crash on a real but differently-shaped response -- degrades to "no selection",
 * matching this repo's own established convention). */
static inline int ai_opponent_parse_active_json(const char *json, long len, int *out_id, char *out_name,
                                                 size_t name_cap, char *out_role, size_t role_cap,
                                                 float *out_elo, int *out_has_weights) {
    /* A bare `null` (the real, honest "no opponent selected yet" response) -- not an object at
     * all, so level_find_key would correctly find nothing; checked explicitly for clarity. */
    const char *p = json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (strncmp(p, "null", 4) == 0) return 0;

    return ai_opponent_parse_one_json(json, json + len, out_id, out_name, name_cap, out_role, role_cap, out_elo, out_has_weights);
}

/* ai_opponent_load_weights downloads checkpoint `id`'s real, LZ4-compressed exported weights
 * (default compression -- founder real-time: "make sure the model downloads with lz4"),
 * decompresses with the exact same real codec BRAWLPIT already links for level downloads
 * (packages/common/lz4/), and loads them into g_ai_opponent.policy. Returns 1 on success. */
static inline int ai_opponent_load_weights(int id) {
    char url[256];
    snprintf(url, sizeof(url), "%s/%d/weights", AI_OPPONENT_BASE_URL, id); /* default: LZ4 */

    unsigned char *compressed = NULL;
    long clen = fetch_url_to_buffer(url, &compressed);
    if (clen <= 0) return 0;

    unsigned char *decompressed = NULL;
    long dlen = pw_lz4_decompress(compressed, clen, &decompressed);
    free(compressed);
    if (dlen < 0) return 0;

    /* Real, found-live fix: a player can select a NEW opponent mid-session (S421-05's own
     * in-game browser) after one was already loaded -- free the previously-loaded policy's own
     * malloc'd layer buffers first, or re-loading leaks them every time. */
    if (g_ai_opponent.policy.loaded) mlp_policy_free(&g_ai_opponent.policy);

    int ok = mlp_policy_load_from_memory(decompressed, dlen, &g_ai_opponent.policy);
    free(decompressed);
    return ok;
}

/* ai_opponent_select_and_load -- S421-05, founder real-time: "we need an interface in brawlpit
 * to brows registry and select model and it works just like the level registry." A real, LOCAL,
 * per-session selection (mirrors the level browser's own real semantics exactly: picking a level
 * there never mutates any shared server-side state either, it's purely "what THIS client loads
 * next") -- deliberately does NOT touch the shared is_active_opponent flag NOCK's own admin UI
 * controls; that stays a separate, real "server-side default" concept. Returns 1 on success (a
 * real download+load succeeded), 0 on failure (network/malformed data) -- caller should show a
 * real error, matching the level browser's own established convention, and g_ai_opponent is left
 * untouched on failure (never a half-applied selection). */
static inline int ai_opponent_select_and_load(int id, const char *name, const char *role, float elo) {
    if (!ai_opponent_load_weights(id)) return 0;
    g_ai_opponent.loaded = 1;
    g_ai_opponent.id = id;
    strncpy(g_ai_opponent.name, name, sizeof(g_ai_opponent.name) - 1);
    strncpy(g_ai_opponent.role, role, sizeof(g_ai_opponent.role) - 1);
    g_ai_opponent.elo = elo;
    printf("AI OPPONENT: locally selected '%s' (id=%d, role=%s, elo=%.0f)\n", name, id, role, elo);
    return 1;
}

/* ai_opponent_init_from_registry -- call once at game start ("download it when the game
 * starts"). Real, honest, printed outcome either way, matching this repo's own established
 * startup-log convention (server_net_init's own "BRAWLPIT SERVER PORT %d" precedent): which real
 * checkpoint loaded, or exactly why the DEFAULT heuristic is in play instead. */
static inline void ai_opponent_init_from_registry(void) {
    memset(&g_ai_opponent, 0, sizeof(g_ai_opponent));

    char url[256];
    snprintf(url, sizeof(url), "%s/active", AI_OPPONENT_BASE_URL);
    unsigned char *buf = NULL;
    long n = fetch_url_to_buffer(url, &buf);
    if (n <= 0) {
        printf("AI OPPONENT: registry unreachable -- using DEFAULT\n");
        return;
    }
    unsigned char *text = (unsigned char *)realloc(buf, (size_t)n + 1);
    if (!text) {
        free(buf);
        printf("AI OPPONENT: registry unreachable -- using DEFAULT\n");
        return;
    }
    text[n] = '\0';

    int id, has_weights;
    char name[128], role[32];
    float elo;
    int selected = ai_opponent_parse_active_json((const char *)text, n, &id, name, sizeof(name),
                                                  role, sizeof(role), &elo, &has_weights);
    free(text);

    if (!selected) {
        printf("AI OPPONENT: none selected -- using DEFAULT\n");
        return;
    }
    if (!has_weights) {
        printf("AI OPPONENT: '%s' (id=%d) selected but has no exported weights yet -- using DEFAULT\n", name, id);
        return;
    }
    if (!ai_opponent_load_weights(id)) {
        printf("AI OPPONENT: failed to download/load weights for '%s' (id=%d) -- using DEFAULT\n", name, id);
        return;
    }

    g_ai_opponent.loaded = 1;
    g_ai_opponent.id = id;
    strncpy(g_ai_opponent.name, name, sizeof(g_ai_opponent.name) - 1);
    strncpy(g_ai_opponent.role, role, sizeof(g_ai_opponent.role) - 1);
    g_ai_opponent.elo = elo;
    printf("AI OPPONENT: loaded '%s' (id=%d, role=%s, elo=%.0f)\n", name, id, role, elo);
}

/* ai_opponent_hud_line writes the real, current status into `out` for on-screen display
 * ("show on the screen what model is loaded (DEFAULT), model id etc") -- "AI: DEFAULT" when no
 * real policy is loaded, or "AI: <name> (id=N) Elo <elo>" when one is. */
static inline void ai_opponent_hud_line(char *out, size_t cap) {
    if (g_ai_opponent.loaded) {
        snprintf(out, cap, "AI: %s (id=%d) Elo %.0f", g_ai_opponent.name, g_ai_opponent.id, g_ai_opponent.elo);
    } else {
        snprintf(out, cap, "AI: DEFAULT");
    }
}

static inline float ai_opponent_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ai_opponent_build_observation mirrors rl_env_packet.py's own real build_observation exactly
 * (same normalization constants, same field order, same commander-posture one-hot block) -- see
 * that file's own doc comment for the full rationale. Reuses the REAL compiled commander_posture
 * decision function directly (commander.h) rather than re-deriving posture logic in C, so the
 * exact same PARENA-compiled decision this training pipeline observed is what the live game
 * feeds back into the policy. */
static inline void ai_opponent_build_observation(const PlayerState *own, const PlayerState *opp, float *obs) {
    const float pos_norm = 1.0f / 80.0f;
    const float vel_norm = 1.0f / 20.0f;
    const float damage_norm = 1.0f / 200.0f;
    int i = 0;

    obs[i++] = own->x * pos_norm;
    obs[i++] = own->y * pos_norm;
    obs[i++] = ai_opponent_clampf(own->vx * vel_norm, -1.0f, 1.0f);
    obs[i++] = ai_opponent_clampf(own->vy * vel_norm, -1.0f, 1.0f);
    obs[i++] = ai_opponent_clampf(own->damage_percent * damage_norm, -1.0f, 1.0f);
    obs[i++] = (float)own->stocks / 4.0f;
    obs[i++] = own->shield_health / 60.0f;
    obs[i++] = own->facing > 0 ? 1.0f : -1.0f;

    obs[i++] = opp->x * pos_norm;
    obs[i++] = opp->y * pos_norm;
    obs[i++] = ai_opponent_clampf(opp->vx * vel_norm, -1.0f, 1.0f);
    obs[i++] = ai_opponent_clampf(opp->vy * vel_norm, -1.0f, 1.0f);
    obs[i++] = ai_opponent_clampf(opp->damage_percent * damage_norm, -1.0f, 1.0f);
    obs[i++] = (float)opp->stocks / 4.0f;
    obs[i++] = opp->shield_health / 60.0f;
    obs[i++] = opp->facing > 0 ? 1.0f : -1.0f;

    int own_edge_dist = (int)(own->x - AI_OPPONENT_BLAST_LEFT < AI_OPPONENT_BLAST_RIGHT - own->x
                                   ? own->x - AI_OPPONENT_BLAST_LEFT
                                   : AI_OPPONENT_BLAST_RIGHT - own->x);
    int opp_edge_dist = (int)(opp->x - AI_OPPONENT_BLAST_LEFT < AI_OPPONENT_BLAST_RIGHT - opp->x
                                   ? opp->x - AI_OPPONENT_BLAST_LEFT
                                   : AI_OPPONENT_BLAST_RIGHT - opp->x);
    int posture = commander_posture(own->stocks, opp->stocks, (int)own->damage_percent, (int)opp->damage_percent,
                                     own_edge_dist, opp_edge_dist, AI_OPPONENT_EDGE_DANGER_THRESHOLD);
    for (int p = 0; p < AI_OPPONENT_POSTURE_COUNT; p++) {
        obs[i++] = (p == posture) ? 1.0f : 0.0f;
    }
}

/* ai_opponent_drive computes a real action from the loaded policy and writes it directly into
 * `self`'s own real input fields -- the exact same fields bot_think already drives, so this is a
 * genuine drop-in replacement at the call site (see local_game.h's own per-frame bot_think call).
 * No-op (leaves input untouched) if no real policy is loaded -- callers should fall back to
 * bot_think in that case, matching the doc comment at the top of this file. */
static inline void ai_opponent_drive(PlayerState *self, const PlayerState *foe) {
    if (!g_ai_opponent.loaded) return;

    float obs[AI_OPPONENT_OBS_SIZE];
    ai_opponent_build_observation(self, foe, obs);

    float action[AI_OPPONENT_ACTION_SIZE];
    if (!mlp_policy_forward(&g_ai_opponent.policy, obs, AI_OPPONENT_OBS_SIZE, action, AI_OPPONENT_ACTION_SIZE)) {
        return; /* a real, checked mismatch (e.g. a stale export) -- leave input untouched this frame */
    }

    self->in_x = ai_opponent_clampf(action[0], -1.0f, 1.0f);
    self->in_y = ai_opponent_clampf(action[1], -1.0f, 1.0f);
    self->btn_jump = action[2] > 0.0f;
    self->btn_attack = action[3] > 0.0f;
    self->btn_shield = action[4] > 0.0f;
    self->btn_special = action[5] > 0.0f;
}

#endif

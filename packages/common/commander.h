#ifndef BRAWLPIT_COMMANDER_H
#define BRAWLPIT_COMMANDER_H

/* commander.h — S419, founder real-time: build a packet-level RL training pipeline for BRAWLPIT
 * applying REDGARDEN's own autocurriculum/league doctrine (NORTHSTAR.md §25.4/§25.4.1) and its
 * real "fractal commander" hierarchy (§26.3) as a single-agent commander -- explicitly WITHOUT
 * the squad-coordination layer (§26.3's own "commander-soldier" nesting), since BRAWLPIT has no
 * teams. `commander_posture` is a real PARENA-compiled decision function (PARENA/stdlib/brawlpit/
 * commander_mod.prn, compiled via `parena build`, checked in verbatim at
 * packages/common/commander/commander_mod.c -- do not edit that file by hand) -- one real
 * decision function, shared by scripts/rl_env_packet.py (via ctypes on
 * libbrawlpit_commander.so, appending the returned posture as an extra observation feature) and
 * any future in-game hybrid bot that wants the same real strategic signal, matching this
 * monorepo's own "same shape trains as ships" discipline.
 *
 * Real, rule-based-first scope, matching REDGARDEN NORTHSTAR §26.3's own precedent: not a
 * learned policy, a deterministic strategic-directive layer the low-level PPO "soldier" policy
 * conditions on.
 */

/* Posture enum -- see commander_mod.prn's own doc comment for the full rationale on each value
 * and the real, deliberate priority ordering (edge-safety beats stock/damage). */
#define COMMANDER_POSTURE_NEUTRAL   0
#define COMMANDER_POSTURE_AGGRESSIVE 1
#define COMMANDER_POSTURE_PATIENT   2
#define COMMANDER_POSTURE_EDGEGUARD 3
#define COMMANDER_POSTURE_RECOVER   4

/* Declared here, defined in packages/common/commander/commander_mod.c (PARENA-generated). I32
 * only, matching commander_mod.prn's own real VS0 FFI-boundary constraint (no F32/struct/Vec
 * across #target yet) -- edge distance is pre-rounded to the nearest world-unit int by the
 * caller. */
int commander_posture(int own_stocks, int opp_stocks,
                       int own_damage_pct, int opp_damage_pct,
                       int own_edge_dist, int opp_edge_dist,
                       int edge_danger_threshold);

/* EDGE_DANGER_THRESHOLD_DEFAULT -- a real, tunable world-unit distance-from-blast-zone-edge
 * below which a fighter is considered in real recovery danger. Deliberately a plain constant
 * here rather than derived from a level's own stage_blast_* variables (physics.h) -- those are
 * level-scaled at load time (S417-01) and callers that already have that context should compute
 * their own edge_dist/threshold pair against it; this default is for a caller (or a quick training
 * harness) that just wants a reasonable, real number without wiring the full stage-scaling path. */
#define COMMANDER_EDGE_DANGER_THRESHOLD_DEFAULT 8

#endif

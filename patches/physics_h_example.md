# Example Patch: `physics.h` (Friction 3D → 2D)

## Git Diff
```diff
--- a/packages/common/physics.h
+++ b/packages/common/physics.h
@@ -3,6 +3,52 @@
 #define PHYSICS_H
 #include <math.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include "protocol.h"
+
+// --- BRAWLPIT: 2D friction helpers (Phase 1 example) ---
+typedef struct { float x, y; } Vec2;
+typedef struct { float x, y, z; } Vec3; /* legacy compatibility */
+
+static inline void apply_friction_2d(Vec2 *vel, float friction_per_sec, float dt) {
+    float vx = vel->x;
+    float vy = vel->y;
+    float speed_sq = (vx * vx) + (vy * vy);
+    if (speed_sq <= 1e-8f) {
+        vel->x = 0.0f;
+        vel->y = 0.0f;
+        return;
+    }
+
+    float speed = sqrtf(speed_sq);
+    float reduction = friction_per_sec * dt;
+    float new_speed = speed - reduction;
+    if (new_speed <= 0.0f) {
+        vel->x = 0.0f;
+        vel->y = 0.0f;
+        return;
+    }
+
+    float scale = new_speed / speed;
+    vel->x = vx * scale;
+    vel->y = vy * scale;
+}
+
+// Converts legacy 3D friction to 2D by ignoring the z-axis.
+static inline void apply_friction(Vec3 *vel, float friction_per_sec, float dt) {
+    Vec2 v2 = { vel->x, vel->y };
+    apply_friction_2d(&v2, friction_per_sec, dt);
+    vel->x = v2.x;
+    vel->y = v2.y;
+}
 
 // --- SMASH PHYSICS TUNING ---
 #define GRAVITY 0.065f
 #define FAST_FALL_GRAVITY 0.14f
```

## Rationale
- **Explicit 2D friction helper** keeps Phase 1 math aligned to x/y movement only.
- **Compatibility shim** preserves legacy 3D call sites by projecting into 2D, so older code paths can still compile.
- **Stable numerical behavior** by clamping to zero at tiny magnitudes and preventing negative speeds.

## Test Criteria
1. **Unit test (logic)**
   - Input: `vel = {1.0, 0.0}`, `friction_per_sec = 1.0`, `dt = 0.5`.
   - Expect: speed reduces to `0.5`, `vel = {0.5, 0.0}`.
2. **Clamp to zero**
   - Input: `vel = {0.01, 0.0}`, `friction_per_sec = 1.0`, `dt = 1.0`.
   - Expect: `vel = {0.0, 0.0}`.
3. **Legacy shim**
   - Input: `Vec3 vel = {1.0, 1.0, 999.0}`, `friction_per_sec = 0.0`, `dt = 0.016`.
   - Expect: `x/y` unchanged, `z` untouched by this helper.

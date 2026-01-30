/* tests/test_physics.c */
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <netinet/in.h>
#endif

/* Include the actual headers instead of redefining them */
#include "../packages/common/physics.h"

int main() {
    printf("BRAWLPIT Phase 1 Physics Smoke Test\n");
    
    Vec2 vel = {10.0f, 0.0f};
    float dt = 1.0f / 60.0f;
    float friction = 0.5f;
    
    printf("Frame | Vx      | Vy\n");
    for(int i = 0; i < 60; i++) {
        apply_friction_2d(&vel, friction, dt);
        if (i % 10 == 0) printf("%5d | %7.4f | %7.4f\n", i, vel.x, vel.y);
    }
    
    /* Validation */
    if (vel.x < 10.0f && vel.x > 0.0f) {
        printf("\n✅ PASS: Friction applied correctly (Final Vx: %f)\n", vel.x);
        return 0;
    }
    
    printf("\n❌ FAIL: Velocity check failed (Final Vx: %f)\n", vel.x);
    return 1;
}

// --- HACK 1: FLOOR BOUNCING (Missed Techs) ---
// We modify the collision resolver to check if a player is hitting the ground
// fast while stunned. If so, they bounce instead of landing efficiently.
void resolve_platform_collisions(PlayerState *p, float prev_y) {
    float pw = 2.0f; // Player Width
    float ph = 4.0f; // Player Height
    
    p->on_ground = 0;
    
    // Don't collide if moving upwards
    if (p->vy > 0) {
        p->ground_platform_type = -1;
        return;
    }

    for(int i=0; i<stage_count; i++) {
        Platform b = stage_geo[i];

        if (b.type == 1 && p->drop_through_timer > 0) continue;

        if (p->x + pw/2 > b.x - b.w/2 && p->x - pw/2 < b.x + b.w/2) {
            float top = b.y + b.h/2;
            if (prev_y >= top && p->y <= top) {
                // --- TECH CHECK ---
                // If hitting ground fast while stunned, BOUNCE.
                if (p->state == STATE_STUNNED && p->vy < -1.8f) {
                    p->y = top + 0.1f;
                    p->vy *= -0.7f; // Retain 70% energy upwards
                    // Apply friction to the bounce
                    p->vx *= 0.5f; 
                    // Add visual flair? (Screen shake trigger could go here)
                    return; 
                }
                // ------------------

                if (b.type == 1 && p->in_y < -0.6f) continue;
                
                p->y = top;
                p->vy = 0;
                p->on_ground = 1;
                p->jumps_remaining = MAX_JUMPS;
                p->ground_platform_type = b.type;
                return;
            }
        }
    }
    p->ground_platform_type = -1;
}

// --- HACK 2: AERIALS & DI ---
// We replace the generic hitbox logic with context-sensitive moves.
void check_attack_hitbox(PlayerState *attacker, PlayerState *target) {
    if (attacker->attack_timer <= 0 && attacker->smash_active_timer <= 0) return;
    if (target->invuln_frames > 0) return;
    if (target->state == STATE_DEAD) return;

    // Hitbox definition (Front of player)
    float reach = 3.5f;
    float hx = attacker->x + (attacker->facing * 2.0f);
    float hy = attacker->y + 1.0f;
    float hw = reach; 
    float hh = 2.5f;

    // Target Hurtbox
    float tx = target->x; 
    float ty = target->y + 2.0f;
    float tw = 2.0f; 
    float th = 4.0f;

    if (check_aabb(hx - hw/2, hy - hh/2, hw, hh, tx - tw/2, ty - th/2, tw, th)) {
        
        // 1. DETERMINE KNOCKBACK PROFILE
        float kb_x = attacker->facing * 1.0f;
        float kb_y = 0.8f;
        float damage = 12.0f;
        int heavy_hit = 0;

        if (attacker->smash_active_timer > 0) {
            // ... (Existing Smash Logic) ...
            float charge = attacker->smash_charge_level;
            damage = 14.0f + (12.0f * charge);
            kb_x = attacker->facing * (1.0f + 1.2f * charge);
            kb_y = 0.9f + 0.9f * charge;
            heavy_hit = 1;
        } 
        else if (!attacker->on_ground) {
            // --- AERIALS ---
            // Forward Air (Fair) -> THE SPIKE
            // Trigger: Holding Forward relative to facing
            if (attacker->in_x * attacker->facing > 0.1f) {
                damage = 14.0f;
                kb_x = attacker->facing * 0.6f;
                kb_y = -1.6f; // METEOR SMASH
                heavy_hit = 1;
            }
            // Back Air (Bair) -> THE KILL MOVE
            // Trigger: Holding Backward relative to facing
            else if (attacker->in_x * attacker->facing < -0.1f) {
                damage = 15.0f;
                kb_x = attacker->facing * 1.6f; // High horizontal KB
                kb_y = 0.8f;
                heavy_hit = 1;
            }
            // Neutral Air (Nair) -> Get off me
            else {
                damage = 9.0f;
                kb_x = attacker->facing * 1.1f;
                kb_y = 0.5f;
            }
        } 
        else {
            // --- GROUND NORMALS ---
            if (attacker->in_y > 0.5f) { kb_y = 1.5f; kb_x *= 0.2f; } // Up Tilt
            else if (attacker->in_y < -0.5f) { kb_y = -0.5f; kb_x *= 1.2f; } // Down Smash
        }
        
        // 2. SHIELD LOGIC (Keep existing)
        if (target->state == STATE_SHIELD) {
            // ... (Copy existing shield logic here) ...
            if (target->parry_timer > 0) {
                 target->parry_timer = 0;
                 target->vx = 0; target->vy = 0;
                 attacker->vx = -attacker->facing * 1.4f;
                 attacker->vy = 0.6f;
                 attacker->hitstun_frames = 18;
                 attacker->state = STATE_STUNNED;
                 return;
            }
            float shield_damage = damage;
            target->shield_health -= shield_damage;
            target->shield_regen_timer = 60;
            target->shield_stun_frames = (int)(shield_damage * 5.0f);
            target->vx += attacker->facing * 0.08f * shield_damage;
            attacker->vx -= attacker->facing * 0.05f * shield_damage;
            if (target->shield_health <= 0) {
                target->state = STATE_STUNNED;
                target->hitstun_frames = SHIELD_BREAK_STUN;
                target->shield_health = 0;
            }
            if (target->shield_health > SHIELD_MAX) target->shield_health = SHIELD_MAX;
            return; 
        }

        // 3. APPLY DIRECTIONAL INFLUENCE (DI)
        // Survivors can hold perpendicular to trajectory to survive
        if (target->damage_percent > 50.0f) { // Only matters at higher percent
            float di_strength = 0.4f; // Influence strength
            
            // Add raw input to knockback vector before scaling
            kb_x += target->in_x * di_strength;
            kb_y += target->in_y * di_strength;
        }

        apply_knockback(target, damage, kb_x, kb_y);

        // 4. HITLAG / HITSTOP
        // Pause both players briefly on heavy hits to sell the impact
        if (heavy_hit) {
            int freeze = 6 + (int)(damage * 0.5f);
            attacker->hitlag_frames = freeze;
            target->hitlag_frames = freeze;
            // Add hit flash
            target->hit_flash_timer = 10;
        }

        if (attacker->smash_active_timer > 0) {
            attacker->attack_cooldown = SMASH_COOLDOWN_FRAMES;
        } else {
            attacker->attack_cooldown = ATTACK_COOLDOWN_FRAMES;
        }
    }
}

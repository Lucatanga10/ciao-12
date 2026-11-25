#include "chain.h"

#include <math.h>
#include <string.h>

/* Il mondo di UE e' grande ma non infinito: oltre questo, la lettura e' rotta. */
#define OJBS_WORLD_LIMIT 2.0e7

bool chain_read_vector(const chain_ctx *ctx, uint64_t address, vec3 *out)
{
    if (ctx == NULL || ctx->off == NULL || out == NULL) {
        return false;
    }
    if (!ctx->off->has_fvector) {
        return false;
    }

    if (ctx->off->fvector_is_double) {
        double raw[3];
        if (!mem_read(&ctx->target, address, raw, sizeof(raw))) {
            return false;
        }
        *out = vec3_make(raw[0], raw[1], raw[2]);
    } else {
        float raw[3];
        if (!mem_read(&ctx->target, address, raw, sizeof(raw))) {
            return false;
        }
        *out = vec3_make((double)raw[0], (double)raw[1], (double)raw[2]);
    }

    return vec3_plausible(*out, OJBS_WORLD_LIMIT);
}

bool chain_read_rotator(const chain_ctx *ctx, uint64_t address, rot3 *out)
{
    if (ctx == NULL || ctx->off == NULL || out == NULL) {
        return false;
    }
    if (!ctx->off->has_frotator) {
        return false;
    }

    if (ctx->off->frotator_is_double) {
        double raw[3];
        if (!mem_read(&ctx->target, address, raw, sizeof(raw))) {
            return false;
        }
        out->pitch = raw[0];
        out->yaw = raw[1];
        out->roll = raw[2];
    } else {
        float raw[3];
        if (!mem_read(&ctx->target, address, raw, sizeof(raw))) {
            return false;
        }
        out->pitch = (double)raw[0];
        out->yaw = (double)raw[1];
        out->roll = (double)raw[2];
    }

    if (!vec3_finite(vec3_make(out->pitch, out->yaw, out->roll))) {
        return false;
    }

    return true;
}

bool chain_read_tarray(const chain_ctx *ctx, uint64_t address,
                       uint64_t *out_data, int32_t *out_count)
{
    uint64_t data = 0;
    int32_t count = 0;

    if (ctx == NULL || ctx->off == NULL || out_data == NULL || out_count == NULL) {
        return false;
    }
    if (!ctx->off->has_tarray) {
        return false;
    }

    *out_data = 0;
    *out_count = 0;

    if (!mem_read_ptr(&ctx->target, address + ctx->off->tarray_data, &data)) {
        return false;
    }
    if (!mem_read_i32(&ctx->target, address + ctx->off->tarray_num, &count)) {
        return false;
    }
    if (count < 0 || count > 1000000) {
        return false;
    }

    *out_data = data;
    *out_count = count;
    return true;
}

bool chain_resolve_world(const chain_ctx *ctx, uint64_t *out_world)
{
    uint64_t world = 0;

    if (ctx == NULL || ctx->off == NULL || out_world == NULL) {
        return false;
    }
    if (!ctx->off->has_gworld_rva) {
        return false;
    }
    if (ctx->target.module_base == 0) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      ctx->target.module_base + ctx->off->gworld_rva,
                      &world)) {
        return false;
    }
    if (!mem_looks_like_uobject(&ctx->target, world)) {
        return false;
    }

    *out_world = world;
    return true;
}

bool chain_local_controller(const chain_ctx *ctx, uint64_t world,
                            uint64_t *out_controller)
{
    uint64_t game_instance = 0;
    uint64_t players_data = 0;
    int32_t players_count = 0;
    uint64_t local_player = 0;
    uint64_t controller = 0;

    if (ctx == NULL || ctx->off == NULL || out_controller == NULL) {
        return false;
    }
    if (!ctx->off->has_uworld_owning_game_instance ||
        !ctx->off->has_ugameinstance_local_players ||
        !ctx->off->has_ulocalplayer_player_controller) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      world + ctx->off->uworld_owning_game_instance,
                      &game_instance)) {
        return false;
    }
    if (!chain_read_tarray(ctx,
                           game_instance + ctx->off->ugameinstance_local_players,
                           &players_data, &players_count)) {
        return false;
    }
    if (players_count <= 0) {
        return false;
    }
    if (!mem_read_ptr(&ctx->target, players_data, &local_player)) {
        return false;
    }
    if (!mem_read_ptr(&ctx->target,
                      local_player + ctx->off->ulocalplayer_player_controller,
                      &controller)) {
        return false;
    }
    if (!mem_looks_like_uobject(&ctx->target, controller)) {
        return false;
    }

    *out_controller = controller;
    return true;
}

bool chain_local_pawn(const chain_ctx *ctx, uint64_t controller,
                      uint64_t *out_pawn)
{
    uint64_t pawn = 0;

    if (ctx == NULL || ctx->off == NULL || out_pawn == NULL) {
        return false;
    }

    *out_pawn = 0;

    if (!ctx->off->has_apc_acknowledged_pawn) {
        return false;
    }
    if (!mem_read_ptr(&ctx->target,
                      controller + ctx->off->apc_acknowledged_pawn,
                      &pawn)) {
        return false;
    }

    *out_pawn = pawn;
    return true;
}

bool chain_read_camera(const chain_ctx *ctx, uint64_t controller,
                       chain_camera *out)
{
    uint64_t manager = 0;
    uint64_t pov = 0;
    float fov = 0.0f;

    if (ctx == NULL || ctx->off == NULL || out == NULL) {
        return false;
    }
    if (!ctx->off->has_apc_player_camera_manager ||
        !ctx->off->has_apcm_camera_cache_private ||
        !ctx->off->has_apcm_pov_in_cache_entry ||
        !ctx->off->has_fmvi_location ||
        !ctx->off->has_fmvi_rotation ||
        !ctx->off->has_fmvi_fov) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      controller + ctx->off->apc_player_camera_manager,
                      &manager)) {
        return false;
    }
    if (!mem_looks_like_uobject(&ctx->target, manager)) {
        return false;
    }

    pov = manager
        + ctx->off->apcm_camera_cache_private
        + ctx->off->apcm_pov_in_cache_entry;

    if (!chain_read_vector(ctx, pov + ctx->off->fmvi_location, &out->position)) {
        return false;
    }
    if (!chain_read_rotator(ctx, pov + ctx->off->fmvi_rotation, &out->rotation)) {
        return false;
    }
    if (!mem_read_f32(&ctx->target, pov + ctx->off->fmvi_fov, &fov)) {
        return false;
    }
    if (!(fov > 1.0f) || !(fov < 179.0f)) {
        return false;
    }

    out->fov = (double)fov;
    return true;
}

bool chain_actor_position(const chain_ctx *ctx, uint64_t actor, vec3 *out)
{
    uint64_t root = 0;

    if (ctx == NULL || ctx->off == NULL || out == NULL) {
        return false;
    }
    if (!ctx->off->has_aactor_root_component ||
        !ctx->off->has_uscene_relative_location) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      actor + ctx->off->aactor_root_component,
                      &root)) {
        return false;
    }

    if (ctx->off->uscene_location_is_float) {
        float raw[3];
        if (!mem_read(&ctx->target,
                      root + ctx->off->uscene_relative_location,
                      raw, sizeof(raw))) {
            return false;
        }
        *out = vec3_make((double)raw[0], (double)raw[1], (double)raw[2]);
        return vec3_plausible(*out, OJBS_WORLD_LIMIT);
    }

    return chain_read_vector(ctx, root + ctx->off->uscene_relative_location, out);
}

int chain_collect_actors(const chain_ctx *ctx, uint64_t world,
                         uint64_t skip_actor, vec3 camera_position,
                         double max_distance,
                         chain_actor *out, int capacity)
{
    uint64_t level = 0;
    uint64_t actors_data = 0;
    int32_t actors_count = 0;
    uint64_t pointers[OJBS_MAX_ACTORS];
    int to_read;
    int found = 0;
    int i;

    if (ctx == NULL || ctx->off == NULL || out == NULL || capacity <= 0) {
        return 0;
    }
    if (!ctx->off->has_uworld_persistent_level) {
        return 0;
    }

    if (!mem_read_ptr(&ctx->target,
                      world + ctx->off->uworld_persistent_level,
                      &level)) {
        return 0;
    }
    if (!ctx->off->has_ulevel_actors) {
        return 0;
    }
    if (!chain_read_tarray(ctx, level + ctx->off->ulevel_actors,
                           &actors_data, &actors_count)) {
        return 0;
    }
    if (actors_count <= 0) {
        return 0;
    }

    to_read = actors_count;
    if (to_read > OJBS_MAX_ACTORS) {
        to_read = OJBS_MAX_ACTORS;
    }
    if (to_read > capacity) {
        to_read = capacity;
    }

    memset(pointers, 0, sizeof(pointers));

    /* Un solo process_vm_readv per tutto l'array: 512 letture separate a
     * frame non stanno nel budget. */
    if (!mem_read(&ctx->target, actors_data, pointers,
                  (size_t)to_read * sizeof(uint64_t))) {
        return 0;
    }

    for (i = 0; i < to_read && found < capacity; ++i) {
        uint64_t actor = pointers[i];
        vec3 position;
        vec3 delta;
        double distance;

        if (actor == 0 || actor == skip_actor) {
            continue;
        }
        if (!mem_ptr_plausible(actor)) {
            continue;
        }
        if (!chain_actor_position(ctx, actor, &position)) {
            continue;
        }

        delta = vec3_sub(position, camera_position);
        distance = vec3_length(delta);

        if (!(distance > 0.0) || distance > max_distance) {
            continue;
        }

        out[found].actor = actor;
        out[found].position = position;
        out[found].distance = distance;
        found += 1;
    }

    return found;
}

#define SCAN_ACTORS 20
#define SCAN_RC_MIN 0x100u
#define SCAN_RC_MAX 0x220u
#define SCAN_RC_STEP 8u
#define SCAN_LOC_MIN 0x100u
#define SCAN_LOC_MAX 0x200u
#define SCAN_DIVERSITY 100.0

static bool scan_try_doubles(const chain_ctx *ctx,
                             const uint64_t *components, int comp_count,
                             uint32_t loc_off, int *out_pos_count)
{
    vec3 first;
    int pos_count = 0;
    bool have_first = false;
    int i;

    for (i = 0; i < comp_count; ++i) {
        double raw[3];
        vec3 pos;
        if (components[i] == 0) {
            continue;
        }
        if (!mem_read(&ctx->target, components[i] + loc_off, raw, sizeof(raw))) {
            continue;
        }
        pos = vec3_make(raw[0], raw[1], raw[2]);
        if (!vec3_plausible(pos, OJBS_WORLD_LIMIT)) {
            continue;
        }
        if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) {
            continue;
        }
        if (!have_first) {
            first = pos;
            have_first = true;
        }
        pos_count += 1;
        if (pos_count >= 3 && vec3_length(vec3_sub(pos, first)) > SCAN_DIVERSITY) {
            *out_pos_count = pos_count;
            return true;
        }
    }
    *out_pos_count = pos_count;
    return false;
}

static bool scan_try_floats(const chain_ctx *ctx,
                            const uint64_t *components, int comp_count,
                            uint32_t loc_off, int *out_pos_count)
{
    vec3 first;
    int pos_count = 0;
    bool have_first = false;
    int i;

    for (i = 0; i < comp_count; ++i) {
        float raw[3];
        vec3 pos;
        if (components[i] == 0) {
            continue;
        }
        if (!mem_read(&ctx->target, components[i] + loc_off, raw, sizeof(raw))) {
            continue;
        }
        pos = vec3_make((double)raw[0], (double)raw[1], (double)raw[2]);
        if (!vec3_plausible(pos, OJBS_WORLD_LIMIT)) {
            continue;
        }
        if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) {
            continue;
        }
        if (!have_first) {
            first = pos;
            have_first = true;
        }
        pos_count += 1;
        if (pos_count >= 3 && vec3_length(vec3_sub(pos, first)) > SCAN_DIVERSITY) {
            *out_pos_count = pos_count;
            return true;
        }
    }
    *out_pos_count = pos_count;
    return false;
}

bool chain_autoscan_position(const chain_ctx *ctx, uint64_t world,
                             uint32_t *out_rc_off, uint32_t *out_loc_off,
                             bool *out_loc_is_float)
{
    uint64_t level = 0;
    uint64_t actors_data = 0;
    int32_t actors_count = 0;
    uint64_t actor_ptrs[SCAN_ACTORS];
    int test_count = 0;
    uint32_t rc_off;
    uint32_t loc_off;
    int step;
    int i;

    if (ctx == NULL || ctx->off == NULL) {
        return false;
    }
    if (!ctx->off->has_uworld_persistent_level || !ctx->off->has_ulevel_actors) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      world + ctx->off->uworld_persistent_level, &level)) {
        return false;
    }
    if (!chain_read_tarray(ctx, level + ctx->off->ulevel_actors,
                           &actors_data, &actors_count)) {
        return false;
    }
    if (actors_count < 10) {
        return false;
    }

    step = actors_count / SCAN_ACTORS;
    if (step < 1) {
        step = 1;
    }
    for (i = 3; i < actors_count && test_count < SCAN_ACTORS; i += step) {
        uint64_t a = 0;
        if (mem_read_ptr(&ctx->target,
                         actors_data + (uint64_t)i * 8, &a) && a != 0) {
            actor_ptrs[test_count++] = a;
        }
    }
    if (test_count < 5) {
        return false;
    }

    for (rc_off = SCAN_RC_MIN; rc_off <= SCAN_RC_MAX; rc_off += SCAN_RC_STEP) {
        uint64_t components[SCAN_ACTORS];
        int readable = 0;

        memset(components, 0, sizeof(components));
        for (i = 0; i < test_count; ++i) {
            uint64_t comp = 0;
            if (mem_read_ptr(&ctx->target, actor_ptrs[i] + rc_off, &comp)) {
                components[i] = comp;
                readable += 1;
            }
        }
        if (readable < 3) {
            continue;
        }

        for (loc_off = SCAN_LOC_MIN; loc_off <= SCAN_LOC_MAX; loc_off += 8) {
            int pos_count = 0;
            if (scan_try_doubles(ctx, components, test_count,
                                 loc_off, &pos_count)) {
                *out_rc_off = rc_off;
                *out_loc_off = loc_off;
                *out_loc_is_float = false;
                return true;
            }
        }

        for (loc_off = SCAN_LOC_MIN; loc_off <= SCAN_LOC_MAX; loc_off += 4) {
            int pos_count = 0;
            if (scan_try_floats(ctx, components, test_count,
                                loc_off, &pos_count)) {
                *out_rc_off = rc_off;
                *out_loc_off = loc_off;
                *out_loc_is_float = true;
                return true;
            }
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Nuove funzioni: scan livelli, camera, raccolta multi-livello       */
/* ------------------------------------------------------------------ */

bool chain_autoscan_position_level(const chain_ctx *ctx, uint64_t level,
                                   uint32_t *out_rc_off, uint32_t *out_loc_off,
                                   bool *out_loc_is_float)
{
    uint64_t actors_data = 0;
    int32_t actors_count = 0;
    uint64_t actor_ptrs[SCAN_ACTORS];
    int test_count = 0;
    uint32_t rc_off;
    uint32_t loc_off;
    int step;
    int i;

    if (ctx == NULL || ctx->off == NULL) {
        return false;
    }
    if (!ctx->off->has_ulevel_actors) {
        return false;
    }

    if (!chain_read_tarray(ctx, level + ctx->off->ulevel_actors,
                           &actors_data, &actors_count)) {
        return false;
    }
    if (actors_count < 10) {
        return false;
    }

    step = actors_count / SCAN_ACTORS;
    if (step < 1) {
        step = 1;
    }
    for (i = 3; i < actors_count && test_count < SCAN_ACTORS; i += step) {
        uint64_t a = 0;
        if (mem_read_ptr(&ctx->target,
                         actors_data + (uint64_t)i * 8, &a) && a != 0) {
            actor_ptrs[test_count++] = a;
        }
    }
    if (test_count < 5) {
        return false;
    }

    for (rc_off = SCAN_RC_MIN; rc_off <= SCAN_RC_MAX; rc_off += SCAN_RC_STEP) {
        uint64_t components[SCAN_ACTORS];
        int readable = 0;

        memset(components, 0, sizeof(components));
        for (i = 0; i < test_count; ++i) {
            uint64_t comp = 0;
            if (mem_read_ptr(&ctx->target, actor_ptrs[i] + rc_off, &comp)) {
                components[i] = comp;
                readable += 1;
            }
        }
        if (readable < 3) {
            continue;
        }

        for (loc_off = SCAN_LOC_MIN; loc_off <= SCAN_LOC_MAX; loc_off += 8) {
            int pos_count = 0;
            if (scan_try_doubles(ctx, components, test_count,
                                 loc_off, &pos_count)) {
                *out_rc_off = rc_off;
                *out_loc_off = loc_off;
                *out_loc_is_float = false;
                return true;
            }
        }

        for (loc_off = SCAN_LOC_MIN; loc_off <= SCAN_LOC_MAX; loc_off += 4) {
            int pos_count = 0;
            if (scan_try_floats(ctx, components, test_count,
                                loc_off, &pos_count)) {
                *out_rc_off = rc_off;
                *out_loc_off = loc_off;
                *out_loc_is_float = true;
                return true;
            }
        }
    }

    return false;
}

bool chain_scan_levels_offset(const chain_ctx *ctx, uint64_t world,
                              uint32_t *out_levels_offset)
{
    uint64_t persistent_level = 0;
    uint32_t off;

    if (ctx == NULL || ctx->off == NULL || out_levels_offset == NULL) {
        return false;
    }
    if (!ctx->off->has_uworld_persistent_level) {
        return false;
    }

    if (!mem_read_ptr(&ctx->target,
                      world + ctx->off->uworld_persistent_level,
                      &persistent_level)) {
        return false;
    }
    if (persistent_level == 0) {
        return false;
    }

    for (off = 0x28; off <= 0x500; off += 8) {
        uint64_t data = 0;
        int32_t count = 0;
        int to_check;
        int i;

        if (!chain_read_tarray(ctx, world + off, &data, &count)) {
            continue;
        }
        if (count < 1 || count > 500) {
            continue;
        }

        to_check = count < 50 ? count : 50;
        for (i = 0; i < to_check; ++i) {
            uint64_t lvl = 0;
            if (!mem_read_ptr(&ctx->target, data + (uint64_t)i * 8, &lvl)) {
                continue;
            }
            if (lvl == persistent_level) {
                *out_levels_offset = off;
                return true;
            }
        }
    }

    return false;
}

int chain_collect_actors_from_level(const chain_ctx *ctx, uint64_t level,
                                    uint64_t skip_actor, vec3 camera_position,
                                    double max_distance,
                                    chain_actor *out, int capacity)
{
    uint64_t actors_data = 0;
    int32_t actors_count = 0;
    uint64_t pointers[OJBS_MAX_ACTORS];
    int to_read;
    int found = 0;
    int i;

    if (ctx == NULL || ctx->off == NULL || out == NULL || capacity <= 0) {
        return 0;
    }
    if (!ctx->off->has_ulevel_actors) {
        return 0;
    }

    if (!chain_read_tarray(ctx, level + ctx->off->ulevel_actors,
                           &actors_data, &actors_count)) {
        return 0;
    }
    if (actors_count <= 0) {
        return 0;
    }

    to_read = actors_count;
    if (to_read > OJBS_MAX_ACTORS) {
        to_read = OJBS_MAX_ACTORS;
    }
    if (to_read > capacity) {
        to_read = capacity;
    }

    memset(pointers, 0, sizeof(pointers));

    if (!mem_read(&ctx->target, actors_data, pointers,
                  (size_t)to_read * sizeof(uint64_t))) {
        return 0;
    }

    for (i = 0; i < to_read && found < capacity; ++i) {
        uint64_t actor = pointers[i];
        vec3 position;
        vec3 delta;
        double distance;

        if (actor == 0 || actor == skip_actor) {
            continue;
        }
        if (!mem_ptr_plausible(actor)) {
            continue;
        }
        if (!chain_actor_position(ctx, actor, &position)) {
            continue;
        }

        delta = vec3_sub(position, camera_position);
        distance = vec3_length(delta);

        if (!(distance > 0.0) || distance > max_distance) {
            continue;
        }

        out[found].actor = actor;
        out[found].position = position;
        out[found].distance = distance;
        found += 1;
    }

    return found;
}

int chain_collect_actors_all_levels(const chain_ctx *ctx, uint64_t world,
                                    uint32_t levels_offset,
                                    uint64_t skip_actor, vec3 camera_position,
                                    double max_distance,
                                    chain_actor *out, int capacity)
{
    uint64_t data = 0;
    int32_t count = 0;
    int found = 0;
    int i;

    if (ctx == NULL || ctx->off == NULL || out == NULL || capacity <= 0) {
        return 0;
    }

    if (!chain_read_tarray(ctx, world + levels_offset, &data, &count)) {
        return 0;
    }
    if (count < 1 || count > 200) {
        return 0;
    }

    for (i = 0; i < count && found < capacity; ++i) {
        uint64_t level = 0;
        if (!mem_read_ptr(&ctx->target, data + (uint64_t)i * 8, &level)) {
            continue;
        }
        if (!mem_looks_like_uobject(&ctx->target, level)) {
            continue;
        }

        found += chain_collect_actors_from_level(ctx, level,
                     skip_actor, camera_position, max_distance,
                     out + found, capacity - found);
    }

    return found;
}

bool chain_scan_camera_offset(const chain_ctx *ctx, uint64_t pcm,
                              uint32_t *out_cache_offset)
{
    uint32_t off;

    if (ctx == NULL || out_cache_offset == NULL || pcm == 0) {
        return false;
    }

    for (off = 0x100; off <= 0x2000; off += 8) {
        double loc[3];
        double rot[3];
        float fov;
        double mag;
        vec3 loc_v;
        vec3 rot_v;

        if (!mem_read(&ctx->target, pcm + off, loc, sizeof(loc))) {
            continue;
        }

        loc_v = vec3_make(loc[0], loc[1], loc[2]);
        if (!vec3_finite(loc_v)) {
            continue;
        }
        if (loc[0] == 0.0 && loc[1] == 0.0 && loc[2] == 0.0) {
            continue;
        }

        mag = vec3_length(loc_v);
        if (mag < 100.0 || mag > 2e7) {
            continue;
        }

        if (!mem_read(&ctx->target, pcm + off + 0x18, rot, sizeof(rot))) {
            continue;
        }
        rot_v = vec3_make(rot[0], rot[1], rot[2]);
        if (!vec3_finite(rot_v)) {
            continue;
        }

        if (!mem_read_f32(&ctx->target, pcm + off + 0x30, &fov)) {
            continue;
        }
        if (!(fov > 50.0f) || !(fov < 140.0f)) {
            continue;
        }

        *out_cache_offset = off;
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Raccolta attori da TArray arbitrario + scan FTransform              */
/* ------------------------------------------------------------------ */

int chain_collect_actors_from_tarray(const chain_ctx *ctx,
                                     uint64_t tarray_addr,
                                     uint64_t skip_actor,
                                     vec3 camera_position,
                                     double max_distance,
                                     chain_actor *out, int capacity)
{
    uint64_t actors_data = 0;
    int32_t actors_count = 0;
    uint64_t pointers[OJBS_MAX_ACTORS];
    int to_read;
    int found = 0;
    int i;

    if (ctx == NULL || ctx->off == NULL || out == NULL || capacity <= 0) {
        return 0;
    }

    if (!chain_read_tarray(ctx, tarray_addr, &actors_data, &actors_count)) {
        return 0;
    }
    if (actors_count <= 0) {
        return 0;
    }

    to_read = actors_count;
    if (to_read > OJBS_MAX_ACTORS) {
        to_read = OJBS_MAX_ACTORS;
    }
    if (to_read > capacity) {
        to_read = capacity;
    }

    memset(pointers, 0, sizeof(pointers));

    if (!mem_read(&ctx->target, actors_data, pointers,
                  (size_t)to_read * sizeof(uint64_t))) {
        return 0;
    }

    for (i = 0; i < to_read && found < capacity; ++i) {
        uint64_t actor = pointers[i];
        vec3 position;
        vec3 delta;
        double distance;

        if (actor == 0 || actor == skip_actor) {
            continue;
        }
        if (!mem_ptr_plausible(actor)) {
            continue;
        }
        if (!chain_actor_position(ctx, actor, &position)) {
            continue;
        }

        delta = vec3_sub(position, camera_position);
        distance = vec3_length(delta);

        if (distance > max_distance) {
            continue;
        }

        out[found].actor = actor;
        out[found].position = position;
        out[found].distance = distance;
        found += 1;
    }

    return found;
}

#define FT_SCAN_MAX 0xC00u

bool chain_find_ftransform_in_rc(const chain_ctx *ctx, uint64_t rc_addr,
                                 uint32_t scan_limit,
                                 uint32_t *out_translation_off,
                                 bool *out_is_float,
                                 vec3 *out_pos)
{
    uint8_t buf[FT_SCAN_MAX];
    uint32_t to_read;
    uint32_t off;
    (void)0;

    if (ctx == NULL || rc_addr == 0 || out_translation_off == NULL) {
        return false;
    }

    to_read = (scan_limit > FT_SCAN_MAX) ? FT_SCAN_MAX : scan_limit;
    if (!mem_read(&ctx->target, rc_addr, buf, to_read)) {
        return false;
    }

    for (off = 0x20; off + 0x18 <= to_read; off += 8) {
        double sd[3];
        memcpy(sd, buf + off, 24);
        if (!isfinite(sd[0]) || !isfinite(sd[1]) || !isfinite(sd[2])) {
            continue;
        }
        if (fabs(sd[0] - 1.0) < 0.01
            && fabs(sd[1] - 1.0) < 0.01
            && fabs(sd[2] - 1.0) < 0.01) {
            uint32_t t_off;
            double xyz[3];
            vec3 pos;
            if (off < 0x18) continue;
            t_off = off - 0x18;
            memcpy(xyz, buf + t_off, 24);
            if (!isfinite(xyz[0]) || !isfinite(xyz[1]) || !isfinite(xyz[2])) {
                continue;
            }
            pos = vec3_make(xyz[0], xyz[1], xyz[2]);
            if (vec3_length(pos) > 100.0 && vec3_plausible(pos, 2e7)) {
                *out_translation_off = t_off;
                *out_is_float = false;
                if (out_pos) *out_pos = pos;
                return true;
            }
        }
    }

    for (off = 0x10; off + 0x0C <= to_read; off += 4) {
        float sf[3];
        memcpy(sf, buf + off, 12);
        if (!isfinite(sf[0]) || !isfinite(sf[1]) || !isfinite(sf[2])) {
            continue;
        }
        if (fabsf(sf[0] - 1.0f) < 0.01f
            && fabsf(sf[1] - 1.0f) < 0.01f
            && fabsf(sf[2] - 1.0f) < 0.01f) {
            uint32_t t_off;
            float fxyz[3];
            vec3 pos;
            if (off < 0x0C) continue;
            t_off = off - 0x0C;
            memcpy(fxyz, buf + t_off, 12);
            if (!isfinite(fxyz[0]) || !isfinite(fxyz[1]) || !isfinite(fxyz[2])) {
                continue;
            }
            pos = vec3_make((double)fxyz[0], (double)fxyz[1], (double)fxyz[2]);
            if (vec3_length(pos) > 100.0 && vec3_plausible(pos, 2e7)) {
                *out_translation_off = t_off;
                *out_is_float = true;
                if (out_pos) *out_pos = pos;
                return true;
            }
        }
    }

    return false;
}

#include "chain.h"

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

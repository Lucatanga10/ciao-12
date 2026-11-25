#include "offsets.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const ue_offsets g_default = {
    /* [build identity | Frida probe 43.11 Android | 2026-11-25] */
    .build_tag = "43.11-Android",
    .dump_date = "2026-11-25",

    .module_name = "libUnreal.so",

    /* [rva 0x19cec360 | build 43.11 | Frida probe 2026-11-25 | verified live] */
    .gworld_rva = 0x19cec360,
    .has_gworld_rva = true,

    /* [field offset | Frida probe 2026-11-25 | verified live]
     * W+0x20->Level, W+0x170->GI confermati dalla catena GI->LP->PC->PCM->FOV=79.5 */
    .uworld_persistent_level = 0x20,
    .uworld_net_driver = 0,
    .uworld_game_state = 0,
    .uworld_levels = 0,
    .uworld_owning_game_instance = 0x170,
    .has_uworld_persistent_level = true,
    .has_uworld_net_driver = false,
    .has_uworld_game_state = false,
    .has_uworld_levels = false,
    .has_uworld_owning_game_instance = true,

    /* [field offset | Frida probe 2026-11-25 | verified live]
     * GI+0x168->TArray LP, confermato: 1 player, LP[0] valido */
    .ugameinstance_local_players = 0x168,
    .has_ugameinstance_local_players = true,

    /* [field offset | Frida probe 2026-11-25 | verified live]
     * LP+0x20->PC, confermato tramite PC->PCM->FOV=79.5 */
    .ulocalplayer_player_controller = 0x20,
    .has_ulocalplayer_player_controller = true,

    /* [field offset | Frida probe 2026-11-25 | verified live / encrypted]
     * PC+0x80 punta al pawn (match con actors[213] in probe7) ma il
     * puntatore e' cifrato da Epic: legge 0x1a0000001a. Self-filter
     * disabilitato per ora.
     * PC+0x338->PCM confermato da FOV=79.5 a PCM+0x15B0 */
    .apc_acknowledged_pawn = 0,
    .apc_pawn = 0,
    .apc_player_camera_manager = 0x338,
    .apc_player_state = 0,
    .has_apc_acknowledged_pawn = false,
    .has_apc_pawn = false,
    .has_apc_player_camera_manager = true,
    .has_apc_player_state = false,

    /* [field offset | Frida probe 2026-11-25 | verified live]
     * PCM+0x1580=CamLoc(3d), PCM+0x1598=CamRot(3d), PCM+0x15B0=FOV(f32)=79.49
     * Zero nella lobby, popolato in partita. */
    .apcm_camera_cache_private = 0x1580,
    .apcm_pov_in_cache_entry = 0x0,
    .has_apcm_camera_cache_private = true,
    .has_apcm_pov_in_cache_entry = true,

    .fmvi_location = 0x0,
    .fmvi_rotation = 0x18,
    .fmvi_fov = 0x30,
    .has_fmvi_location = true,
    .has_fmvi_rotation = true,
    .has_fmvi_fov = true,

    /* [field offset | Frida probe 2026-11-25 | verified live]
     * Level+0xe0->TArray actors, confermato: 280 attori in lobby */
    .ulevel_actors = 0xe0,
    .ulevel_owning_world = 0,
    .has_ulevel_actors = true,
    .has_ulevel_owning_world = false,

    /* [field offset | HYPOTHESIS | non verificato: lobby non popola posizioni]
     * SDK dump 43.10 dice 0x1B0, community Windows dice 0x130.
     * Fallback auto-scan a runtime prova entrambi + range 0x100-0x220. */
    .aactor_root_component = 0x1B0,
    .has_aactor_root_component = true,

    /* [field offset | HYPOTHESIS | non verificato: lobby non popola posizioni]
     * SDK dump 43.10 dice 0x140 (double), community Windows dice 0x11C (float).
     * Fallback auto-scan a runtime prova entrambi i tipi. */
    .uscene_relative_location = 0x140,
    .uscene_relative_rotation = 0x158,
    .has_uscene_relative_location = true,
    .has_uscene_relative_rotation = true,
    .uscene_location_is_float = false,

    .apawn_player_state = 0,
    .aps_team_index = 0,
    .aps_is_a_bot = 0,
    .has_apawn_player_state = false,
    .has_aps_team_index = false,
    .has_aps_is_a_bot = false,

    /* [layout | derivato dalla size 0x10 dei TArray nel dump | validato a
     * runtime con num>0 && num<=max && data plausibile] */
    .tarray_data = 0x0,
    .tarray_num = 0x8,
    .tarray_max = 0xC,
    .has_tarray = true,

    /* [layout | dump SDK 2026-11-24 | confermato: UE5 LWC, tre double] */
    .fvector_size = 0x18,
    .fvector_is_double = true,
    .has_fvector = true,

    /* [layout | dump SDK 2026-11-24 | confermato: tre double] */
    .frotator_size = 0x18,
    .frotator_is_double = true,
    .has_frotator = true,

    .pawn_height = 180.0
};

typedef struct {
    const char *label;
    size_t flag_offset;
} dep_entry;

#define DEP(field) { #field, offsetof(ue_offsets, field) }

static const dep_entry g_deps_actor_walk[] = {
    DEP(has_gworld_rva),
    DEP(has_uworld_persistent_level),
    DEP(has_ulevel_actors),
    DEP(has_tarray)
};

static const dep_entry g_deps_actor_position[] = {
    DEP(has_aactor_root_component),
    DEP(has_uscene_relative_location),
    DEP(has_fvector)
};

static const dep_entry g_deps_local_camera[] = {
    DEP(has_gworld_rva),
    DEP(has_uworld_owning_game_instance),
    DEP(has_ugameinstance_local_players),
    DEP(has_ulocalplayer_player_controller),
    DEP(has_apc_player_camera_manager),
    DEP(has_apcm_camera_cache_private),
    DEP(has_apcm_pov_in_cache_entry),
    DEP(has_fmvi_location),
    DEP(has_fmvi_rotation),
    DEP(has_fmvi_fov),
    DEP(has_fvector),
    DEP(has_frotator),
    DEP(has_tarray)
};

static const dep_entry g_deps_self_filter[] = {
    DEP(has_apc_acknowledged_pawn)
};

static const dep_entry g_deps_team_filter[] = {
    DEP(has_apawn_player_state),
    DEP(has_aps_team_index),
    DEP(has_aps_is_a_bot)
};

#undef DEP

typedef struct {
    const char *name;
    const dep_entry *deps;
    size_t count;
} feature_entry;

static const feature_entry g_features[FEAT_COUNT] = {
    { "iterazione lista attori", g_deps_actor_walk,
      sizeof(g_deps_actor_walk) / sizeof(g_deps_actor_walk[0]) },
    { "posizione mondo per attore", g_deps_actor_position,
      sizeof(g_deps_actor_position) / sizeof(g_deps_actor_position[0]) },
    { "POV camera locale", g_deps_local_camera,
      sizeof(g_deps_local_camera) / sizeof(g_deps_local_camera[0]) },
    { "esclusione pawn locale", g_deps_self_filter,
      sizeof(g_deps_self_filter) / sizeof(g_deps_self_filter[0]) },
    { "filtro team e bot", g_deps_team_filter,
      sizeof(g_deps_team_filter) / sizeof(g_deps_team_filter[0]) }
};

const ue_offsets *offsets_default(void)
{
    return &g_default;
}

const char *offsets_feature_name(ue_feature feature)
{
    if (feature < 0 || feature >= FEAT_COUNT) {
        return "feature sconosciuta";
    }
    return g_features[feature].name;
}

static bool flag_at(const ue_offsets *off, size_t flag_offset)
{
    const unsigned char *base = (const unsigned char *)off;
    bool value = false;
    memcpy(&value, base + flag_offset, sizeof(value));
    return value;
}

bool offsets_feature_ready(const ue_offsets *off, ue_feature feature,
                           char *missing, size_t missing_size)
{
    size_t i;
    size_t written = 0;
    bool ready = true;

    if (missing != NULL && missing_size > 0) {
        missing[0] = '\0';
    }

    if (off == NULL || feature < 0 || feature >= FEAT_COUNT) {
        return false;
    }

    for (i = 0; i < g_features[feature].count; ++i) {
        const dep_entry *dep = &g_features[feature].deps[i];
        if (flag_at(off, dep->flag_offset)) {
            continue;
        }

        ready = false;

        if (missing != NULL && missing_size > 0) {
            const char *separator = (written > 0) ? ", " : "";
            int n = snprintf(missing + written, missing_size - written,
                             "%s%s", separator, dep->label);
            if (n > 0 && (size_t)n < missing_size - written) {
                written += (size_t)n;
            } else {
                written = missing_size - 1;
            }
        }
    }

    return ready;
}

int offsets_report(const ue_offsets *off)
{
    char missing[512];
    int ready_count = 0;
    int i;

    if (off == NULL) {
        fprintf(stderr, "[config] nessuna config\n");
        return 0;
    }

    if (off->build_tag == NULL) {
        fprintf(stderr, "[config] build non dichiarata - ogni offset e' una ipotesi "
                        "legata al dump del %s\n",
                (off->dump_date != NULL) ? off->dump_date : "?");
    } else {
        fprintf(stderr, "[config] build: %s\n", off->build_tag);
    }

    fprintf(stderr, "[config] modulo atteso: %s\n",
            (off->module_name != NULL) ? off->module_name : "?");
    fprintf(stderr, "[config] pawn_height: %.1f (UNCALIBRATED)\n", off->pawn_height);

    for (i = 0; i < FEAT_COUNT; ++i) {
        if (offsets_feature_ready(off, (ue_feature)i, missing, sizeof(missing))) {
            fprintf(stderr, "[ok]     %s\n", g_features[i].name);
            ready_count += 1;
        } else {
            fprintf(stderr, "[gated]  %s - mancano: %s\n", g_features[i].name, missing);
        }
    }

    fprintf(stderr, "[config] feature pronte: %d/%d\n", ready_count, (int)FEAT_COUNT);
    return ready_count;
}

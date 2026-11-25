#include "offsets.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const ue_offsets g_default = {
    /* [build identity | letta dal pacchetto installato 2026-11-24]
     * versionName 43.10.0-60118918-Android, versionCode 60118918
     * libUnreal.so GNU build-id 686f70d3f37d72bcf20d3f51592b244792187211
     * NOTA: gli offset qui sotto vengono da un dump SDK la cui build NON e'
     * dichiarata. Finche' non combacia con questa riga restano ipotesi. */
    .build_tag = "43.10.0-60118918-Android",
    .dump_date = "2026-11-24",

    .module_name = "libUnreal.so",

    .gworld_rva = 0,
    .has_gworld_rva = false,

    .uworld_persistent_level = 0x40,
    .uworld_net_driver = 0x48,
    .uworld_game_state = 0x1D0,
    .uworld_levels = 0x1E8,
    .uworld_owning_game_instance = 0x248,
    .has_uworld_persistent_level = true,
    .has_uworld_net_driver = true,
    .has_uworld_game_state = true,
    .has_uworld_levels = true,
    .has_uworld_owning_game_instance = true,

    /* [field offset | dump SDK 2026-11-24 | non verificato live] */
    .ugameinstance_local_players = 0x38,
    .has_ugameinstance_local_players = true,

    /* UPlayer::PlayerController non e' riflesso: ULocalPlayer nel dump parte
     * a 0x48 con UnknownData. Si deriva a runtime nella finestra 0x28..0x48
     * cercando il puntatore il cui CameraManager torna indietro a lui. */
    .ulocalplayer_player_controller = 0,
    .has_ulocalplayer_player_controller = false,

    /* [field offset | dump SDK 2026-11-24 | non verificato live] */
    .apc_acknowledged_pawn = 0x318,
    .apc_pawn = 0,
    .apc_player_camera_manager = 0x328,
    .apc_player_state = 0,
    .has_apc_acknowledged_pawn = true,
    .has_apc_pawn = false,
    .has_apc_player_camera_manager = true,
    .has_apc_player_state = false,

    /* [field offset | dump SDK 2026-11-24 | non verificato live]
     * POV sta a 0x10 dentro FCameraCacheEntry, che e' a 0x1590. */
    .apcm_camera_cache_private = 0x1590,
    .apcm_pov_in_cache_entry = 0x10,
    .has_apcm_camera_cache_private = true,
    .has_apcm_pov_in_cache_entry = true,

    /* [field offset | dump SDK 2026-11-24 | non verificato live]
     * FMinimalViewInfo size 0x910: Location FVector 0x0, Rotation FRotator
     * 0x18, FOV float 0x30. */
    .fmvi_location = 0x0,
    .fmvi_rotation = 0x18,
    .fmvi_fov = 0x30,
    .has_fmvi_location = true,
    .has_fmvi_rotation = true,
    .has_fmvi_fov = true,

    .ulevel_actors = 0,
    .ulevel_owning_world = 0xF0,
    .has_ulevel_actors = false,
    .has_ulevel_owning_world = true,

    /* [field offset | dump SDK 2026-11-24 | non verificato live] */
    .aactor_root_component = 0x1B0,
    .has_aactor_root_component = true,

    /* [field offset | dump SDK 2026-11-24 | non verificato live] */
    .uscene_relative_location = 0x140,
    .uscene_relative_rotation = 0x158,
    .has_uscene_relative_location = true,
    .has_uscene_relative_rotation = true,

    /* [field offset | dump SDK 2026-11-24 | non verificato live]
     * aps_is_a_bot: il browser collassa i bitfield e stampa ogni bit come
     * ":0", quindi il byte e' 0x27a ma la posizione del bit dentro il byte
     * NON e' nota: va confermata a video prima di usarla come filtro. */
    .apawn_player_state = 0x290,
    .aps_team_index = 0,
    .aps_is_a_bot = 0x27A,
    .has_apawn_player_state = true,
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

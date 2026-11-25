#ifndef OJBS_OFFSETS_H
#define OJBS_OFFSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Config block unico. Ogni valore porta la sua provenienza sulla riga in cui
 * vive. I valori non confermati restano 0 con il flag has_ a false, e la
 * feature che li usa resta gated: il resto del binario gira lo stesso.
 *
 * Tre tier, mai promossi per comodita':
 *   confirmed - letto dal dump di Olivia
 *   derived   - calcolato da una struttura confermata
 *   unknown   - ricetta spedita, feature gated
 */

typedef struct {
    /* identita' build -------------------------------------------------- */
    const char *build_tag;   /* NULL = non dichiarata */
    const char *dump_date;

    /* modulo -----------------------------------------------------------
     * [module name | build installato 2026-11-14 | lib/arm64 dell'APK
     *  | visto su disco 2026-11-24, non ancora visto mappato]
     */
    const char *module_name;

    /* [rva | UNKNOWN] indirizzo di GWorld relativo alla base del modulo */
    uint64_t gworld_rva;
    bool has_gworld_rva;

    /* UWorld ------------------------------------------------------------
     * [field offset | dump UWorld incollato 2026-11-23 | build non dichiarata
     *  | non verificato live]
     */
    uint32_t uworld_persistent_level;      /* 0x40  */
    uint32_t uworld_net_driver;            /* 0x48  */
    uint32_t uworld_game_state;            /* 0x1D0 */
    uint32_t uworld_levels;                /* 0x1E8 */
    uint32_t uworld_owning_game_instance;  /* 0x248 */
    bool has_uworld_persistent_level;
    bool has_uworld_net_driver;
    bool has_uworld_game_state;
    bool has_uworld_levels;
    bool has_uworld_owning_game_instance;

    /* UGameInstance  [field offset | UNKNOWN] */
    uint32_t ugameinstance_local_players;
    bool has_ugameinstance_local_players;

    /* ULocalPlayer / UPlayer  [field offset | UNKNOWN] */
    uint32_t ulocalplayer_player_controller;
    bool has_ulocalplayer_player_controller;

    /* APlayerController  [field offset | UNKNOWN] */
    uint32_t apc_acknowledged_pawn;
    uint32_t apc_pawn;
    uint32_t apc_player_camera_manager;
    uint32_t apc_player_state;
    bool has_apc_acknowledged_pawn;
    bool has_apc_pawn;
    bool has_apc_player_camera_manager;
    bool has_apc_player_state;

    /* APlayerCameraManager  [field offset | UNKNOWN] */
    uint32_t apcm_camera_cache_private;
    uint32_t apcm_pov_in_cache_entry;
    bool has_apcm_camera_cache_private;
    bool has_apcm_pov_in_cache_entry;

    /* FMinimalViewInfo  [field offset | UNKNOWN] */
    uint32_t fmvi_location;
    uint32_t fmvi_rotation;
    uint32_t fmvi_fov;
    bool has_fmvi_location;
    bool has_fmvi_rotation;
    bool has_fmvi_fov;

    /* ULevel
     * Actors non e' una UPROPERTY su UE5: nel dump cade dentro
     * UnknownData_000028[0xC8], quindi resta unknown e si deriva a runtime
     * cercando nella finestra 0x28..0xf0 il TArray i cui elementi hanno un
     * RootComponent valido.
     * OwningWorld [field offset | dump SDK 2026-11-24 | NON confermato live:
     * l'invariante L->OwningWorld == W non si e' chiusa su nessun candidato]
     */
    uint32_t ulevel_actors;
    uint32_t ulevel_owning_world;
    bool has_ulevel_actors;
    bool has_ulevel_owning_world;

    /* AActor  [field offset | dump SDK 2026-11-24 | non verificato live] */
    uint32_t aactor_root_component;
    bool has_aactor_root_component;

    /* USceneComponent  [field offset | dump SDK 2026-11-24 | non verificato live] */
    uint32_t uscene_relative_location;
    uint32_t uscene_relative_rotation;
    bool has_uscene_relative_location;
    bool has_uscene_relative_rotation;

    /* APawn / APlayerState  [field offset | UNKNOWN] */
    uint32_t apawn_player_state;
    uint32_t aps_team_index;
    uint32_t aps_is_a_bot;
    bool has_apawn_player_state;
    bool has_aps_team_index;
    bool has_aps_is_a_bot;

    /* TArray  [layout | UNKNOWN] attesi 0x0 / 0x8 / 0xC, size 0x10 */
    uint32_t tarray_data;
    uint32_t tarray_num;
    uint32_t tarray_max;
    bool has_tarray;

    /* FVector  [layout | UNKNOWN]
     * UE5 con LWC: tre double, size 0x18.  UE4: tre float, size 0xC.
     * Sbagliarlo non crasha e non da' sintomi: va letto dal dump.
     */
    uint32_t fvector_size;
    bool fvector_is_double;
    bool has_fvector;

    /* FRotator  [layout | UNKNOWN] stesso discorso di FVector */
    uint32_t frotator_size;
    bool frotator_is_double;
    bool has_frotator;

    /* costante di taratura, non un offset: altezza del box in unita' mondo.
     * UNCALIBRATED finche' non la confronti a schermo.
     */
    double pawn_height;
} ue_offsets;

typedef enum {
    FEAT_ACTOR_WALK = 0,
    FEAT_ACTOR_POSITION,
    FEAT_LOCAL_CAMERA,
    FEAT_SELF_FILTER,
    FEAT_TEAM_FILTER,
    FEAT_COUNT
} ue_feature;

/* Config di default: solo UWorld confermato, tutto il resto gated. */
const ue_offsets *offsets_default(void);

const char *offsets_feature_name(ue_feature feature);

/* true se ogni dipendenza della feature ha il suo has_ a true.
 * missing, se non NULL, riceve la lista dei campi mancanti separati da ", ".
 */
bool offsets_feature_ready(const ue_offsets *off, ue_feature feature,
                           char *missing, size_t missing_size);

/* Stampa su stderr lo stato di ogni feature. Torna il numero di feature pronte. */
int offsets_report(const ue_offsets *off);

#endif /* OJBS_OFFSETS_H */

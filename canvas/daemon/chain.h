#ifndef OJBS_CHAIN_H
#define OJBS_CHAIN_H

#include "memread.h"
#include "offsets.h"
#include "vecmath.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Percorso UE, un hop per funzione. Ogni funzione valida quello che legge e
 * torna false invece di propagare spazzatura. Nessuna funzione qui dentro
 * scrive un byte nel processo target.
 */

#define OJBS_MAX_ACTORS 512

typedef struct {
    mem_target target;
    const ue_offsets *off;
} chain_ctx;

typedef struct {
    uint64_t actor;
    vec3 position;
    double distance;
} chain_actor;

typedef struct {
    vec3 position;
    rot3 rotation;
    double fov;
} chain_camera;

/* Legge un FVector rispettando fvector_is_double. */
bool chain_read_vector(const chain_ctx *ctx, uint64_t address, vec3 *out);

/* Legge un FRotator rispettando frotator_is_double. */
bool chain_read_rotator(const chain_ctx *ctx, uint64_t address, rot3 *out);

/* Legge un TArray: puntatore ai dati e conteggio, gia' validati. */
bool chain_read_tarray(const chain_ctx *ctx, uint64_t address,
                       uint64_t *out_data, int32_t *out_count);

/* GWorld = base modulo + rva, dereferenziato una volta. */
bool chain_resolve_world(const chain_ctx *ctx, uint64_t *out_world);

/* PlayerController locale, passando per GameInstance e LocalPlayers[0]. */
bool chain_local_controller(const chain_ctx *ctx, uint64_t world,
                            uint64_t *out_controller);

/* AcknowledgedPawn del controller locale: serve solo a escludersi. */
bool chain_local_pawn(const chain_ctx *ctx, uint64_t controller,
                      uint64_t *out_pawn);

/* POV dalla CameraCache del PlayerCameraManager. */
bool chain_read_camera(const chain_ctx *ctx, uint64_t controller,
                       chain_camera *out);

/* Posizione mondo di un attore via RootComponent. */
bool chain_actor_position(const chain_ctx *ctx, uint64_t actor, vec3 *out);

/*
 * Riempie out con gli attori di PersistentLevel che hanno una posizione
 * leggibile e plausibile. skip_actor viene escluso. Torna quanti ne ha messi.
 */
int chain_collect_actors(const chain_ctx *ctx, uint64_t world,
                         uint64_t skip_actor, vec3 camera_position,
                         double max_distance,
                         chain_actor *out, int capacity);

/*
 * Auto-scan: prova combinazioni di RootComponent e RelativeLocation su 20
 * attori del PersistentLevel. Torna true quando almeno 3 attori hanno
 * posizioni diverse (distanza > 100 unita'). Scrive gli offset trovati.
 * Funziona solo in partita: in lobby le posizioni sono tutte zero.
 */
bool chain_autoscan_position(const chain_ctx *ctx, uint64_t world,
                             uint32_t *out_rc_off, uint32_t *out_loc_off,
                             bool *out_loc_is_float);

/*
 * Come chain_autoscan_position ma su un livello specifico, non PersistentLevel.
 * Usato per scandire i livelli di streaming alla ricerca di attori con posizioni.
 */
bool chain_autoscan_position_level(const chain_ctx *ctx, uint64_t level,
                                   uint32_t *out_rc_off, uint32_t *out_loc_off,
                                   bool *out_loc_is_float);

/*
 * Cerca l'offset di UWorld::Levels scandendo world+0x28..0x300 per un TArray
 * che contiene il PersistentLevel gia' noto. Forte: il match con un puntatore
 * confermato elimina i falsi positivi.
 */
bool chain_scan_levels_offset(const chain_ctx *ctx, uint64_t world,
                              uint32_t *out_levels_offset);

/*
 * Raccoglie attori da un singolo livello (non necessariamente PersistentLevel).
 */
int chain_collect_actors_from_level(const chain_ctx *ctx, uint64_t level,
                                    uint64_t skip_actor, vec3 camera_position,
                                    double max_distance,
                                    chain_actor *out, int capacity);

/*
 * Raccoglie attori da TUTTI i livelli nel TArray UWorld::Levels.
 */
int chain_collect_actors_all_levels(const chain_ctx *ctx, uint64_t world,
                                    uint32_t levels_offset,
                                    uint64_t skip_actor, vec3 camera_position,
                                    double max_distance,
                                    chain_actor *out, int capacity);

/*
 * Scandisce il PCM alla ricerca di dati camera non-zero: 3 double (posizione) +
 * 3 double (rotazione) a +0x18 + float FOV a +0x30. Torna l'offset dentro il PCM.
 */
bool chain_scan_camera_offset(const chain_ctx *ctx, uint64_t pcm,
                              uint32_t *out_cache_offset);

/*
 * Raccoglie attori da un TArray arbitrario (non da un ULevel).
 * tarray_addr punta al primo campo del TArray (data, num, max).
 */
int chain_collect_actors_from_tarray(const chain_ctx *ctx,
                                     uint64_t tarray_addr,
                                     uint64_t skip_actor,
                                     vec3 camera_position,
                                     double max_distance,
                                     chain_actor *out, int capacity);

/*
 * Cerca FTransform in un RootComponent scandendo per Scale3D = (1.0, 1.0, 1.0).
 * scan_limit byte letti dal componente; massimo 0xC00.
 * Torna true e scrive l'offset della Translation dentro il RC, piu' la posizione
 * letta come validazione. is_float true se il match era su float, non double.
 */
bool chain_find_ftransform_in_rc(const chain_ctx *ctx, uint64_t rc_addr,
                                 uint32_t scan_limit,
                                 uint32_t *out_translation_off,
                                 bool *out_is_float,
                                 vec3 *out_pos);

bool chain_find_guobjectarray(const chain_ctx *ctx,
                              uint64_t gworld_rva,
                              uint64_t *out_objects_addr);

bool chain_resolve_fobjectptr(const chain_ctx *ctx,
                              uint64_t guobj_objects,
                              uint64_t handle,
                              uint64_t *out_ptr);

int chain_collect_via_guobj(const chain_ctx *ctx,
                            uint64_t world,
                            uint64_t guobj_objects,
                            uint32_t rc_off,
                            uint32_t loc_off,
                            bool loc_float,
                            uint64_t skip_actor,
                            vec3 camera_position,
                            double max_distance,
                            chain_actor *out, int capacity);

#endif /* OJBS_CHAIN_H */

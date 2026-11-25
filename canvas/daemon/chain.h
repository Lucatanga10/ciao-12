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

#endif /* OJBS_CHAIN_H */

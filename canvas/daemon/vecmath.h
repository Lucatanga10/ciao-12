#ifndef OJBS_VECMATH_H
#define OJBS_VECMATH_H

#include <stdbool.h>

/*
 * Convenzione, dichiarata una volta e valida per tutto il file:
 *   - sistema Unreal: X avanti, Y destra, Z su, mano sinistra
 *   - FRotator in GRADI, ordine pitch (Y), yaw (Z), roll (X)
 *   - FOV orizzontale in gradi, come lo tiene FMinimalViewInfo
 *   - origine schermo in alto a sinistra, Y cresce verso il basso
 *   - tutto in double: FVector su UE5 con LWC e' gia' double, su UE4 si
 *     promuove in lettura e la matematica resta una sola
 */

typedef struct {
    double x;
    double y;
    double z;
} vec3;

typedef struct {
    double pitch;
    double yaw;
    double roll;
} rot3;

typedef struct {
    vec3 forward;
    vec3 right;
    vec3 up;
} basis3;

typedef struct {
    double x;
    double y;
} vec2;

vec3 vec3_make(double x, double y, double z);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_add(vec3 a, vec3 b);
double vec3_dot(vec3 a, vec3 b);
double vec3_length(vec3 a);
bool vec3_finite(vec3 a);

/* Plausibilita' di una coordinata mondo: il mondo di UE e' grande ma non
 * infinito, e una lettura sbagliata produce quasi sempre numeri assurdi.
 */
bool vec3_plausible(vec3 a, double limit);

/* Equivalente di FRotationMatrix: righe X avanti, Y destra, Z su. */
basis3 rot_to_basis(rot3 rotation);

/*
 * World to screen. Torna false quando il punto sta dietro la camera o quando
 * uno degli ingressi non e' finito: in quel caso out non viene toccato.
 * screen_w e screen_h in pixel; la lunghezza focale viene da screen_w, che e'
 * il motivo per cui il FOV in ingresso dev'essere quello orizzontale.
 */
bool world_to_screen(vec3 world, vec3 camera_position, basis3 camera_basis,
                     double fov_degrees, double screen_w, double screen_h,
                     vec2 *out);

#endif /* OJBS_VECMATH_H */

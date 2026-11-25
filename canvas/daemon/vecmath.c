#include "vecmath.h"

#include <math.h>
#include <stddef.h>

#define OJBS_DEG2RAD 0.017453292519943295

vec3 vec3_make(double x, double y, double z)
{
    vec3 out;
    out.x = x;
    out.y = y;
    out.z = z;
    return out;
}

vec3 vec3_sub(vec3 a, vec3 b)
{
    return vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3 vec3_add(vec3 a, vec3 b)
{
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

double vec3_dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double vec3_length(vec3 a)
{
    return sqrt(vec3_dot(a, a));
}

bool vec3_finite(vec3 a)
{
    return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
}

bool vec3_plausible(vec3 a, double limit)
{
    if (!vec3_finite(a)) {
        return false;
    }
    if (fabs(a.x) > limit || fabs(a.y) > limit || fabs(a.z) > limit) {
        return false;
    }
    return true;
}

basis3 rot_to_basis(rot3 rotation)
{
    const double pitch = rotation.pitch * OJBS_DEG2RAD;
    const double yaw = rotation.yaw * OJBS_DEG2RAD;
    const double roll = rotation.roll * OJBS_DEG2RAD;

    const double cp = cos(pitch);
    const double sp = sin(pitch);
    const double cy = cos(yaw);
    const double sy = sin(yaw);
    const double cr = cos(roll);
    const double sr = sin(roll);

    basis3 out;

    out.forward = vec3_make(cp * cy, cp * sy, sp);

    out.right = vec3_make(sr * sp * cy - cr * sy,
                          sr * sp * sy + cr * cy,
                          -sr * cp);

    out.up = vec3_make(-(cr * sp * cy + sr * sy),
                       cy * sr - cr * sp * sy,
                       cr * cp);

    return out;
}

bool world_to_screen(vec3 world, vec3 camera_position, basis3 camera_basis,
                     double fov_degrees, double screen_w, double screen_h,
                     vec2 *out)
{
    vec3 delta;
    double depth;
    double tan_half;
    double focal;
    double offset_right;
    double offset_up;
    vec2 result;

    if (out == NULL) {
        return false;
    }
    if (!vec3_finite(world) || !vec3_finite(camera_position)) {
        return false;
    }
    if (!isfinite(fov_degrees) || fov_degrees <= 1.0 || fov_degrees >= 179.0) {
        return false;
    }
    if (!(screen_w > 0.0) || !(screen_h > 0.0)) {
        return false;
    }
    if (!vec3_finite(camera_basis.forward) ||
        !vec3_finite(camera_basis.right) ||
        !vec3_finite(camera_basis.up)) {
        return false;
    }

    delta = vec3_sub(world, camera_position);
    depth = vec3_dot(delta, camera_basis.forward);

    /* Dietro la camera, o cosi' vicino che la divisione esplode. */
    if (!(depth > 1.0)) {
        return false;
    }

    tan_half = tan(fov_degrees * 0.5 * OJBS_DEG2RAD);
    if (!isfinite(tan_half) || tan_half <= 0.0) {
        return false;
    }

    focal = (screen_w * 0.5) / tan_half;

    offset_right = vec3_dot(delta, camera_basis.right);
    offset_up = vec3_dot(delta, camera_basis.up);

    result.x = (screen_w * 0.5) + offset_right * focal / depth;
    result.y = (screen_h * 0.5) - offset_up * focal / depth;

    if (!isfinite(result.x) || !isfinite(result.y)) {
        return false;
    }

    *out = result;
    return true;
}

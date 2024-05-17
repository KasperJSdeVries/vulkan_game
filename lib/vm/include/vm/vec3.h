#ifndef VM_VEC3_H_INCLUDED
#define VM_VEC3_H_INCLUDED

#include "common.h"
#include "util.h"

VM_INLINE vec3 vm_vec3_zero(void) { return (vec3){{0.0f, 0.0f, 0.0f}}; }

VM_INLINE vec3 vm_vec3_one(void) { return (vec3){{1.0f, 1.0f, 1.0f}}; }

VM_INLINE vec3 vm_vec3_add(vec3 a, vec3 b) {
    return (vec3){
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z,
    };
}

VM_INLINE vec3 vm_vec3_sub(vec3 a, vec3 b) {
    return (vec3){
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
}

VM_INLINE vec3 vm_vec3_scale(vec3 v, f32 s) {
    return (vec3){
        .x = v.x * s,
        .y = v.y * s,
        .z = v.z * s,
    };
}

VM_INLINE f32 vm_vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

VM_INLINE f32 vm_vec3_magnitude2(vec3 v) { return vm_vec3_dot(v, v); }

VM_INLINE f32 vm_vec3_magnitude(vec3 v) { return sqrtf(vm_vec3_magnitude2(v)); }

VM_INLINE vec3 vm_vec3_normalize(vec3 v) {
    f32 magnitude = vm_vec3_magnitude(v);

    if (VM_UNLIKELY(magnitude < F32_EPSILON)) {
        return vm_vec3_zero();
    }

    return vm_vec3_scale(v, 1.0 / magnitude);
}

VM_INLINE vec3 vm_vec3_cross(vec3 a, vec3 b) {
    return (vec3){
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x,
    };
}

#endif // VM_VEC3_H_INCLUDED

#ifndef VM_CAM_H_INCLUDED
#define VM_CAM_H_INCLUDED

#include "common.h"
#include "mat4.h"
#include "vec3.h"

VM_INLINE mat4 vm_perspective_lh_zo(f32 fov_y, f32 aspect, f32 near_z, f32 far_z) {
    mat4 r = vm_mat4_zero();

    f32 f = 1.0f / tanf(fov_y * 0.5f);
    f32 fn = 1.0f / (near_z - far_z);

    r.m00 = f / aspect;
    r.m11 = f;
    r.m22 = -far_z * fn;
    r.m23 = 1.0f;
    r.m32 = near_z * far_z * fn;

    return r;
}

VM_INLINE mat4 vm_lookat_lh(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vm_vec3_normalize(vm_vec3_sub(center, eye));
    vec3 s = vm_vec3_normalize(vm_vec3_cross(up, f));
    vec3 u = vm_vec3_cross(f, s);

    return (mat4){
        .m00 = s.x,
        .m01 = u.x,
        .m02 = f.x,
        .m03 = 0.0f,
        .m10 = s.y,
        .m11 = u.y,
        .m12 = f.y,
        .m13 = 0.0f,
        .m20 = s.z,
        .m21 = u.z,
        .m22 = f.z,
        .m23 = 0.0f,
        .m30 = -vm_vec3_dot(s, eye),
        .m31 = -vm_vec3_dot(u, eye),
        .m32 = -vm_vec3_dot(f, eye),
        .m33 = 1.0f,
    };
}

#endif // VM_CAM_H_INCLUDED

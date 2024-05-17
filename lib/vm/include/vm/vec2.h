#ifndef VM_VEC2_H_INCLUDED
#define VM_VEC2_H_INCLUDED

#include "common.h"

VM_INLINE vec2 vm_vec2_divs(vec2 v, f32 s) {
    return (vec2){
        .x = v.x / s,
        .y = v.y / s,
    };
}

#endif // VM_VEC2_H_INCLUDED

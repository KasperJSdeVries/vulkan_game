#ifndef VM_MAT4_H_INCLUDED
#define VM_MAT4_H_INCLUDED

#include "common.h"

VM_INLINE mat4 vm_mat4_zero(void) {
    return (mat4){
        .col[0] = {{0.0f, 0.0f, 0.0f, 0.0f}},
        .col[1] = {{0.0f, 0.0f, 0.0f, 0.0f}},
        .col[2] = {{0.0f, 0.0f, 0.0f, 0.0f}},
        .col[3] = {{0.0f, 0.0f, 0.0f, 0.0f}},
    };
}

VM_INLINE mat4 vm_mat4_identity(void) {
    return (mat4){
        .col[0] = {{1.0f, 0.0f, 0.0f, 0.0f}},
        .col[1] = {{0.0f, 1.0f, 0.0f, 0.0f}},
        .col[2] = {{0.0f, 0.0f, 1.0f, 0.0f}},
        .col[3] = {{0.0f, 0.0f, 0.0f, 1.0f}},
    };
}

#endif // VM_MAT4_H_INCLUDED

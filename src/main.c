#include <stdio.h>

#include <cglm/struct.h>

int main() {
    printf("Hello, World!\n");

    mat4s mat = GLMS_MAT4_IDENTITY_INIT;
    mat4s inv = glms_mat4_inv(mat);
}

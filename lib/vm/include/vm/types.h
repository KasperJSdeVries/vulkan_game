#ifndef VM_TYPES_H_INCLUDED
#define VM_TYPES_H_INCLUDED

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#include <stdalign.h>
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef float f32;
typedef double f64;
typedef __float128 f128;

typedef _Bool b8;
typedef int b32;

#define true 1
#define false 0

#define F32_EPSILON 1.19209290e-7F
#define F64_EPSILON 2.2204460492503131e-16
#define F128_EPSILON FLT128_EPSILON

#if defined(__clang__) || defined(__GNUC__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 bytes.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

STATIC_ASSERT(sizeof(b8) == 1, "Expected b8 to be 1 bytes.");
STATIC_ASSERT(sizeof(b32) == 4, "Expected b32 to be 4 bytes.");

typedef union {
    i32 raw[2];
    struct {
        i32 x, y;
    };
} ivec2;

typedef union {
    i32 raw[3];
    struct {
        i32 x, y, z;
    };
} ivec3;

typedef union {
    i32 raw[4];
    struct {
        i32 x, y, z, w;
    };
} ivec4;

typedef union {
    f32 raw[2];
    struct {
        f32 x, y;
    };
} vec2;

typedef union {
    f32 raw[3];
    struct {
        f32 x, y, z;
    };
} vec3;

typedef union {
    f32 raw[4];
    struct {
        f32 x, y, z, w;
    };
} vec4;

typedef union {
    f64 raw[2];
    struct {
        f64 x, y;
    };
} dvec2;

typedef union {
    f64 raw[3];
    struct {
        f64 x, y, z;
    };
} dvec3;

typedef union {
    f64 raw[4];
    struct {
        f64 x, y, z, w;
    };
} dvec4;

typedef union {
    f128 raw[2];
    struct {
        f128 x, y;
    };
} qvec2;

typedef union {
    f128 raw[3];
    struct {
        f128 x, y, z;
    };
} qvec3;

typedef union {
    f128 raw[4];
    struct {
        f128 x, y, z, w;
    };
} qvec4;

typedef union {
    vec2 col[2];
    struct {
        f32 m00, m01;
        f32 m10, m11;
    };
} mat2;

typedef union {
    dvec2 col[2];
    struct {
        f64 m00, m01;
        f64 m10, m11;
    };
} dmat2;

typedef union {
    vec3 col[3];
    struct {
        f32 m00, m01, m02;
        f32 m10, m11, m12;
        f32 m20, m21, m22;
    };
} mat3;

typedef union {
    dvec3 col[3];
    struct {
        f64 m00, m01, m02;
        f64 m10, m11, m12;
        f64 m20, m21, m22;
    };
} dmat3;

typedef union {
    vec4 col[4];
    struct {
        f32 m00, m01, m02, m03;
        f32 m10, m11, m12, m13;
        f32 m20, m21, m22, m23;
        f32 m30, m31, m32, m33;
    };
} mat4;

typedef union {
    dvec4 col[4];
    struct {
        f64 m00, m01, m02, m03;
        f64 m10, m11, m12, m13;
        f64 m20, m21, m22, m23;
        f64 m30, m31, m32, m33;
    };
} dmat4;

#define VM_E 2.71828182845904523536028747135266250        /* e           */
#define VM_LOG2E 1.44269504088896340735992468100189214    /* log2(e)     */
#define VM_LOG10E 0.434294481903251827651128918916605082  /* log10(e)    */
#define VM_LN2 0.693147180559945309417232121458176568     /* loge(2)     */
#define VM_LN10 2.30258509299404568401799145468436421     /* loge(10)    */
#define VM_PI 3.14159265358979323846264338327950288       /* pi          */
#define VM_PI_2 1.57079632679489661923132169163975144     /* pi/2        */
#define VM_PI_4 0.785398163397448309615660845819875721    /* pi/4        */
#define VM_1_PI 0.318309886183790671537767526745028724    /* 1/pi        */
#define VM_2_PI 0.636619772367581343075535053490057448    /* 2/pi        */
#define VM_2_SQRTPI 1.12837916709551257389615890312154517 /* 2/sqrt(pi)  */
#define VM_SQRT2 1.41421356237309504880168872420969808    /* sqrt(2)     */
#define VM_SQRT1_2 0.707106781186547524400844362104849039 /* 1/sqrt(2)   */

#endif // VM_TYPES_H_INCLUDED

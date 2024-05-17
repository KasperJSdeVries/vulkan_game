#ifndef VM_UTIL_H_INCLUDED
#define VM_UTIL_H_INCLUDED

#include "common.h"

#define VM_RAD(deg) (deg * (typeof(deg))VM_PI / (typeof(deg))180)
#define VM_DEG(rad) (deg * (typeof(deg))180 / (typeof(deg))VM_PI)

#define VM_CLAMP(value, min, max) (value <= min ? min : (value >= max ? max : value))

#endif // VM_UTIL_H_INCLUDED

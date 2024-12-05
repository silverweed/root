#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usz;

typedef float f32;
typedef double f64;

#ifndef __cplusplus
typedef _Bool b8;
#else
typedef bool b8;
#endif
typedef int32_t b32;
typedef int b32x;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define countof(a) (sizeof(a) / sizeof((a)[0]))

#define Fae_Max(a, b) (((a) > (b)) ? (a) : (b))
#define Fae_Min(a, b) (((a) < (b)) ? (a) : (b))
#define Fae_Clamp(v, a, b) (Max((a), Min((v), (b))))
#define Fae_Abs(a) ((a) >= 0 ? (a) : (-a))
#define Fae_Square(a) ((a) * (a))

#ifdef __cplusplus
#define FAE_API extern "C"
#else
#define FAE_API
#endif

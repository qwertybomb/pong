#pragma once

extern int _fltused;
int _fltused = 0;

static inline float fabsf(float value)
{
    // NOTE: this is UB but it will not be a problem for most compilers
    *(uint32_t*)&value &= 0x7FFFFFFFu;
    return value;
}

static inline float fmaxf(float const a, float const b)
{
    return a < b ? b : a;
}

static inline float fminf(float const a, float const b)
{
    return a < b ? a : b;
}

static inline float rsqrtf(float const value)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(value)));
}

static inline float flerp(float const a, float const b, float const c)
{
    return a + c * (b - a);
}

#ifdef REAL_MSVC
#pragma function(sqrtf)
#endif
static inline float sqrtf(float const value)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
}

typedef struct float2
{
    float x, y;
} float2;

static inline float2 f2add2(float2 const a,
                            float2 const b)
{
    return (float2){a.x + b.x, a.y + b.y};
}

static inline float2 f2addf(float2 const a,
                            float const b)
{
    return (float2){a.x + b, a.y + b};
}

static inline float2 f2divf(float2 const a,
                            float const b)
{
    return (float2){a.x / b, a.y / b};
}

static inline float2 f2mulf(float2 const a,
                            float const b)
{
    return (float2){a.x * b, a.y * b};
}

static inline float2 f2sub2(float2 const a,
                            float2 const b)
{
    return (float2){a.x - b.x, a.y - b.y};
}

static inline float fdot2(float2 const a, float2 const b)
{
    return a.x * b.x + a.y * b.y;
}

static inline float filength2(float2 const value)
{
    return rsqrtf(value.x * value.x + value.y * value.y);
}

static inline float flength2(float2 const value)
{
    return sqrtf(value.x * value.x + value.y * value.y);
}

static inline float fclamp(float value, float min, float max)
{
    return fminf(fmaxf(value, min), max);
}

static inline float2 fclamp2(float2 const value, float2 const min, float2 const max)
{
    return (float2){fclamp(value.x, min.x, max.x), fclamp(value.y, min.y, max.y)};
}

static inline float2 fneg2(float2 const value)
{
    return (float2){-value.x, -value.y};
}

static inline float2 fnormalize2(float2 const value)
{
    return f2mulf(value, filength2(value));
}

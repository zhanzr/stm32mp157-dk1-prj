#ifndef CORE_PORTME_H
#define CORE_PORTME_H

#include <stdint.h>
#include <stddef.h>

#define HAS_FLOAT  1
#define HAS_TIME_H 0
#define USE_CLOCK  0
#define HAS_STDIO  1
#define HAS_PRINTF 1

#ifndef ITERATIONS
#define ITERATIONS 10000
#endif

typedef uint32_t CORE_TICKS;

#ifndef COMPILER_VERSION
#if defined(__clang__)
#define COMPILER_VERSION "Clang " __VERSION__
#elif defined(__GNUC__)
#define COMPILER_VERSION "GCC " __VERSION__
#else
#define COMPILER_VERSION "Unknown"
#endif
#endif

#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS "Ofast -ffp-contract=fast -funroll-all-loops"
#endif

#ifndef MEM_LOCATION
#define MEM_LOCATION "Static"
#endif

#ifndef MEM_METHOD
#define MEM_METHOD MEM_STATIC
#endif

typedef signed short ee_s16;
typedef unsigned short ee_u16;
typedef signed int ee_s32;
typedef double ee_f32;
typedef unsigned char ee_u8;
typedef unsigned int ee_u32;
typedef ee_u32 ee_ptr_int;
typedef size_t ee_size_t;

#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x) - 1) & ~3))

#ifndef SEED_METHOD
#define SEED_METHOD SEED_VOLATILE
#endif

#ifndef MULTITHREAD
#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK 0
#define USE_SOCKET 0
#endif

#ifndef MAIN_HAS_NOARGC
#define MAIN_HAS_NOARGC 1
#endif

#ifndef MAIN_HAS_NORETURN
#define MAIN_HAS_NORETURN 0
#endif

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S {
	ee_u8 portable_id;
} core_portable;

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && \
	!defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE == 1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE == 2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

#endif

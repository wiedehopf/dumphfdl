/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stddef.h>                 // offsetof
#include <stdio.h>                  // fprintf, stderr
#include <pthread.h>                // pthread_t, pthread_barrier_t
#include <stdlib.h>                 // calloc, realloc

// debug message classes
#define D_ALL                       (~0)
#define D_NONE                      (0)
#define D_SDR                       (1 <<  0)
#define D_DEMOD                     (1 <<  1)
#define D_DEMOD_DETAIL              (1 <<  2)
#define D_BURST                     (1 <<  3)
#define D_BURST_DETAIL              (1 <<  4)
#define D_PROTO                     (1 <<  5)
#define D_PROTO_DETAIL              (1 <<  6)
#define D_STATS                     (1 <<  7)
#define D_CACHE                     (1 <<  8)
#define D_OUTPUT                    (1 <<  9)
#define D_MISC                      (1 << 31)

#define nop() do {} while (0)

#ifdef __GNUC__
#define LIKELY(x)   (__builtin_expect(!!(x),1))
#define UNLIKELY(x) (__builtin_expect(!!(x),0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define ASSERT_se(expr) \
	do { \
		if (UNLIKELY(!(expr))) { \
			fprintf(stderr, "Assertion '%s' failed at %s:%u, function %s(). Aborting.\n", #expr, \
					__FILE__, __LINE__, PRETTY_FUNCTION); \
				abort(); \
		} \
	} while (0)

#ifdef NDEBUG
#define ASSERT(expr) nop()
#else
#define ASSERT(expr) ASSERT_se(expr)
#endif

#ifdef DEBUG
#define debug_print(debug_class, fmt, ...) \
	do { \
		if(Config.debug_filter & debug_class) { \
			fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
		} \
	} while (0)

#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) \
	do { \
		if(Config.debug_filter & debug_class) { \
			fprintf(stderr, "%s(): " fmt, __func__, ##__VA_ARGS__); \
			fprintf(stderr, "%s(): ", __func__); \
			for(size_t zz = 0; zz < (len); zz++) { \
				fprintf(stderr, "%02x ", buf[zz]); \
				if(zz && (zz+1) % 32 == 0) fprintf(stderr, "\n%s(): ", __func__); \
			} \
			fprintf(stderr, "\n"); \
		} \
	} while(0)
#else
#define debug_print(debug_class, fmt, ...) nop()
#define debug_print_buf_hex(debug_class, buf, len, fmt, ...) nop()
#endif

#define XCALLOC(nmemb, size) xcalloc((nmemb), (size), __FILE__, __LINE__, __func__)
#define XREALLOC(ptr, size) xrealloc((ptr), (size), __FILE__, __LINE__, __func__)
#define XFREE(ptr) do { free(ptr); ptr = NULL; } while(0)
#define NEW(type, x) type *(x) = XCALLOC(1, sizeof(type))
#define UNUSED(x) (void)(x)
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))
#define max(a, b) ((a) > (b) ? (a) : (b))


void *xcalloc(size_t nmemb, size_t size, char const *file, int line, char const *func);
void *xrealloc(void *ptr, size_t size, char const *file, int line, char const *func);
int start_thread(pthread_t *pth, void *(*start_routine)(void *), void *thread_ctx);
void stop_thread(pthread_t pth);
int pthread_barrier_create(pthread_barrier_t *barrier, unsigned count);
int pthread_cond_initialize(pthread_cond_t *cond);
int pthread_mutex_initialize(pthread_mutex_t *mutex);

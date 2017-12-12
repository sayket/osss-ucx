/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "shmemu.h"
#include "shmemc.h"
#include "shmem/defs.h"

#include "shmem_mutex.h"
#include "allocator/memalloc.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

int malloc_error = SHMEM_MALLOC_OK;

#ifdef ENABLE_THREADS
static pthread_mutex_t m_alloc = PTHREAD_MUTEX_INITIALIZER;
#endif /* ENABLE_THREADS */

/*
 * -- API --------------------------------------------------------------------
 */

#ifdef ENABLE_PSHMEM
#pragma weak shmem_malloc = pshmem_malloc
#define shmem_malloc pshmem_malloc
#pragma weak shmem_calloc = pshmem_calloc
#define shmem_calloc pshmem_calloc
#pragma weak shmem_free = pshmem_free
#define shmem_free pshmem_free
#pragma weak shmem_realloc = pshmem_realloc
#define shmem_realloc pshmem_realloc
#pragma weak shmem_align = pshmem_align
#define shmem_align pshmem_align
#endif /* ENABLE_PSHMEM */

void *
shmem_malloc(size_t s)
{
    void *addr;

    SHMEML_MUTEX_PROTECT(addr = shmema_mem_malloc(s), &m_alloc);

    shmemc_barrier_all();

    return addr;
}

void *
shmem_calloc(size_t n, size_t s)
{
    void *addr;

    SHMEML_MUTEX_PROTECT(addr = shmema_mem_calloc(n, s), &m_alloc);

    shmemc_barrier_all();

    return addr;
}

void
shmem_free(void *p)
{
    shmemc_barrier_all();

    SHMEML_MUTEX_PROTECT(shmema_mem_free(p), &m_alloc);
}

/*
 * realloc can cause memory to move around, so we protect it before
 * *and* after (spec 1.4rc5, p. 25)
 */

void *
shmem_realloc(void *p, size_t s)
{
    void *addr;

    shmemc_barrier_all();

    SHMEML_MUTEX_PROTECT(addr = shmema_mem_realloc(p, s), &m_alloc);

    shmemc_barrier_all();

    return addr;
}

void *
shmem_align(size_t a, size_t s)
{
    void *addr;

    SHMEML_MUTEX_PROTECT(addr = shmema_mem_align(a, s), &m_alloc);

    shmemc_barrier_all();

    return addr;
}

/*
 * layer these through top-level API in case we want to insert error
 * checking above
 */

#ifdef ENABLE_PSHMEM
#pragma weak shmalloc = pshmalloc
#define shmalloc pshmalloc
#pragma weak shfree = pshfree
#define shfree pshfree
#pragma weak shrealloc = pshrealloc
#define shrealloc pshrealloc
#pragma weak shmemalign = pshmemalign
#define shmemalign pshmemalign
#endif /* ENABLE_PSHMEM */

void *
shmalloc(size_t s)
{
    deprecate(__func__);
    return shmem_malloc(s);
}

void
shfree(void *p)
{
    deprecate(__func__);
    shmem_free(p);
}

void *
shrealloc(void *p, size_t s)
{
    deprecate(__func__);
    return shmem_realloc(p, s);
}

void *
shmemalign(size_t a, size_t s)
{
    deprecate(__func__);
    return shmem_align(a, s);
}

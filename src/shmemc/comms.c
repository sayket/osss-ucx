#include "thispe.h"

#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <ucp/api/ucp.h>

/*
 * private helper
 */
inline static void
do_flush(void)
{
    ucs_status_t s;

    s = ucp_worker_flush(proc.comms.wrkr);
    assert(s == UCS_OK);
}

/**
 * API
 *
 **/

void
shmemc_quiet(void)
{
    do_flush();
}

/*
 * -- puts & gets --------------------------------------------------------
 */

void
shmemc_put(void *dest, const void *src,
           size_t nbytes, int pe)
{
    uint64_t r_dest = TRANSLATE_ADDR(dest, pe); /* address on other PE */
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest); /* rkey for remote address */
    ucs_status_t s;

    s = ucp_put(LOOKUP_UCP_EP(pe), src, nbytes, r_dest, rkey);
    assert(s == UCS_OK);
}

void
shmemc_get(void *dest, const void *src,
           size_t nbytes, int pe)
{
    uint64_t r_src = TRANSLATE_ADDR(dest, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    ucs_status_t s;

    s = ucp_get(LOOKUP_UCP_EP(pe), dest, nbytes, r_src, rkey);
    assert(s == UCS_OK);
}

/**
 * Return values from put_nbi/get_nbi probably need more handling
 *
 */

void
shmemc_put_nbi(void *dest, const void *src,
               size_t nbytes, int pe)
{
    uint64_t r_dest = TRANSLATE_ADDR(dest, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    ucs_status_t s;

    s = ucp_put_nbi(LOOKUP_UCP_EP(pe), src, nbytes, r_dest, rkey);
    assert(s == UCS_OK || s == UCS_INPROGRESS);
}

void
shmemc_get_nbi(void *dest, const void *src,
               size_t nbytes, int pe)
{
    uint64_t r_src = TRANSLATE_ADDR(dest, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    ucs_status_t s;

    s = ucp_get_nbi(LOOKUP_UCP_EP(pe), dest, nbytes, r_src, rkey);
    assert(s == UCS_OK || s == UCS_INPROGRESS);
}

/*
 * -- atomics ------------------------------------------------------------
 */

/*
 * helpers
 */

inline static uint32_t
helper_fadd32(uint64_t t, uint32_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint32_t ret;
    ucs_status_t s;

    s = ucp_atomic_fadd32(LOOKUP_UCP_EP(pe), v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

inline static uint64_t
helper_fadd64(uint64_t t, uint64_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint64_t ret;
    ucs_status_t s;

    s = ucp_atomic_fadd64(LOOKUP_UCP_EP(pe), v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

inline static void
helper_add32(uint64_t t, uint32_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    ucs_status_t s;

    s = ucp_atomic_add32(LOOKUP_UCP_EP(pe), v, r_t, rkey);
    assert(s == UCS_OK);
}

inline static void
helper_add64(uint64_t t, uint64_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    ucs_status_t s;

    s = ucp_atomic_add64(LOOKUP_UCP_EP(pe), v, r_t, rkey);
    assert(s == UCS_OK);
}

inline static uint32_t
helper_swap32(uint64_t t, uint32_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint32_t ret;
    ucs_status_t s;

    s = ucp_atomic_swap32(LOOKUP_UCP_EP(pe), v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

inline static uint64_t
helper_swap64(uint64_t t, uint64_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint64_t ret;
    ucs_status_t s;

    s = ucp_atomic_swap64(LOOKUP_UCP_EP(pe), v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

inline static uint32_t
helper_cswap32(uint64_t t, uint32_t c, uint32_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint32_t ret;
    ucs_status_t s;

    s = ucp_atomic_cswap32(LOOKUP_UCP_EP(pe), c, v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

inline static uint64_t
helper_cswap64(uint64_t t, uint64_t c, uint64_t v, int pe)
{
    uint64_t r_t = TRANSLATE_ADDR(t, pe);
    ucp_rkey_h rkey = LOOKUP_RKEY(r_dest);
    uint64_t ret;
    ucs_status_t s;

    s = ucp_atomic_cswap64(LOOKUP_UCP_EP(pe), c, v, r_t, rkey, &ret);
    assert(s == UCS_OK);

    return ret;
}

/**
 * API
 **/

/*
 * add
 */

#define SHMEMC_TYPED_ADD(_name, _type, _size)                   \
    void                                                        \
    shmemc_##_name##_add(_type *t, _type v, int pe)             \
    {                                                           \
        helper_add##_size((uint64_t) t, v, pe);                 \
    }

SHMEMC_TYPED_ADD(int, int, 32)
SHMEMC_TYPED_ADD(long, long, 64)
SHMEMC_TYPED_ADD(longlong, long long, 64)

/*
 * inc is just "add 1"
 */

#define SHMEMC_TYPED_INC(_name, _type, _size)                   \
    void                                                        \
    shmemc_##_name##_inc(_type *t, int pe)                      \
    {                                                           \
        helper_add##_size((uint64_t) t, 1, pe);                 \
    }

SHMEMC_TYPED_INC(int, int, 32)
SHMEMC_TYPED_INC(long, long, 64)
SHMEMC_TYPED_INC(longlong, long long, 64)

/*
 * fetch-and-add
 */

#define SHMEMC_TYPED_FADD(_name, _type, _size)                  \
    _type                                                       \
    shmemc_##_name##_fadd(_type *t, _type v, int pe)            \
    {                                                           \
        return (_type) helper_fadd##_size((uint64_t) t, v, pe); \
    }

SHMEMC_TYPED_FADD(int, int, 32)
SHMEMC_TYPED_FADD(long, long, 64)
SHMEMC_TYPED_FADD(longlong, long long, 64)

/*
 * finc is just "fadd 1"
 */

#define SHMEMC_TYPED_FINC(_name, _type, _size)                  \
    _type                                                       \
    shmemc_##_name##_finc(_type *t, int pe)                     \
    {                                                           \
        return (_type) helper_fadd##_size((uint64_t) t, 1, pe); \
    }

SHMEMC_TYPED_FINC(int, int, 32)
SHMEMC_TYPED_FINC(long, long, 64)
SHMEMC_TYPED_FINC(longlong, long long, 64)

/*
 * swaps
 */

#define SHMEMC_TYPED_SWAP(_name, _type, _size)                  \
    _type                                                       \
    shmemc_##_name##_swap(_type *t, _type v, int pe)            \
    {                                                           \
        return (_type) helper_swap##_size((uint64_t) t, v, pe); \
    }                                                           \

SHMEMC_TYPED_SWAP(int, int, 32)
SHMEMC_TYPED_SWAP(long, long, 64)
SHMEMC_TYPED_SWAP(longlong, long long, 64)
SHMEMC_TYPED_SWAP(float, float, 32)
SHMEMC_TYPED_SWAP(double, double, 64)

#define SHMEMC_TYPED_CSWAP(_name, _type, _size)                     \
    _type                                                           \
    shmemc_##_name##_cswap(_type *t, _type c, _type v, int pe)      \
    {                                                               \
        return (_type) helper_cswap##_size((uint64_t) t, c, v, pe); \
    }                                                               \

SHMEMC_TYPED_CSWAP(int, int, 32)
SHMEMC_TYPED_CSWAP(long, long, 64)
SHMEMC_TYPED_CSWAP(longlong, long long, 64)

/*
 * fetch & set
 *
 * TODO: silly impl. for now
 *
 */

#define SHMEMC_TYPED_FETCH(_name, _type, _size)                 \
    _type                                                       \
    shmemc_##_name##_fetch(_type *t, int pe)                    \
    {                                                           \
        return (_type) helper_fadd##_size((uint64_t) t, 0, pe); \
    }

SHMEMC_TYPED_FETCH(int, int, 32)
SHMEMC_TYPED_FETCH(long, long, 64)
SHMEMC_TYPED_FETCH(longlong, long long, 64)
SHMEMC_TYPED_FETCH(float, float, 32)
SHMEMC_TYPED_FETCH(double, double, 64)

#define SHMEMC_TYPED_SET(_name, _type, _size)       \
    void                                            \
    shmemc_##_name##_set(_type *t, _type v, int pe) \
    {                                               \
        *t = v;                                     \
    }

SHMEMC_TYPED_SET(int, int, 32)
SHMEMC_TYPED_SET(long, long, 64)
SHMEMC_TYPED_SET(longlong, long long, 64)
SHMEMC_TYPED_SET(float, float, 32)
SHMEMC_TYPED_SET(double, double, 64)
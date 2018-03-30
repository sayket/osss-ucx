/* For license: see LICENSE file at top-level */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "state.h"
#include "shmemu.h"

#include "shmem/defs.h"

#include <ucp/api/ucp.h>

#if 0
#define VOLATILIZE(_type, _var) (* ( volatile _type *) (_var))
#endif

/*
 * return 1 if the memory location changed to "value", otherwise 0
 */

#define COMMS_CTX_TEST_SIZE(_size, _opname, _op)                        \
    int                                                                 \
    shmemc_ctx_test_##_opname##_size(shmem_ctx_t ctx,                   \
                                     int##_size##_t *var,               \
                                     int##_size##_t value)              \
    {                                                                   \
        shmemc_context_h ch = (shmemc_context_h) ctx;                   \
                                                                        \
        ucp_worker_wait_mem(ch->w, var);                                \
        return ( (*var) _op (value) ) ? 1 : 0;                          \
    }

COMMS_CTX_TEST_SIZE(16, eq, ==)
COMMS_CTX_TEST_SIZE(32, eq, ==)
COMMS_CTX_TEST_SIZE(64, eq, ==)

COMMS_CTX_TEST_SIZE(16, ne, !=)
COMMS_CTX_TEST_SIZE(32, ne, !=)
COMMS_CTX_TEST_SIZE(64, ne, !=)

COMMS_CTX_TEST_SIZE(16, gt, >)
COMMS_CTX_TEST_SIZE(32, gt, >)
COMMS_CTX_TEST_SIZE(64, gt, >)

COMMS_CTX_TEST_SIZE(16, le, <=)
COMMS_CTX_TEST_SIZE(32, le, <=)
COMMS_CTX_TEST_SIZE(64, le, <=)

COMMS_CTX_TEST_SIZE(16, lt, <)
COMMS_CTX_TEST_SIZE(32, lt, <)
COMMS_CTX_TEST_SIZE(64, lt, <)

COMMS_CTX_TEST_SIZE(16, ge, >=)
COMMS_CTX_TEST_SIZE(32, ge, >=)
COMMS_CTX_TEST_SIZE(64, ge, >=)

#define COMMS_CTX_WAIT_SIZE(_size, _opname, _op)                        \
    void                                                                \
    shmemc_ctx_wait_##_opname##_until##_size(shmem_ctx_t ctx,           \
                                             int##_size##_t *var,       \
                                             int##_size##_t value)      \
    {                                                                   \
        do {                                                            \
            shmemc_progress();                                          \
        } while (shmemc_ctx_test_##_opname##_size(ctx, var, value) == 0); \
    }

COMMS_CTX_WAIT_SIZE(16, eq, ==)
COMMS_CTX_WAIT_SIZE(32, eq, ==)
COMMS_CTX_WAIT_SIZE(64, eq, ==)

COMMS_CTX_WAIT_SIZE(16, ne, !=)
COMMS_CTX_WAIT_SIZE(32, ne, !=)
COMMS_CTX_WAIT_SIZE(64, ne, !=)

COMMS_CTX_WAIT_SIZE(16, gt, >)
COMMS_CTX_WAIT_SIZE(32, gt, >)
COMMS_CTX_WAIT_SIZE(64, gt, >)

COMMS_CTX_WAIT_SIZE(16, le, <=)
COMMS_CTX_WAIT_SIZE(32, le, <=)
COMMS_CTX_WAIT_SIZE(64, le, <=)

COMMS_CTX_WAIT_SIZE(16, lt, <)
COMMS_CTX_WAIT_SIZE(32, lt, <)
COMMS_CTX_WAIT_SIZE(64, lt, <)

COMMS_CTX_WAIT_SIZE(16, ge, >=)
COMMS_CTX_WAIT_SIZE(32, ge, >=)
COMMS_CTX_WAIT_SIZE(64, ge, >=)

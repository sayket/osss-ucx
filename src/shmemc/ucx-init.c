#include "thispe.h"
#include "shmemu.h"

#include <stdlib.h>             /* getenv */
#include <assert.h>

#include <ucp/api/ucp.h>

#define KB 1024L
#define MB (KB * KB)
#define GB (KB * MB)

/*
 * the PE's state
 */
thispe_info_t proc = {
    .status = SHMEM_PE_UNKNOWN,
    .refcount = 0,
    .heaps = NULL
};

/*
 * where globals/statics/commons/saves live
 *
 */
static ucp_mem_h global_segment;

/*
 * some memory to play with registering
 *
 * TODO: multiple heap support
 */
static ucp_mem_h symm_heap;

/*
 * debugging output stream
 */
static FILE *say;

#if 0
/*
 * TODO: move to shmemu namespace in real implementation
 */
static void *
shmemu_round_down_address_to_pagesize(void *addr)
{
    const long psz = sysconf(_SC_PAGESIZE);
    /* make sure aligned to page size multiples */
    const size_t mod = (size_t) addr % psz;

    if (mod != 0) {
        const size_t div = (size_t) addr / psz;

        addr = (void *) ((div - 1) * psz);
    }

    return addr;
}
#endif

/**
 * no special treatment required here
 *
 * TODO put getenv calls all in 1 place
 */
char *
shmemc_getenv(const char *name)
{
    return getenv(name);
}

static void
check_version(void)
{
    unsigned int maj, min, rel;

    ucp_get_version(&maj, &min, &rel);

    fprintf(say, "Piecewise query:\n");
    fprintf(say, "    UCX version \"%u.%u.%u\"\n", maj, min, rel);
    fprintf(say, "String query\n");
    fprintf(say, "    UCX version \"%s\"\n", ucp_get_version_string());
    fprintf(say, "\n");
}

static void
make_init_params(ucp_params_t *p_p)
{
    p_p->field_mask =
        UCP_PARAM_FIELD_FEATURES |
        UCP_PARAM_FIELD_ESTIMATED_NUM_EPS;

    /*
     * we'll try at first to get both 32- and 64-bit direct AMO
     * support because OpenSHMEM wants them
     */
    p_p->features =
        UCP_FEATURE_RMA
#if 1
        /* while testing so we can play on mlx4 card */
        |
        UCP_FEATURE_AMO32
        |
        UCP_FEATURE_AMO64
#endif
        ;
    p_p->estimated_num_eps = proc.nranks;
}

/*
 * worker tables
 */
static void
allocate_workers(void)
{
    proc.comms.wrkrs = (worker_info_t *)
        calloc(proc.nranks, sizeof(*(proc.comms.wrkrs)));
    assert(proc.comms.wrkrs != NULL);
}

static void
deallocate_workers(void)
{
    if (proc.comms.wrkrs != NULL) {
        free(proc.comms.wrkrs);
    }
}

static void
make_local_worker(void)
{
    ucs_status_t s;
    ucp_worker_params_t wkpm;
    ucp_address_t *a;
    size_t l;

    wkpm.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    wkpm.thread_mode = UCS_THREAD_MODE_SINGLE;

    s = ucp_worker_create(proc.comms.ctxt, &wkpm, &proc.comms.wrkr);
    assert(s == UCS_OK);

    /* get address for remote access to worker */
    s = ucp_worker_get_address(proc.comms.wrkr, &a, &l);
    proc.comms.wrkrs[proc.rank].addr = a;
    proc.comms.wrkrs[proc.rank].len = l;

    assert(s == UCS_OK);
}

/*
 * endpoint tables
 */
static void
allocate_endpoints(void)
{
    proc.comms.eps = (ucp_ep_h *)
        calloc(proc.nranks, sizeof(*(proc.comms.eps)));
    assert(proc.comms.eps != NULL);
}

static void
deallocate_endpoints(void)
{
    if (proc.comms.eps != NULL) {
        free(proc.comms.eps);
    }
}

static void
make_an_endpoint(ucp_address_t *addr, int pe)
{
    ucs_status_t s;
    ucp_ep_params_t epm;

    epm.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
    epm.address = addr;

    s = ucp_ep_create(proc.comms.wrkr, &epm, &proc.comms.eps[pe]);
    assert(s == UCS_OK);
}

/*
 * debugging output
 */
static void
dump_mapped_mem_info(const char *name, const ucp_mem_h *m)
{
    ucs_status_t s;
    ucp_mem_attr_t attr;

    /* the attributes we want to inspect */
    attr.field_mask =
        UCP_MEM_ATTR_FIELD_ADDRESS |
        UCP_MEM_ATTR_FIELD_LENGTH;

    s = ucp_mem_query(*m, &attr);
    assert(s == UCS_OK);

    fprintf(say,
            "%d: \"%s\" memory at %p, length %.2f MB (%lu bytes)\n",
            proc.rank, name,
            attr.address,
            (double) attr.length / MB,
            (unsigned long) attr.length
            );
}

extern int shmemu_parse_size(char *size_str, size_t *bytes_p);

static void
reg_symmetric_heap(void)
{
    ucs_status_t s;
    ucp_mem_map_params_t mp;
    ucp_mem_attr_t attr;
    char *hs_str = shmemc_getenv("SHMEM_SYMMETRIC_HEAP_SIZE");
    size_t len;

    /* first look to see if we request a certain size */
    if (hs_str != NULL) {
        const int r = shmemu_parse_size(hs_str, &len);
        assert(r == 0);
    }
    else {
        len = 4 * MB; /* just some usable value */
    }

    /* now register it with UCX */
    mp.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_LENGTH |
        UCP_MEM_MAP_PARAM_FIELD_FLAGS;

    mp.length = len;
    mp.flags =
        UCP_MEM_MAP_ALLOCATE;

    s = ucp_mem_map(proc.comms.ctxt, &mp, &symm_heap);
    assert(s == UCS_OK);

    /*
     * query back to find where it is, and its actual size (might be
     * aligned/padded)
     */

    /* the attributes we want to inspect */
    attr.field_mask =
        UCP_MEM_ATTR_FIELD_ADDRESS |
        UCP_MEM_ATTR_FIELD_LENGTH;

    s = ucp_mem_query(symm_heap, &attr);
    assert(s == UCS_OK);

    /* tell the PE what was given */
    proc.heaps[proc.rank].base = attr.address;
    proc.heaps[proc.rank].size = attr.length;
}

static void
dereg_symmetric_heap(void)
{
    ucs_status_t s;

    s = ucp_mem_unmap(proc.comms.ctxt, symm_heap);
    assert(s == UCS_OK);
}

extern char data_start;
extern char end;

static void
reg_globals(void)
{
    ucs_status_t s;
    ucp_mem_map_params_t mp;
    void *base = &data_start;
    const size_t len = (size_t) &end - (size_t) base;

    mp.field_mask =
        UCP_MEM_MAP_PARAM_FIELD_ADDRESS |
        UCP_MEM_MAP_PARAM_FIELD_LENGTH;

    mp.address = base;
    mp.length = len;

    s = ucp_mem_map(proc.comms.ctxt, &mp, &global_segment);
    assert(s == UCS_OK);
}

static void
dereg_globals(void)
{
    ucs_status_t s;

    s = ucp_mem_unmap(proc.comms.ctxt, global_segment);
    assert(s == UCS_OK);
}

/**
 * API
 *
 **/

void
shmemc_ucx_make_remote_endpoints(void)
{
    int pe;

    for (pe = 0; pe < proc.nranks; pe += 1) {
        make_an_endpoint(proc.comms.wrkrs[pe].addr, pe);
    }
}

void
shmemc_ucx_init(void)
{
    ucs_status_t s;
    ucp_params_t pm;

    say = stderr;

    proc.refcount += 1;

    /* no re-init */
    if (proc.status == SHMEM_PE_RUNNING) {
        return;
    }

    /* start initialization */
    s = ucp_config_read(NULL, NULL, &proc.comms.cfg);
    assert(s == UCS_OK);

    make_init_params(&pm);

    s = ucp_init(&pm, proc.comms.cfg, &proc.comms.ctxt);
    assert(s == UCS_OK);

    /*
     * Create workers and space for EPs
     */
    allocate_workers();
    allocate_endpoints();

    make_local_worker();

#if 0
    /*
     * try registering some implicit memory as symmetric heap
     */
    reg_symmetric_heap();

    /*
     * try registering the data and bss segments
     */
    reg_globals();
#endif

#ifdef DEBUG
    if (proc.rank == 0) {
        const ucs_config_print_flags_t flags =
            UCS_CONFIG_PRINT_CONFIG |
            UCS_CONFIG_PRINT_HEADER;

        ucp_config_print(proc.comms.cfg, say, "My config", flags);
        ucp_context_print_info(proc.comms.ctxt, say);
        check_version();
        fprintf(say, "----------------------------------------------\n\n");
        fflush(say);
    }
    ucp_worker_print_info(proc.comms.wrkr, say);
    dump_mapped_mem_info("heap", &symm_heap);
    dump_mapped_mem_info("globals", &global_segment);
#endif
    /* don't need config info any more */
    ucp_config_release(proc.comms.cfg);
}

void
shmemc_ucx_finalize(void)
{
    ucs_status_ptr_t sp;

    proc.refcount -= 1;

    if (proc.status != SHMEM_PE_RUNNING) {
        return;
    }

    return;

    /* really want a global barrier */
    shmemc_quiet();

    /* and clean up */

    sp = ucp_disconnect_nb(proc.comms.eps[proc.rank]);
    if (sp != UCS_OK) {
        ucs_status_t s;

        do {
            ucp_worker_progress(proc.comms.wrkr);
            s = ucp_request_test(sp, NULL);
        } while (s == UCS_INPROGRESS);

        assert(s == UCS_OK);
    }
    deallocate_endpoints();

    ucp_worker_release_address(proc.comms.wrkr,
                               proc.comms.wrkrs[proc.rank].addr);
    ucp_worker_destroy(proc.comms.wrkr); /* and free worker_info_t's ? */

    deallocate_workers();

    dereg_globals();
    dereg_symmetric_heap();

    ucp_cleanup(proc.comms.ctxt);
}

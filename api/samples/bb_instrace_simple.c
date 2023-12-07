#include <stdio.h>
#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "utils.h"
// #include "dr_ir_opcodes_arm.h"
#include "mutils.h"

static client_id_t client_id;
static reg_id_t tls_seg;
static uint tls_offs;
static uint64 num_refs; /* keep a global instruction reference count */
static int tls_idx;
static void *mutex;


/* Max number of ins_ref a buffer can have. It should be big enough
 * to hold all entries between clean calls.
 */
#define MAX_NUM_INS_REFS 8192
/* The maximum size of buffer for holding ins_refs. */
#define MEM_BUF_SIZE (sizeof(ins_ref_t) * MAX_NUM_INS_REFS)

/* Allocated TLS slot offsets */
enum {
    INSTRACE_TLS_OFFS_BUF_PTR,
    INSTRACE_TLS_COUNT, /* total number of TLS slots allocated */
};

#define TLS_SLOT(tls_base, enum_val) (void **)((byte *)(tls_base) + tls_offs + (enum_val))
#define BUF_PTR(tls_base) *(ins_ref_t **)TLS_SLOT(tls_base, INSTRACE_TLS_OFFS_BUF_PTR)
#define MINSERT instrlist_meta_preinsert

typedef struct {
    app_pc pc;
    // int opcode;
    uint64 cycles;
} ins_ref_t;

typedef struct {
    byte *seg_base;
    ins_ref_t *buf_base;
    file_t log;
    FILE *logf;
    uint64 num_refs;
}per_thread_t;

// static void instrace(void* drcontext){
//     per_thread_t *data;
//     ins_ref_t *ins_ref, *buf_ptr;

//     data = drmgr_get_tls_field(drcontext, tls_idx);
//     buf_ptr = BUF_PTR(data->seg_base);
//     for(ins_ref=(ins_ref_t *)data->buf_base; ins_ref < buf_ptr; ins_ref++){
//         fprintf(data->logf, PIFX ",%s\n", (ptr_uint_t)ins_ref->pc, decode_opcode_name(ins_ref->opcode));
//         data->num_refs++;
//     }
//     BUF_PTR(data->seg_base) = data->buf_base;
// }

static void instrace(void* drcontext){
    per_thread_t *data;
    ins_ref_t *ins_ref, *buf_ptr;

    data = drmgr_get_tls_field(drcontext, tls_idx);
    buf_ptr = BUF_PTR(data->seg_base);
    for(ins_ref=(ins_ref_t *)data->buf_base; ins_ref < buf_ptr; ins_ref++){
        fprintf(data->logf, PIFX ",%lu\n", (ptr_uint_t)ins_ref->pc, (ptr_uint_t)ins_ref->cycles);
        data->num_refs++;
    }
    BUF_PTR(data->seg_base) = data->buf_base;
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data);


static void
event_thread_init(void *drcontext)
{
    printf("clock cycle frequency: %lu\n", get_arm_clock_freq());
    per_thread_t *data = dr_thread_alloc(drcontext, sizeof(per_thread_t));
    DR_ASSERT(data != NULL);
    drmgr_set_tls_field(drcontext, tls_idx, data);

    /* Keep seg_base in a per-thread data structure so we can get the TLS
     * slot and find where the pointer points to in the buffer.
     */
    data->seg_base = dr_get_dr_segment_base(tls_seg);
    data->buf_base =
        dr_raw_mem_alloc(MEM_BUF_SIZE, DR_MEMPROT_READ | DR_MEMPROT_WRITE, NULL);
    DR_ASSERT(data->seg_base != NULL && data->buf_base != NULL);
    /* put buf_base to TLS as starting buf_ptr */
    BUF_PTR(data->seg_base) = data->buf_base;

    data->num_refs = 0;

    /* We're going to dump our data to a per-thread file.
     * On Windows we need an absolute path so we place it in
     * the same directory as our library. We could also pass
     * in a path as a client argument.
     */
    data->log =
        log_file_open(client_id, drcontext, NULL /* using client lib path */, "instrace",
#ifndef WINDOWS
                      DR_FILE_CLOSE_ON_FORK |
#endif
                          DR_FILE_ALLOW_LARGE);
    data->logf = log_stream_from_file(data->log);
    fprintf(data->logf, "Format: <instr address>,<opcode>\n");
}

static void
event_thread_exit(void *drcontext){
    per_thread_t *data;
    instrace(drcontext); // dump
    data = drmgr_get_tls_field(drcontext, tls_idx);
    dr_mutex_lock(mutex);
    num_refs += data->num_refs;
    dr_mutex_unlock(mutex);
    log_stream_close(data->logf);
    dr_raw_mem_free(data->buf_base, MEM_BUF_SIZE);
    dr_thread_free(drcontext, data, sizeof(per_thread_t));
}

static void event_exit(void){
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'instrace' num refs seen: " SZFMT "\n", num_refs);
    if (!dr_raw_tls_cfree(tls_offs, INSTRACE_TLS_COUNT))
        DR_ASSERT(false);
    
    if(!drmgr_unregister_tls_field(tls_idx) ||
        !drmgr_unregister_thread_init_event(event_thread_init) ||
        !drmgr_unregister_thread_exit_event(event_thread_exit) ||
        !drmgr_unregister_bb_insertion_event(event_app_instruction) ||
        drreg_exit() != DRREG_SUCCESS){
            DR_ASSERT(false);
        }
    dr_mutex_destroy(mutex);
    drmgr_exit();
}


static void
insert_load_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr){
    // inseart "reg_ptr = tls_seg + tls_offs"
    dr_insert_read_raw_tls(drcontext, ilist, where, tls_seg, 
                            tls_offs + INSTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}


static void
insert_save_pc(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
               reg_id_t scratch, app_pc pc){
    // scratch = pc
    instrlist_insert_mov_immed_ptrsz(drcontext, (ptr_int_t)pc, opnd_create_reg(scratch), 
                                    ilist, where, NULL, NULL);
    // *(base+ offsetof(ins_ref_t, pc)) = scratch 
    MINSERT(ilist, where, 
            XINST_CREATE_store(drcontext, 
                                    OPND_CREATE_MEMPTR(base, offsetof(ins_ref_t, pc)), opnd_create_reg(scratch)));
}


// static void
// insert_save_opcode(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
//     reg_id_t scratch, int opcode){
//     // Change register to 2 bytes
//     scratch = reg_resize_to_opsz(scratch, OPSZ_2);
//     MINSERT(ilist, where,
//             XINST_CREATE_load_int(drcontext, opnd_create_reg(scratch), OPND_CREATE_INT16(opcode)));
//     MINSERT(ilist, where,
//             XINST_CREATE_store_2bytes(
//                 drcontext, OPND_CREATE_MEM16(base, offsetof(ins_ref_t, opcode)),
//                 opnd_create_reg(scratch)));
// }

static void
insert_update_buf_ptr(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t reg_ptr, int adjust){
    // reg_ptr = sizeof(ins_ref_t)
    MINSERT(ilist, where,
            XINST_CREATE_add(drcontext, opnd_create_reg(reg_ptr), OPND_CREATE_INT16(adjust)));
    dr_insert_write_raw_tls(drcontext, ilist, where, tls_seg, tls_offs + INSTRACE_TLS_OFFS_BUF_PTR, reg_ptr);
}


// Insert "isb"
// static void
// insert_instruction_sync_barrier(void *drcontext, instrlist_t *ilist, instr_t *where){
//     MINSERT(ilist, where, instr_create_0dst_0src(drcontext, OP_isb));
// }

// Insert "mov scratch cntvct_el0"
static void
insert_save_cycles(void *drcontext, instrlist_t *ilist, instr_t *where, reg_id_t base,
    reg_id_t scratch){
    scratch = reg_resize_to_opsz(scratch, OPSZ_8);
    MINSERT(ilist, where, 
            INSTR_CREATE_mrs(drcontext, opnd_create_reg(scratch),
                                               opnd_create_reg(DR_REG_CNTVCT_EL0)));
    MINSERT(ilist, where,
            XINST_CREATE_store(
                drcontext, OPND_CREATE_MEM64(base, offsetof(ins_ref_t, cycles)),
                opnd_create_reg(scratch)));
}

static void
instrument_instr(void *drcontext, instrlist_t *ilist, instr_t *where){
    reg_id_t reg_ptr, reg_tmp;
    if (drreg_reserve_register(drcontext, ilist, where, NULL, &reg_ptr) !=
            DRREG_SUCCESS ||
        drreg_reserve_register(drcontext, ilist, where, NULL, &reg_tmp) !=
            DRREG_SUCCESS) {
        DR_ASSERT(false); /* cannot recover */
        return;
    }

    insert_load_buf_ptr(drcontext, ilist, where, reg_ptr);
    insert_save_pc(drcontext, ilist, where, reg_ptr, reg_tmp, instr_get_app_pc(where));
    // insert_save_opcode(drcontext, ilist, where, reg_ptr, reg_tmp, instr_get_opcode(where));
    // insert_instruction_sync_barrier(drcontext, ilist, where);
    insert_save_cycles(drcontext, ilist, where, reg_ptr, reg_tmp);
    insert_update_buf_ptr(drcontext, ilist, where, reg_ptr, sizeof(ins_ref_t));
    /* Restore scratch registers */
    if (drreg_unreserve_register(drcontext, ilist, where, reg_ptr) != DRREG_SUCCESS ||
        drreg_unreserve_register(drcontext, ilist, where, reg_tmp) != DRREG_SUCCESS)
        DR_ASSERT(false);
}

/* clean_call dumps the memory reference info to the log file */
// static void
// clean_call(void)
// {
//     void *drcontext = dr_get_current_drcontext();
//     instrace(drcontext);
// }

static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data){
    /* we don't want to auto-predicate any instrumentation */
    drmgr_disable_auto_predication(drcontext, bb);
    
    if (!instr_is_app(instr))
        return DR_EMIT_DEFAULT;
    
    /* insert code to add an entry to the buffer */
    if(drmgr_is_first_instr(drcontext, instr)){
        instrument_instr(drcontext, bb, instr);
    }
    else if(drmgr_is_last_instr(drcontext, instr)){
        instrument_instr(drcontext, bb, instr);
    }

    // /* insert code once per bb to call clean_call for processing the buffer */
    // if (drmgr_is_first_instr(drcontext, instr)
    //     /* XXX i#1698: there are constraints for code between ldrex/strex pairs,
    //      * so we minimize the instrumentation in between by skipping the clean call.
    //      * We're relying a bit on the typical code sequence with either ldrex..strex
    //      * in the same bb, in which case our call at the start of the bb is fine,
    //      * or with a branch in between and the strex at the start of the next bb.
    //      * However, there is still a chance that the instrumentation code may clear the
    //      * exclusive monitor state.
    //      * Using a fault to handle a full buffer should be more robust, and the
    //      * forthcoming buffer filling API (i#513) will provide that.
    //      */
    //     IF_AARCHXX(&&!instr_is_exclusive_store(instr)))
    //     dr_insert_clean_call(drcontext, bb, instr, (void *)clean_call, false, 0);
    
    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[]){
    /* We need 2 reg slots beyond drreg's eflags slots => 3 slots */
    drreg_options_t ops = { sizeof(ops), 3, false };
    dr_set_client_name("DynamoRIO Sample Client 'instrace'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);
    /* register events */
    dr_register_exit_event(event_exit);

    if(!drmgr_register_thread_init_event(event_thread_init) ||
        !drmgr_register_thread_exit_event(event_thread_exit) ||
        !drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL)){
            DR_ASSERT(false);
    }
    client_id = id;
    mutex = dr_mutex_create();

    tls_idx = drmgr_register_tls_field();
    DR_ASSERT(tls_idx != -1);

    if(!dr_raw_tls_calloc(&tls_seg, &tls_offs, INSTRACE_TLS_COUNT, 0)){
        DR_ASSERT(false);
    }
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'instrace' initializing\n");
}
#include <stddef.h> /* for offsetof */
#include "dr_api.h"
#include "drmgr.h"
#include "drreg.h"
#include "drx.h"



/* we only have a global count */
static int enter_count;
static int exit_count;

static void
event_exit(void)
{
    drx_exit();
    drreg_exit();
    drmgr_exit();
    printf("global_count: %d\n", enter_count);
    printf("exit_count: %d\n", exit_count);
}

// // Your application-specific code to print the value before the basic block
// static void print_before(void) {
//     // __asm__ volatile(
//     //     "mov rdi, before_bb_string\n"
//     //     "mov rax, 1\n"
//     //     "mov rdx, 14\n"
//     //     "syscall"
//     // );
//     printf("before\n");
// }

// // Your application-specific code to print the value after the basic block
// static void print_after(void) {
//     // __asm__ volatile(
//     //     "mov rdi, after_bb_string\n"
//     //     "mov rax, 1\n"
//     //     "mov rdx, 13\n"
//     //     "syscall"
//     // );
//     printf("after\n");
// }

static void
insert_berfore_first(void *drcontext, instrlist_t *bb, instr_t *where, void* addr){
    instrlist_meta_preinsert(
    // instrlist_preinsert(
        bb, where,
        LOCK(INSTR_CREATE_inc(
            drcontext, OPND_CREATE_ABSMEM(((byte *)addr), OPSZ_4))));
    // printf("* ");
    // instr_disassemble(drcontext, where, STDOUT);
    // printf("*\n");
}

static void
insert_after_last(void *drcontext, instrlist_t *bb, instr_t *where, void* addr){
    instrlist_meta_preinsert(
        bb, where,
        LOCK(INSTR_CREATE_inc(
            drcontext, OPND_CREATE_ABSMEM(((byte *)addr), OPSZ_4))));
    // instrlist_meta_append(
    // instrlist_postinsert(
    //     bb, where,
    //     LOCK(INSTR_CREATE_inc(
    //         drcontext, OPND_CREATE_ABSMEM(((byte *)addr), OPSZ_4))));
    // printf("* ");
    // instr_disassemble(drcontext, where, STDOUT);
    // printf("*\n");
}

static dr_emit_flags_t
event_app_instruction(void *drcontext, void* tag, instrlist_t *bb, instr_t *inst,
                     bool for_trace, bool translating, void *user_data){
    drmgr_disable_auto_predication(drcontext, bb);
    // If not first and not last instruction, return normal emit
    if (drmgr_is_first_instr(drcontext, inst) || drmgr_is_last_instr(drcontext, inst)){
        // If first instruction
        if(drmgr_is_first_instr(drcontext, inst)){
            // Insert code to print before
            // dr_insert_clean_call(drcontext, bb, inst, (void *)print_before, false, 0);
            insert_berfore_first(drcontext, bb, inst, &enter_count);
            instr_disassemble(drcontext, inst, STDOUT);
            printf("\n");
        }
        
        // If last instruction
        if(drmgr_is_last_instr(drcontext, inst)){
            // Insert code to print after
            // dr_insert_clean_call(drcontext, bb, inst, (void *)print_after, false, 0);
            insert_after_last(drcontext, bb, inst, &exit_count);
            instr_disassemble(drcontext, inst, STDOUT);
            printf("\n");
        }
    }
    return DR_EMIT_DEFAULT;
}


DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char* argv[]){
    drreg_options_t ops = { sizeof(ops), 1 /*max slots needed: aflags*/, false };
    dr_set_client_name("DynamoRIO Sample Client 'bbtiming'",
                       "http://dynamorio.org/issues");
    if (!drmgr_init() || !drx_init() || drreg_init(&ops) != DRREG_SUCCESS)
        DR_ASSERT(false);
    /* register events */
    dr_register_exit_event(event_exit);
    if(!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
        DR_ASSERT(false);
    
    
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'bbtiming' initializing\n");
}

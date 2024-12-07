#include "debug/debug_macros.h"
#include "debug/debug_print.h"
#include "globals/global_defs.h"
#include "globals/global_types.h"
#include "globals/global_vars.h"

#include "globals/assert.h"
#include "globals/utils.h"
#include "op.h"

#include "core.param.h"
#include "dcache_stage.h"
#include "debug/debug.param.h"
#include "general.param.h"
#include "libs/cache_lib.h"
#include "libs/hash_lib.h"
#include "libs/list_lib.h"
#include "memory/memory.h"
#include "memory/memory.param.h"
#include "prefetcher/pref.param.h"
#include "prefetcher/pref_common.h"
#include "prefetcher/pref_bop.h"
#include "prefetcher/pref_bop.param.h"
#include "statistics.h"


bop_prefetchers bop_prefetchers_array;
unsigned long global_access_counter = 0;

void increment_global_access_counter() {
    global_access_counter++;
}

void pref_bop_init(HWP* hwp) {
    hwp->hwp_info->enabled = TRUE;

    bop_prefetchers_array.bop_hwp_core_ul1 = (BOP_State*)malloc(sizeof(BOP_State) * NUM_CORES);
    bop_prefetchers_array.bop_hwp_core_umlc = (BOP_State*)malloc(sizeof(BOP_State) * NUM_CORES);

    for (int proc_id = 0; proc_id < NUM_CORES; proc_id++) {
        BOP_State* state_ul1 = &bop_prefetchers_array.bop_hwp_core_ul1[proc_id];
        BOP_State* state_umlc = &bop_prefetchers_array.bop_hwp_core_umlc[proc_id];

        state_ul1->history.addresses = malloc(sizeof(Addr) * HISTORY_SIZE);
        state_umlc->history.addresses = malloc(sizeof(Addr) * HISTORY_SIZE);

        state_ul1->history.size = state_umlc->history.size = HISTORY_SIZE;
        state_ul1->history.count = state_umlc->history.count = 0;
        state_ul1->history.head = state_umlc->history.head = 0;

        init_hash_table(&state_ul1->offset_table, "BOP Offset Table UL1", HASH_TABLE_SIZE, sizeof(int));
        init_hash_table(&state_umlc->offset_table, "BOP Offset Table UMLC", HASH_TABLE_SIZE, sizeof(int));

        state_ul1->best_offset = state_umlc->best_offset = 0;
        state_ul1->round_count = state_umlc->round_count = 0;

        for (int i = 0; i < RR_TABLE_SIZE; i++) {
            state_ul1->rr_table[i].tag = 0;
            state_ul1->rr_table[i].timestamp = 0;
            state_umlc->rr_table[i].tag = 0;
            state_umlc->rr_table[i].timestamp = 0;
        }
    }
}

void update_rr_table(BOP_State* state, Addr baseAddr, unsigned long access_count) {
    int index = baseAddr % RR_TABLE_SIZE;
    state->rr_table[index].tag = baseAddr;
    state->rr_table[index].timestamp = access_count;
}

int check_timeliness(BOP_State* state, Addr demandAddr, unsigned long access_count) {
    int index = demandAddr % RR_TABLE_SIZE;
    RR_Entry* entry = &state->rr_table[index];

    if (entry->tag == demandAddr) {
        unsigned long latency = access_count - entry->timestamp;
        return (latency <= TIMELINESS_THRESHOLD);
    }
    return 0; 
}

void bop_train(BOP_State* state, Addr lineAddr) {
    BOP_History* history = &state->history;

    for (int i = 0; i < history->count; i++) {
        int index = (history->head - i - 1 + history->size) % history->size;
        Addr prev_addr = history->addresses[index];
        int offset = lineAddr - prev_addr;

        Flag new_entry;
        int* score = (int*)hash_table_access_create(&state->offset_table, offset, &new_entry);
        if (new_entry) {
            *score = 1;
        } else {
            (*score)++;
        }

        if (*score >= SCORE_MAX) {
            state->best_offset = offset;
            state->round_count = 0;
            return;
        }
    }

    state->round_count++;
    if (state->round_count >= ROUND_MAX) {
        int max_score = 0;
        for (int i = 0; i < HASH_TABLE_SIZE; i++) {
            int* score = hash_table_access(&state->offset_table, i);
            if (score && *score > max_score) {
                max_score = *score;
                state->best_offset = i;
            }
        }
        state->round_count = 0;
    }
}

void pref_bop_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
    BOP_State* state = &bop_prefetchers_array.bop_hwp_core_ul1[proc_id];
    BOP_History* history = &state->history;

    history->addresses[history->head] = lineAddr;
    history->head = (history->head + 1) % history->size;
    if (history->count < history->size) history->count++;

    bop_train(state, lineAddr);

    Addr prefetch_addr = lineAddr + state->best_offset;
    update_rr_table(state, prefetch_addr, global_access_counter);

    if (!pref_addto_ul1req_queue(proc_id, prefetch_addr, state->history.size)) {
        printf("UL1 prefetch failed for proc_id %u at addr %llx\n", proc_id, prefetch_addr);
    }
}

void pref_bop_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
    BOP_State* state = &bop_prefetchers_array.bop_hwp_core_ul1[proc_id];

    int* score = (int*)hash_table_access(&state->offset_table, state->best_offset);
    if (score) {
        (*score)++;
    }
}

void pref_bop_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
    BOP_State* state = &bop_prefetchers_array.bop_hwp_core_umlc[proc_id];
    BOP_History* history = &state->history;

    history->addresses[history->head] = lineAddr;
    history->head = (history->head + 1) % history->size;
    if (history->count < history->size) history->count++;

    bop_train(state, lineAddr);

    Addr prefetch_addr = lineAddr + state->best_offset;
    update_rr_table(state, prefetch_addr, global_access_counter);

    if (!pref_addto_umlc_req_queue(proc_id, prefetch_addr, state->history.size)) {
        printf("UMLC prefetch failed for proc_id %u at addr %llx\n", proc_id, prefetch_addr);
    }
}

void pref_bop_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist) {
    BOP_State* state = &bop_prefetchers_array.bop_hwp_core_umlc[proc_id];

    int* score = (int*)hash_table_access(&state->offset_table, state->best_offset);
    if (score) {
        (*score)++;
    }
}


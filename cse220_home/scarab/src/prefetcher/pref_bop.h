#ifndef __PREF_BOP_H__
#define __PREF_BOP_H__

#include "pref_common.h"
#include "libs/hash_lib.h"

#define SCORE_MAX 31
#define ROUND_MAX 100
#define RR_TABLE_SIZE 256
#define HISTORY_SIZE 1024
#define HASH_TABLE_SIZE 16384
#define TIMELINESS_THRESHOLD 10 

typedef struct {
    Addr* addresses;   
    int size;          
    int count;         
    int head;          
} BOP_History;

typedef struct {
    Addr tag;                
    unsigned long timestamp;
} RR_Entry;

typedef struct {
    Hash_Table offset_table;  
    BOP_History history;       
    int best_offset;         
    int round_count;          
    RR_Entry rr_table[RR_TABLE_SIZE]; 
} BOP_State;

typedef struct {
    BOP_State* bop_hwp_core_ul1;
    BOP_State* bop_hwp_core_umlc; 
} bop_prefetchers;

void pref_bop_init(HWP* hwp);
void pref_bop_ul1_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist);
void pref_bop_ul1_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist);
void pref_bop_umlc_miss(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist);
void pref_bop_umlc_prefhit(uns8 proc_id, Addr lineAddr, Addr loadPC, uns32 global_hist);

void bop_train(BOP_State* state, Addr lineAddr);
void update_rr_table(BOP_State* state, Addr baseAddr, unsigned long access_count);
int check_timeliness(BOP_State* state, Addr demandAddr, unsigned long access_count);
void increment_global_access_counter(void);

#endif /* __PREF_BOP_H__ */


#ifndef SIMPLE_SIM_UTILS_H
#define SIMPLE_SIM_UTILS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <assert.h>

#include <vector>
#include <list>

#include <string>

using namespace std;

// ----------------------- types ----------------------------

typedef struct {
    bool enable_rtlb;
    bool enable_l2_rtlb;
    bool enable_meta_l2;
    bool enable_mapping;
    bool enable_2M;

    // DTLB
    uint32_t l1_dtlb_set_num_4K;
    uint32_t l1_dtlb_associativity_4K;
    uint32_t l1_dtlb_set_num_2M;
    uint32_t l1_dtlb_associativity_2M;
    uint32_t l2_dtlb_set_num;
    uint32_t l2_dtlb_associativity;
    uint32_t cluster_factor;

    // RTLB
    uint32_t rtlb_entry_num;
    uint32_t l2_rtlb_set_num;
    uint32_t l2_rtlb_associativity;
    
    // META L2, compress the metadata of several pages into one entry
    uint32_t l2_idx_mod;
    uint32_t meta_idx_len;
    uint64_t meta_idx_mask; // meta_idx_mask = (1UL << meta_idx_len) - 1
} config_info;

typedef struct{
    bool valid;
    bool is_meta;
    uint64_t tag; // in L1 is vpn, in L2 is vpn with an extra bit indicating for translation of meta
    uint32_t lru_stamp;
    uint64_t bitmap; // for a translation entry, the lowest 2 bits represents its A/D bit
} tlb_entry;

typedef struct {
    bool valid;

    uint64_t seg_start;
    uint64_t seg_end;

    uint32_t main_tag;

    uint32_t lru_stamp;
} rtlb_entry;

typedef struct {
    bool active;
    uint64_t s_start;
    uint64_t s_end;
} subrange_entry;

typedef struct {
    uint32_t range_id;
    uint64_t r_start;
    uint64_t r_end;
    uint64_t r_size;

    vector<subrange_entry> subrange;
} range_entry;

enum tlb_hit_where {l1, l2, l2_miss, l1_r_hit, l2_r_hit, r_miss};

enum dtlb_type {l1_d, l2_d};

// ----------------------------------------------------

extern config_info sim_cfg;
extern const uint32_t TAG_SHIFT;
extern const uint64_t TAG_MASK;
extern const uint64_t PTR_MASK;

extern const uint32_t PAGE_SHIFT_4K;
extern const uint32_t PAGE_SHIFT_2M;

void read_config(char*);
void read_mem_config(char*);
void print_config();

bool inline isTagged(uint64_t addr)
{
    // return ((addr >> 63) & 0x1);
    if ((addr >> TAG_SHIFT) != 0)
        return true;
    else
        return false;
}

bool isPower2(uint32_t n);
int32_t floorLog2(uint32_t n);
int32_t ceilLog2(uint32_t n);

#endif
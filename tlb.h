#ifndef SIMPLE_SIM_TLB_H
#define SIMPLE_SIM_TLB_H

#include "utils.h"
#include "range_table.h"

// ---------------------------------------------------- 
class DTLB
{
public:
    DTLB();
    DTLB(uint32_t set_num, uint32_t associativity, dtlb_type tlb_type, uint32_t idx_mod=0);
    ~DTLB();

    tlb_hit_where lookup(uint64_t vaddr, uint8_t opcode, uint32_t page_shift);
    // void lookup_meta(uint64_t vaddr, uint8_t opcode);
    
    // update A/D bits for an evicted L1 entry in L2 TLB
    // void update_trans(uint64_t vaddr, uint64_t bitmap, uint32_t page_shift);

    int allocate(uint64_t vaddr, uint8_t opcode, uint32_t page_shift);
    // int allocate_meta(uint64_t vaddr);

    void set_next_level(DTLB *next_ptr);

    void print_meta_stat();

private:
    uint32_t m_set_num, m_associativity;

    dtlb_type m_type;
    tlb_entry** m_entries;
    DTLB* m_next_d;

    uint64_t m_meta_hit, m_meta_miss;
    uint64_t m_tr2meta, m_tr2tr, m_meta2tr, m_meta2meta;
    
    // L2 TLB is neither fully-inclusive or -exclusive, so evicted L1 entries may not exist in L2 TLB. In that case, the evicted L1 entries should trigger a write back to memory (if the dirty bit is set)
    uint64_t m_evtr_to_mem, m_evmeta_to_mem;
    uint64_t m_ev_passl2; // evicted L1 entries that fail to be filtered by L2
    
    uint32_t m_index_mod;
};

class RTLB
{
public:
    RTLB();
    RTLB(int l1_entries, bool meta_l2);
    RTLB(int l1_entries, int l2_set_num, int l2_associativity, bool meta_l2);
    void setRangeTable(RangeTable* range_table);
    void setMetaL2(DTLB* meta_l2);
    ~RTLB();

    tlb_hit_where lookup(uint32_t id, uint64_t vaddr, uint8_t opcode);

    void flushEntry(uint32_t id);

private:
    bool lookupL2(uint32_t id, uint64_t vaddr, rtlb_entry* where_to_put);
    rtlb_entry* allocateEntry();
    rtlb_entry* allocateEntryL2(int set_idx);

    bool m_enable_l2_rtlb;
    bool m_enable_meta_l2;

    int m_set_num, m_associativity;
    int m_l2_set_num, m_l2_associativity;

    rtlb_entry** m_entries;
    rtlb_entry** m_l2_entries;

    RangeTable* m_range_table;

    DTLB* m_meta_d;
};

#endif
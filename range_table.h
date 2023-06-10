#ifndef SIMPLE_SIM_RTABLE_H
#define SIMPLE_SIM_RTABLE_H

#include "utils.h"

// typedef struct {
//     bool active;
//     uint64_t s_start;
//     uint64_t s_end;
// } subrange_entry;

// typedef struct {
//     uint32_t range_id;
//     uint64_t r_start;
//     uint64_t r_end;
//     uint64_t r_size;
//     vector<subrange_entry> subrange;
// } range_entry;

typedef struct {
    uint64_t num_pages;
    int sub_count;
} mapping_seg;

typedef struct {
    uint32_t id;
    int64_t access_low;
    int64_t access_high;
    vector<mapping_seg> subs;
} mapping_info;

class RangeTable
{
public:
    RangeTable();
    RangeTable(char *mapping_dir, char *trace_dir);
    ~RangeTable();
    
    // All the virtual address used here have been preprocessed and have tags
    void insertEntry(uint32_t id, uint64_t va_start, uint64_t r_size);
    void freeEntry(uint32_t id, uint64_t va_start, uint64_t r_size);
    void remapEntry(uint32_t id, uint64_t old_va_start, uint64_t new_va_start, uint64_t old_r_size, uint64_t new_r_size);
    void fetch(uint32_t id, uint64_t vaddr, rtlb_entry *res);

    void printStat();

private:
    void _readMappings(range_entry &r_info);
    void _activateMappings(range_entry &r_info);
    void _insertMappings(range_entry &r_info, int p_low, int p_high, int a_low, int a_high, int seg_count, bool activate);

    char _mapping_dir[300];
    vector<range_entry> m_entries;
    vector<mapping_info> m_mappings;

    int max_count, cur_count;
    int max_search_len;
    int search_count;
    int *search_len_bucket;
};

#endif
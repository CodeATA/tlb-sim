#ifndef SIMPLE_SIM_RTABLE_H
#define SIMPLE_SIM_RTABLE_H

#include "utils.h"

using namespace std;

class RangeTable
{
public:
    RangeTable();
    ~RangeTable();
    
    void insertEntry(uint32_t id, uint64_t va_start, uint64_t r_size);
    void freeEntry(uint32_t id, uint64_t va_start, uint64_t r_size);
    void remapEntry(uint32_t id, uint64_t old_va_start, uint64_t old_r_size, uint64_t new_va_start, uint64_t new_r_size);

    int lookupAddr(uint64_t vaddr);
    bool lookupID(uint32_t id);

    void printStat();
    void printAccessStat(const char *a_path);

private:
    static const uint32_t TAG_SHIFT = 48;
    static const uint64_t TAG_MASK = 0x7FFF;
    static const uint64_t PTR_MASK = 0xFFFFFFFFFFFF;

    vector<range_entry> m_entries;
    vector<range_access> access_info;
};

#endif
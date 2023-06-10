#ifndef SIMPLE_SIM_UTILS_H
#define SIMPLE_SIM_UTILS_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>
#include <assert.h>

// #include <list>
#include <vector>
#include <algorithm>

// ----------------------- types ----------------------------

typedef struct {
    // bool valid;
    uint32_t id;
    uint64_t r_start;
    uint64_t r_end;
    uint64_t r_size;
} range_entry;

typedef struct {
    uint32_t id;
    int64_t page_low;  // page offset to the range base
    int64_t page_high;
} range_access;

// bool a_info_less (const range_access &a, const range_access &b)
// {
//     return a.id < b.id;
// }

// ----------------------------------------------------

bool isPower2(uint32_t n);
int32_t floorLog2(uint32_t n);
int32_t ceilLog2(uint32_t n);

extern uint64_t THRESHOLD;

#endif
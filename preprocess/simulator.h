#ifndef SIMPLE_SIM_TLBSIM_H
#define SIMPLE_SIM_TLBSIM_H

#include "utils.h"
#include "range_table.h"

class TLBSim
{
public:
    TLBSim();
    TLBSim(char* in_dir, char* out_dir, char* benchamrk, int region_num);
    ~TLBSim();

    void setRangeTable(RangeTable* ptr_rtable);

    void run();

private:
    static const uint32_t TAG_SHIFT = 48;
    static const uint64_t TAG_MASK = 0xFFFF;
    static const uint64_t PTR_MASK = 0xFFFFFFFFFFFF;

    RangeTable *range_table;

    // uint64_t i_threshold;

    int total_region;

    char in_name_base[300];
    char out_name_base[300];

    bool in_trace;
};

#endif
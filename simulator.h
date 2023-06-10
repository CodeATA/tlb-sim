#ifndef SIMPLE_SIM_TLBSIM_H
#define SIMPLE_SIM_TLBSIM_H

#include "utils.h"
#include "tlb.h"
#include "range_table.h"

// extern class DTLB;
// extern class RTLB;

typedef struct {
    float weight;

    double inst_load;
    double inst_store;
    
    double l1_dtlb_hit;
    double l1_dtlb_miss;
    double l2_dtlb_hit;
    double l2_dtlb_miss;

    double l1_rtlb_hit;
    double l1_rtlb_miss;
    double l2_rtlb_hit;
    double l2_rtlb_miss;
} region_data;

class TLBSim
{
public:
    TLBSim();
    TLBSim(char* trace_name, char* warm_name);
    ~TLBSim();

    void setDTLB_4K(DTLB* ptr_dtlb);
    void setDTLB_2M(DTLB* ptr_dtlb);
    void setRTLB(RTLB* ptr_rtlb);
    void setRangeTable(RangeTable* ptr_rtable);

    void run();
    void printRes();

private:
    std::vector<region_data> i_regions;
    int i_cur_region;
    int i_total_region;

    DTLB *l1_dtlb_4K;
    DTLB *l1_dtlb_2M;
    RTLB *rtlb;
    RangeTable *range_table;

    bool in_trace;

    FILE* i_trace_file;
    FILE* i_range_file;

    char i_bench_name[300];
};

#endif
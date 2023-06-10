#include "utils.h"
#include "simulator.h"
#include "tlb.h"
#include "range_table.h"

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: tlb-sim config_file trace_dir benchmark weight_file [mapping_file]\n");
        exit(0);
    }

    // char weight_file[300];
    char bench_name[300];

    sprintf(bench_name, "%s/%s", argv[2], argv[3]);
    // strcpy(weight_file, argv[4]);

    read_config(argv[1]);

    print_config();

    // responsible for initializing all parts
    // then hit the start button in the TLBSim class
    DTLB* l2_dtlb_ptr = new DTLB(sim_cfg.l2_dtlb_set_num, 
                                 sim_cfg.l2_dtlb_associativity,
                                 l2_d,
                                 sim_cfg.l2_idx_mod);
    DTLB* l1_dtlb_ptr_4K = new DTLB(sim_cfg.l1_dtlb_set_num_4K,
                                    sim_cfg.l1_dtlb_associativity_4K,
                                    l1_d);
    
    RangeTable* r_table_ptr = NULL;
    if (sim_cfg.enable_mapping && sim_cfg.enable_rtlb)
    {
        if (argc != 6)
        {
            fprintf(stderr, "Need to provide the directory of mapping files\n");
            exit(0);
        }
        r_table_ptr = new RangeTable(argv[5], argv[2]);
    }
    else
        r_table_ptr = new RangeTable();
    
    RTLB* rtlb_ptr = NULL;
    if (sim_cfg.enable_l2_rtlb)
        rtlb_ptr = new RTLB(sim_cfg.rtlb_entry_num,
                            sim_cfg.l2_rtlb_set_num,
                            sim_cfg.l2_rtlb_associativity,
                            sim_cfg.enable_meta_l2);
    else
        rtlb_ptr = new RTLB(sim_cfg.rtlb_entry_num,
                            sim_cfg.enable_meta_l2);
    rtlb_ptr->setRangeTable(r_table_ptr);

    if (sim_cfg.enable_meta_l2)
        rtlb_ptr->setMetaL2(l2_dtlb_ptr);

    l1_dtlb_ptr_4K->set_next_level(l2_dtlb_ptr);

    TLBSim* sim_ptr = new TLBSim(bench_name, argv[4]);
    sim_ptr->setDTLB_4K(l1_dtlb_ptr_4K);
    if (sim_cfg.enable_2M)
    {
        DTLB* l1_dtlb_ptr_2M = new DTLB(sim_cfg.l1_dtlb_set_num_2M,
                                        sim_cfg.l1_dtlb_associativity_2M,
                                        l1_d);
        l1_dtlb_ptr_2M->set_next_level(l2_dtlb_ptr);
        sim_ptr->setDTLB_2M(l1_dtlb_ptr_2M);
    }
    sim_ptr->setRTLB(rtlb_ptr);
    sim_ptr->setRangeTable(r_table_ptr);

    sim_ptr->run();
    printf("\n");
    sim_ptr->printRes();
    
    if (sim_cfg.enable_meta_l2)
        l2_dtlb_ptr->print_meta_stat();
    
    r_table_ptr->printStat();
    printf("\n");

    delete l2_dtlb_ptr;
    delete l1_dtlb_ptr_4K;
    delete r_table_ptr;
    delete rtlb_ptr;
    delete sim_ptr;
    return 0;
}
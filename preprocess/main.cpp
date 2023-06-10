#include "utils.h"
#include "simulator.h"
// #include "tlb.h"

int main(int argc, char** argv)
{
    if (argc < 6)
    {
        fprintf(stderr, "Usage: preprocess in_dir out_dir benchmark threshold region_num\n");
        exit(0);
    }
    
    char* in_dir = argv[1];
    char* out_dir = argv[2];
    char* benchmark = argv[3];

    uint64_t threshold;
    sscanf(argv[4], "%lu", &THRESHOLD);

    int region_num;
    sscanf(argv[5], "%d", &region_num);

    // responsible for initializing all parts
    // then hit the start button in the TLBSim class
    RangeTable* r_table_ptr = new RangeTable();

    TLBSim* sim_ptr = new TLBSim(in_dir, out_dir, benchmark, region_num);
    sim_ptr->setRangeTable(r_table_ptr);
    sim_ptr->run();
    r_table_ptr->printAccessStat(out_dir);

    delete r_table_ptr;
    delete sim_ptr;

    return 0;
}
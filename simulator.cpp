#include "simulator.h"

TLBSim::TLBSim()
{
}

TLBSim::TLBSim(char* trace_name, char* weight_file)
{
    region_data tmp;
    tmp.inst_load = 0;
    tmp.inst_store = 0;
    tmp.l1_dtlb_hit = 0;
    tmp.l1_dtlb_miss = 0;
    tmp.l2_dtlb_hit = 0;
    tmp.l2_dtlb_miss = 0;
    tmp.l1_rtlb_hit = 0;
    tmp.l1_rtlb_miss = 0;
    tmp.l2_rtlb_hit = 0;
    tmp.l2_rtlb_miss = 0;

    l1_dtlb_4K = NULL;
    l1_dtlb_2M = NULL;
    rtlb = NULL;
    i_trace_file = NULL;
    i_range_file = NULL;

    i_cur_region = 0;

    FILE *weights = fopen(weight_file, "r");
    if (weights == NULL)
    {
        fprintf(stderr, "Fail to open weight file\n");
        exit(-1);
    }
    char line[300];
    fgets(line, 300, weights);
    sscanf(line, "%d", &i_total_region);
    for (int i = 0; i < i_total_region; i++)
    {
        fgets(line, 300, weights);
        sscanf(line, "%*u,%*u,%f", &tmp.weight);
        i_regions.push_back(tmp);
    }

    strcpy(i_bench_name, trace_name);
}

TLBSim::~TLBSim()
{
    if (i_trace_file != NULL)
    {
        fclose(i_trace_file);
    }

    if (i_range_file != NULL)
    {
        fclose(i_range_file);
    }

    i_regions.clear();
}

void TLBSim::setDTLB_4K(DTLB* ptr_dtlb)
{
    assert(ptr_dtlb != NULL);
    l1_dtlb_4K = ptr_dtlb;
}

void TLBSim::setDTLB_2M(DTLB* ptr_dtlb)
{
    assert(ptr_dtlb != NULL);
    l1_dtlb_2M = ptr_dtlb;
}

void TLBSim::setRTLB(RTLB* ptr_rtlb)
{
    assert(ptr_rtlb != NULL);
    rtlb = ptr_rtlb;
}

void TLBSim::setRangeTable(RangeTable* ptr_rtable)
{
    assert(ptr_rtable != NULL);
    range_table = ptr_rtable;
}

void TLBSim::run()
{
    // read opcode
    // read ID
    // read instruction
    // execute instruction

    in_trace = false;

    FILE* in_file;
    char i_input_file_name[300];
    sprintf(i_input_file_name, "%s.range.0", i_bench_name);
    in_file = fopen(i_input_file_name, "rb");
    if (in_file == NULL)
    {
        fprintf(stderr, "[TLBSim] Fail to open input file: %s\n", i_input_file_name);
        exit(-1);
    }

    uint8_t opcode;
    uint32_t id;
    uint64_t rw_addr;
    uint64_t old_addr, ret_addr, size_1, size_2;

    printf("Total region number: %d\n", i_total_region);

    while (true)
    {
        if (fread(&opcode, 1, 1, in_file) != 1)
        {
            if (!feof(in_file))
            {
                fprintf(stderr, "[TLBSim] Cannot read opcode (not EOF)\n");
                exit(-1);
            }
        
            // current file finished, change input file
            fclose(in_file);
            if (in_trace)
            {
                printf("  Region %d finished\n", i_cur_region);
                i_cur_region++;
                if (i_cur_region == i_total_region)
                {
                    // finished the final region
                    break;
                }
                else
                {
                    // open next range file
                    sprintf(i_input_file_name, "%s.range.%d", i_bench_name, i_cur_region);
                    in_trace = false;
                }
            }
            else
            {
                // open next trace file
                sprintf(i_input_file_name, "%s.trace.%d", i_bench_name, i_cur_region);
                in_trace = true;
            }

            in_file = fopen(i_input_file_name, "rb");
            if (in_file == NULL)
            {
                fprintf(stderr, "[TLBSim] Fail to open input file: %s\n", i_input_file_name);
                exit(-1);
            }

            // check again to skip empty range file
            continue;
        }

        switch (opcode)
        {
        case 0:
        case 1:
        {
            fread(&id, 4, 1, in_file);
            fread(&rw_addr, 8, 1, in_file);
            
            if (opcode == 0)
                i_regions[i_cur_region].inst_load++;
            else if (opcode == 1)
                i_regions[i_cur_region].inst_store++;

            // uint32_t tag = (rw_addr >> TAG_SHIFT) & TAG_MASK;
            // printf("rw tag: %u\n", tag);
            tlb_hit_where hit_res;

            if (!sim_cfg.enable_rtlb)
            {
                if (id == 0)
                    hit_res = l1_dtlb_4K->lookup(rw_addr, opcode, PAGE_SHIFT_4K);
                else if (sim_cfg.enable_2M)
                    hit_res = l1_dtlb_2M->lookup(rw_addr, opcode, PAGE_SHIFT_2M);
                else
                    hit_res = l1_dtlb_4K->lookup(rw_addr, opcode, PAGE_SHIFT_4K);
            }
            else
            {
                if (id == 0)
                    hit_res = l1_dtlb_4K->lookup(rw_addr, opcode, PAGE_SHIFT_4K);
                else
                    hit_res = rtlb->lookup(id, rw_addr, opcode);
            }

            // if (!sim_cfg.enable_rtlb || id == 0)
            // {
            //     // no tag, use dtlb
            //     rw_addr = rw_addr & PTR_MASK;
            //     hit_res = l1_dtlb_4K->lookup(rw_addr, opcode);
            // }
            // else
            // {
            //     // has tag, use rtlb
            //     hit_res = rtlb->lookup(id, rw_addr, opcode);
            // }

            if (!in_trace) // skip counting in range files
                break;

            switch (hit_res)
            {
            case l1:
                i_regions[i_cur_region].l1_dtlb_hit++;
                break;
            case l2:
                i_regions[i_cur_region].l1_dtlb_miss++;
                i_regions[i_cur_region].l2_dtlb_hit++;
                break;
            case l2_miss:
                i_regions[i_cur_region].l1_dtlb_miss++;
                i_regions[i_cur_region].l2_dtlb_miss++;
                break;
            case l1_r_hit:
                i_regions[i_cur_region].l1_rtlb_hit++;
                break;
            case l2_r_hit:
                assert(sim_cfg.enable_l2_rtlb);
                i_regions[i_cur_region].l1_rtlb_miss++;
                i_regions[i_cur_region].l2_rtlb_hit++;
                break;
            case r_miss:
                if (sim_cfg.enable_l2_rtlb)
                {
                    // L2 RTLB enabled, a miss should count as two
                    i_regions[i_cur_region].l1_rtlb_miss++;
                    i_regions[i_cur_region].l2_rtlb_miss++;
                }
                else
                {
                    // L2 RTLB disabled, miss only happens in L1 RTLB
                    i_regions[i_cur_region].l1_rtlb_miss++;
                }
                break;
            default:
                break;
            }
            break;
        }
        case 2:
        {
            // mmap
            fread(&id, 4, 1, in_file);
            fread(&ret_addr, 8, 1, in_file);
            fread(&size_1, 8, 1, in_file);
            
            if (!sim_cfg.enable_rtlb)
                break;
            // fprintf(stderr, "mmap, ret_addr: 0x%lx, size: %lu\n", ret_addr, size_1);
            range_table->insertEntry(id, ret_addr, size_1);
            break;
        }
        case 3:
        {
            // mremap
            fread(&id, 4, 1, in_file);
            fread(&old_addr, 8, 1, in_file);
            fread(&size_1, 8, 1, in_file);
            fread(&ret_addr, 8, 1, in_file);
            fread(&size_2, 8, 1, in_file);
            
            if (!sim_cfg.enable_rtlb)
                break;
            // preprocess does not need to consider the generation of child segments
            // because child segments share the same main segment id
            // fprintf(stderr, "mremap, old_addr: 0x%lx, size_1: %lu, new_addr: 0x%lx, size_2: %lu\n", old_addr, size_1, ret_addr, size_2);
            range_table->remapEntry(id, old_addr, ret_addr, size_1, size_2);
            // flush the corresponding rtlb entries
            rtlb->flushEntry(id);
            break;
        }
        case 4:
        {
            // munmap
            fread(&id, 4, 1, in_file);
            fread(&old_addr, 8, 1, in_file);
            fread(&size_1, 8, 1, in_file);
            if (!sim_cfg.enable_rtlb)
                break;
            // fprintf(stderr, "munmap, ret_addr: 0x%lx, size: %lu\n", old_addr, size_1);
            range_table->freeEntry(id, old_addr, size_1);
            rtlb->flushEntry(id);
            break;
        }
        default:
            fprintf(stderr, "Unknown instruction!\n");
            exit(-1);
        }
    }
    return;
}

void TLBSim::printRes()
{
    char i_res_name[300];
    FILE *res_file = NULL;

    region_data weighted_res;
    weighted_res.weight = 0;
    weighted_res.inst_load = 0;
    weighted_res.inst_store = 0;
    weighted_res.l1_dtlb_hit = 0;
    weighted_res.l1_dtlb_miss = 0;
    weighted_res.l2_dtlb_hit = 0;
    weighted_res.l2_dtlb_miss = 0;
    weighted_res.l1_rtlb_hit = 0;
    weighted_res.l1_rtlb_miss = 0;
    weighted_res.l2_rtlb_hit = 0;
    weighted_res.l2_rtlb_miss = 0;

    for (int i = 0; i < i_total_region; i++)
    {
        sprintf(i_res_name, "%s.res.%d", i_bench_name, i);
        res_file = fopen(i_res_name, "w");
        if (res_file == NULL)
        {
            fprintf(stderr, "[TLBSim] Fail to open res log file: %s\n", i_res_name);
            exit(-1);
        }
        fprintf(res_file, "region weight: %f\n\n", i_regions[i].weight);

        fprintf(res_file, "inst_load: %lu\n", (uint64_t)i_regions[i].inst_load);
        fprintf(res_file, "inst_store: %lu\n", (uint64_t)i_regions[i].inst_store);
        fprintf(res_file, "l1_dtlb_hit: %lu\n", (uint64_t)i_regions[i].l1_dtlb_hit);
        fprintf(res_file, "l1_dtlb_miss: %lu\n", (uint64_t)i_regions[i].l1_dtlb_miss);
        fprintf(res_file, "l2_dtlb_hit: %lu\n", (uint64_t)i_regions[i].l2_dtlb_hit);
        fprintf(res_file, "l2_dtlb_miss: %lu\n\n", (uint64_t)i_regions[i].l2_dtlb_miss);

        if (sim_cfg.enable_l2_rtlb)
        {
            fprintf(res_file, "l1_rtlb_hit: %lu\n", (uint64_t)i_regions[i].l1_rtlb_hit);
            fprintf(res_file, "l1_rtlb_miss: %lu\n", (uint64_t)i_regions[i].l1_rtlb_miss);
            fprintf(res_file, "l2_rtlb_hit: %lu\n", (uint64_t)i_regions[i].l2_rtlb_hit);
            fprintf(res_file, "l2_rtlb_miss: %lu\n", (uint64_t)i_regions[i].l2_rtlb_miss);
        }
        else
        {
            fprintf(res_file, "l1_rtlb_hit: %lu\n", (uint64_t)i_regions[i].l1_rtlb_hit);
            fprintf(res_file, "l1_rtlb_miss: %lu\n", (uint64_t)i_regions[i].l1_rtlb_miss);
        }
        fclose(res_file);

        weighted_res.weight += i_regions[i].weight;
        weighted_res.inst_load += i_regions[i].inst_load * i_regions[i].weight;
        weighted_res.inst_store += i_regions[i].inst_store * i_regions[i].weight;
        weighted_res.l1_dtlb_hit += i_regions[i].l1_dtlb_hit * i_regions[i].weight;
        weighted_res.l1_dtlb_miss += i_regions[i].l1_dtlb_miss * i_regions[i].weight;
        weighted_res.l2_dtlb_hit += i_regions[i].l2_dtlb_hit * i_regions[i].weight;
        weighted_res.l2_dtlb_miss += i_regions[i].l2_dtlb_miss * i_regions[i].weight;
        weighted_res.l1_rtlb_hit += i_regions[i].l1_rtlb_hit * i_regions[i].weight;
        weighted_res.l1_rtlb_miss += i_regions[i].l1_rtlb_miss * i_regions[i].weight;
        weighted_res.l2_rtlb_hit += i_regions[i].l2_rtlb_hit * i_regions[i].weight;
        weighted_res.l2_rtlb_miss += i_regions[i].l2_rtlb_miss * i_regions[i].weight;
    }

    printf("weighted summary:\n");
    printf("l1_dtlb_hit: %lf\n", weighted_res.l1_dtlb_hit);
    printf("l1_dtlb_miss: %lf\n", weighted_res.l1_dtlb_miss);
    printf("l2_dtlb_hit: %lf\n", weighted_res.l2_dtlb_hit);
    printf("l2_dtlb_miss: %lf\n\n", weighted_res.l2_dtlb_miss);
    
    if (sim_cfg.enable_l2_rtlb)
    {
        printf("l1_rtlb_hit: %lf\n", weighted_res.l1_rtlb_hit);
        printf("l1_rtlb_miss: %lf\n", weighted_res.l1_rtlb_miss);
        printf("l2_rtlb_hit: %lf\n", weighted_res.l2_rtlb_hit);
        printf("l2_rtlb_miss: %lf\n", weighted_res.l2_rtlb_miss);
    }
    else
    {
        printf("l1_rtlb_hit: %lf\n", weighted_res.l1_rtlb_hit);
        printf("l1_rtlb_miss: %lf\n", weighted_res.l1_rtlb_miss);
    }

    return;
}
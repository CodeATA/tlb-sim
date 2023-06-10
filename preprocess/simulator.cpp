#include "simulator.h"

TLBSim::TLBSim()
{
}

TLBSim::TLBSim(char* in_dir, char* out_dir, char* benchmark, int region_num)
{
    total_region = region_num;

    sprintf(in_name_base, "%s/%s", in_dir, benchmark);
    sprintf(out_name_base, "%s/%s", out_dir, benchmark);

    in_trace = false;
}

TLBSim::~TLBSim()
{
}

void TLBSim::setRangeTable(RangeTable* ptr_rtable)
{
    assert(ptr_rtable != NULL);
    range_table = ptr_rtable;
}

void TLBSim::run()
{
    // read opcode
    // read instruction
    // execute instruction
    FILE* in_file;
    FILE* out_file;
    char input_cmd[300];
    char output_name[300];

    uint8_t opcode;
    uint32_t id;
    uint64_t rw_addr;
    uint64_t old_addr, ret_addr, size_1, size_2;
    uint32_t range_id;

    int cur_region = 0;

    sprintf(input_cmd, "%s.range.0", in_name_base);
    sprintf(output_name, "%s.range.0", out_name_base);
    in_file = fopen(input_cmd, "rb");
    if (in_file == NULL)
    {
        fprintf(stderr, "Fail to open: %s", input_cmd);
        exit(-1);
    }
    out_file = fopen(output_name, "wb");
    if (out_file == NULL)
    {
        fprintf(stderr, "Fail to open: %s", output_name);
        exit(-1);
    }
    in_trace = false;

    while (true)
    {
        if (fread(&opcode, 1, 1, in_file) != 1)
        {
            if (!feof(in_file))
            {
                fprintf(stderr, "[TLBSim] Cannot read opcode (not EOF)\n");
                exit(-1);
            }

            // pclose(in_file);
            fclose(in_file);
            fclose(out_file);
            if (in_trace)
            {
                printf("  Region %d finished\n", cur_region);
                cur_region++;
                if (cur_region == total_region)
                {
                    break;
                }
                else
                {
                    // sprintf(input_cmd, "gzip -dc %s.range.%d.gz", in_name_base, cur_region);
                    sprintf(input_cmd, "%s.range.%d", in_name_base, cur_region);
                    sprintf(output_name, "%s.range.%d", out_name_base, cur_region);
                    in_trace = false;
                }
            }
            else
            {
                // sprintf(input_cmd, "gzip -dc %s.trace.%d.gz", in_name_base, cur_region);
                printf("  Range %d finished\n", cur_region);
                sprintf(input_cmd, "%s.trace.%d", in_name_base, cur_region);
                sprintf(output_name, "%s.trace.%d", out_name_base, cur_region);
                in_trace = true;
            }

            // in_file = popen(input_cmd, "r");
            in_file = fopen(input_cmd, "rb");
            if (in_file == NULL)
            {
                fprintf(stderr, "[TLBSim] Fail to open: %s", input_cmd);
                exit(-1);
            }
            out_file = fopen(output_name, "wb");
            if (out_file == NULL)
            {
                fprintf(stderr, "[TLBSim] Fail to open: %s", output_name);
                exit(-1);
            }
            continue;
        }

        switch (opcode)
        {
        case 0:
        case 1:
        {
            fread(&rw_addr, 8, 1, in_file);
#ifndef ONLY_CHECK
            range_id = range_table->lookupAddr(rw_addr);
            // if (range_id > 100)
            // {
            //     fprintf(stderr, "    search for 0x%lx returns %u\n", rw_addr, range_id);
            //     // rw_addr = rw_addr | (0x1UL << 63);
            //     getchar();
            // }
            fwrite(&opcode, 1, 1, out_file);
            fwrite(&range_id, 4, 1, out_file);
            fwrite(&rw_addr, 8, 1, out_file);
#endif
            break;
        }
        case 2:
        {
            // mmap
            fread(&id, 4, 1, in_file);
            fread(&ret_addr, 8, 1, in_file);
            fread(&size_1, 8, 1, in_file);

            if (size_1 > THRESHOLD)
            {
                range_table->insertEntry(id, ret_addr, size_1);
                // printf("[MMAP] size: %lu, ret: 0x%lx\n", size_1, ret_addr);
                // range_table->printStat();
                // printf("\n");
                // getchar();
#ifndef ONLY_CHECK
                // ret_addr = ret_addr | (range_id << TAG_SHIFT);
                // ret_addr = ret_addr | (0x1 << 63);
                fwrite(&opcode, 1, 1, out_file);
                fwrite(&id, 4, 1, out_file);
                fwrite(&ret_addr, 8, 1, out_file);
                fwrite(&size_1, 8, 1, out_file);
#endif
            }
            else
            {
                printf("  Under thresholod: mmap 0x%lx %lu\n", ret_addr, size_1);
            }
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

            if (range_table->lookupID(id))
            {
                // remapping an object above the threshold
                range_table->remapEntry(id, old_addr, size_1, ret_addr, size_2);
#ifndef ONLY_CHECK
                fwrite(&opcode, 1, 1, out_file);
                fwrite(&id, 4, 1, out_file);
                fwrite(&old_addr, 8, 1, out_file);
                fwrite(&size_1, 8, 1, out_file);
                fwrite(&ret_addr, 8, 1, out_file);
                fwrite(&size_2, 8, 1, out_file);
#endif
            }
            // lookupID() cannot find the object, so size_1 is below the threshold
            else if (size_2 > THRESHOLD)
            {
                // object grow across the threshold, treat as mmap
                printf("  Cross threshold: remap 0x%lx %lu 0x%lx %lu\n", old_addr, size_1, ret_addr, size_2);
                range_table->insertEntry(id, ret_addr, size_2);
#ifndef ONLY_CHECK
                // write an mmap instruction into the trace
                opcode = 2;
                fwrite(&opcode, 1, 1, out_file);
                fwrite(&id, 4, 1, out_file);
                fwrite(&ret_addr, 8, 1, out_file);
                fwrite(&size_2, 8, 1, out_file);
#endif
            }
            else
            {
                printf("  Under threshold: remap 0x%lx %lu 0x%lx %lu\n", old_addr, size_1, ret_addr, size_2);
            }
            break;
        }
        case 4:
        {
            // munmap
            fread(&id, 4, 1, in_file);
            fread(&old_addr, 8, 1, in_file);
            fread(&size_1, 8, 1, in_file);

            if (range_table->lookupID(id))
            {
                range_table->freeEntry(id, old_addr, size_1);
#ifndef ONLY_CHECK
                fwrite(&opcode, 1, 1, out_file);
                fwrite(&id, 4, 1, out_file);
                fwrite(&old_addr, 8, 1, out_file);
                fwrite(&size_1, 8, 1, out_file);
#endif
            }
            else
            {
                printf("  Under threshold: unmap 0x%lx %lu\n", old_addr, size_1);
            }
            break;
        }
        default:
            fprintf(stderr, "Unknown instruction! (%u)\n", opcode);
            exit(-1);
        }
    }
    return;
}
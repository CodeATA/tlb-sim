#include "utils.h"

config_info sim_cfg;

const uint32_t TAG_SHIFT = 48;
const uint64_t TAG_MASK = 0xFFFF;
const uint64_t PTR_MASK = 0xFFFFFFFFFFFF;

const uint32_t PAGE_SHIFT_4K = 12;
const uint32_t PAGE_SHIFT_2M = 21;

void read_config(char* cfg_name)
{
    FILE* cfg_file = NULL;
    cfg_file = fopen(cfg_name, "r");
    if (cfg_file == NULL)
    {
        fprintf(stderr, "Cannot open config file: %s\n", cfg_name);
        exit(-1);
    }

    enum config_part {global, dtlb, rtlb, meta_l2};
    config_part cur_part;

    char line[80];
    while(fgets(line, 80, cfg_file) != NULL)
    {
        if (line[0] == '\n' || line[0] == '#')
            continue;

        if (strcmp(line, "[DTLB]\n") == 0)
        {
            cur_part = dtlb;
            continue;
        }
        else if (strcmp(line, "[RTLB]\n") == 0)
        {
            cur_part = rtlb;
            continue;
        }
        else if (strcmp(line, "[META L2]\n") == 0)
        {
            cur_part = meta_l2;
            continue;
        }

        char field[40];
        // uint32_t value;
        sscanf(line, "%[^:]", field);

        // ======= DTLB =======
        if (strcmp(field, "enable_2M") == 0)
        {
            uint32_t value;
            sscanf(line, "%*s %u", &value);
            if (value != 0)
                sim_cfg.enable_2M = true;
            else
                sim_cfg.enable_2M = false;
        }
        else if (strcmp(field, "L1_set_num_4K") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l1_dtlb_set_num_4K);
        }
        else if (strcmp(field, "L1_associativity_4K") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l1_dtlb_associativity_4K);
        }
        else if (strcmp(field, "L1_set_num_2M") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l1_dtlb_set_num_2M);
        }
        else if (strcmp(field, "L1_associativity_2M") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l1_dtlb_associativity_2M);
        }
        else if (strcmp(field, "L2_set_num") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l2_dtlb_set_num);
        }
        else if (strcmp(field, "L2_associativity") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l2_dtlb_associativity);
        }
        else if (strcmp(field, "cluster_factor") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.cluster_factor);
        }
        // ======= DTLB =======
        // ======= RTLB =======
        else if (strcmp(field, "enable_L1") == 0)
        {
            uint32_t value;
            sscanf(line, "%*s %u", &value);
            if (value != 0)
                sim_cfg.enable_rtlb = true;
            else
                sim_cfg.enable_rtlb = false;
        }
        else if (strcmp(field, "enable_L2") == 0)
        {
            uint32_t value;
            sscanf(line, "%*s %u", &value);
            if (value != 0)
                sim_cfg.enable_l2_rtlb = true;
            else
                sim_cfg.enable_l2_rtlb = false;
        }
        else if (strcmp(field, "L1_entry_num") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.rtlb_entry_num);
        }
        else if (strcmp(field, "L2_RTLB_set_num") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l2_rtlb_set_num);
        }
        else if (strcmp(field, "L2_RTLB_associativity") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l2_rtlb_associativity);
        }
        // ======= RTLB =======
        // ======= META L2 =======
        else if (strcmp(field, "enable_meta") == 0)
        {
            uint32_t value;
            sscanf(line, "%*s %u", &value);
            if (value != 0)
                sim_cfg.enable_meta_l2 = true;
            else
                sim_cfg.enable_meta_l2 = false;
        }
        else if (strcmp(field, "L2_meta_idx_mod") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.l2_idx_mod);
        }
        else if (strcmp(field, "L2_meta_len") == 0)
        {
            sscanf(line, "%*s %u", &sim_cfg.meta_idx_len);
            sim_cfg.meta_idx_mask = (1UL << sim_cfg.meta_idx_len) - 1;
        }
        // ======= META L2 =======
        // ======= MAPPING =======
        else if (strcmp(field, "enable_mapping") == 0)
        {
            uint32_t value;
            sscanf(line, "%*s %u", &value);
            if (value != 0)
                sim_cfg.enable_mapping = true;
            else
                sim_cfg.enable_mapping = false;
        }
        // ======= MAPPING =======
        else
        {
            fprintf(stderr, "[config]: unknown field \"%s\"\n", field);
            exit(-1);
        }
    }
    fclose(cfg_file);
}

void print_config()
{
    printf("======== Config ========\n");
    // printf("Page Size: %u\n", 1U << sim_cfg.page_shift);
    
    printf("[DTLB]\n");
    printf("L1 set number (4K): %u\n", sim_cfg.l1_dtlb_set_num_4K);
    printf("L1 associativity (4K): %u\n", sim_cfg.l1_dtlb_associativity_4K);
    if (sim_cfg.enable_2M)
    {
        printf("L1 set number (2M): %u\n", sim_cfg.l1_dtlb_set_num_2M);
        printf("L1 associativity (2M): %u\n", sim_cfg.l1_dtlb_associativity_2M);
    }
    printf("L2 set number: %u\n", sim_cfg.l2_dtlb_set_num);
    printf("L2 associativity: %u\n", sim_cfg.l2_dtlb_associativity);
    printf("cluster factor: %u\n", sim_cfg.cluster_factor);

    if (sim_cfg.enable_rtlb)
    {
        printf("\n[RTLB]\n");
        printf("entry number: %u\n", sim_cfg.rtlb_entry_num);
        if (sim_cfg.enable_l2_rtlb)
        {
            printf("L2 RTLB set number: %u\n", sim_cfg.l2_rtlb_set_num);
            printf("L2 RTLB associativity: %u\n", sim_cfg.l2_rtlb_associativity);
        }
    }

    if (sim_cfg.enable_meta_l2)
    {
        printf("\n[META L2]\n");
        printf("index modification: %u\n", sim_cfg.l2_idx_mod);
        printf("entry number in one meta entry: %lu\n", 1UL << sim_cfg.meta_idx_len);
        printf("meta entry mask: 0x%lx\n", sim_cfg.meta_idx_mask);
    }
    
    printf("======== Config ========\n\n");
}

bool isPower2(uint32_t n)
{ 
    return ((n & (n - 1)) == 0);
}


int32_t floorLog2(uint32_t n)
{
   int32_t p = 0;

   if (n == 0) return -1;

   if (n & 0xffff0000) { p += 16; n >>= 16; }
   if (n & 0x0000ff00) { p +=  8; n >>=  8; }
   if (n & 0x000000f0) { p +=  4; n >>=  4; }
   if (n & 0x0000000c) { p +=  2; n >>=  2; }
   if (n & 0x00000002) { p +=  1; }

   return p;
}

int32_t ceilLog2(uint32_t n)
{
    return floorLog2(n - 1) + 1;
}


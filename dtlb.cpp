#include "tlb.h"

DTLB::DTLB()
{
}

DTLB::DTLB(uint32_t set_num, uint32_t associativity, dtlb_type tlb_type, uint32_t idx_mod)
{
    m_set_num = set_num;
    m_associativity = associativity;
    m_type = tlb_type;

    m_next_d = NULL;

    m_entries = new tlb_entry*[m_set_num];
    for (int i = 0; i < m_set_num; i++)
    {
        m_entries[i] = new tlb_entry[m_associativity];
        for (int j = 0; j < m_associativity; j++)
        {
            m_entries[i][j].valid = false;
            m_entries[i][j].is_meta = false;
            m_entries[i][j].bitmap = 0;
        }
    }

    // for compressed meta entries in L2 TLB
    m_meta_hit = m_meta_miss = 0;
    m_tr2meta = m_tr2tr = 0;
    m_meta2tr = m_meta2meta = 0;

    m_index_mod = idx_mod;
}

DTLB::~DTLB()
{
    for (int i = 0; i < m_set_num; i++)
    {
        delete m_entries[i];
    }
    delete m_entries;
}

tlb_hit_where DTLB::lookup(uint64_t vaddr, uint8_t opcode, uint32_t page_shift)
{
    uint64_t index, tag;
    uint64_t raw_vaddr = vaddr & PTR_MASK;

    uint64_t index_mask = m_set_num - 1; // make sure m_set_num is a power of 2

    // hardware may concatenate remaining bits instead of using the whole vpn as tag
    int basic_shift = page_shift + sim_cfg.cluster_factor;
    tag = raw_vaddr >> basic_shift;
    if (m_type == l1_d)
    {
        index = (raw_vaddr >> basic_shift) & index_mask;
    }
    else
    {
        index = (raw_vaddr >> (basic_shift + m_index_mod)) & index_mask;
        // add one extra bit to indicate whether or not a meta entry
        // useful to distinguish a meta entry from the translation of a 64K-aligned page
        // for a translation entry, this bit is always 0
        tag = tag << 1;
    }

    for (int i = 0; i < m_associativity; i++)
    {
        if (!m_entries[index][i].valid)
            continue;
        if (m_entries[index][i].tag == tag)
        {
            assert(!m_entries[index][i].is_meta);

            // a hit, update lru_stamps
            for (int j = 0; j < m_associativity; j++)
            {
                if (!m_entries[index][j].valid)
                    continue;
                if (j != i)
                    m_entries[index][j].lru_stamp++;
            }
            if (m_type == l1_d)
            {
                switch(opcode)
                {
                case 0:
                    m_entries[index][i].bitmap |= 0b10;
                    break;
                case 1:
                    m_entries[index][i].bitmap |= 0b11;
                    break;
                default:
                    fprintf(stderr, "L1 lookup: unknown opcode %u\n", opcode);
                    exit(-1);
                }
                return l1;
            }
            else
            {
                switch(opcode)
                {
                case 0:
                    m_entries[index][i].bitmap |= 0b10;
                    break;
                case 1:
                    m_entries[index][i].bitmap |= 0b11;
                    break;
                default:
                    fprintf(stderr, "L2 lookup: unknown opcode %u\n", opcode);
                    exit(-1);
                }
                return l2;
            }
        }
    }
    // a miss
    int entry_pos = allocate(vaddr, opcode, page_shift);
    assert(!m_entries[index][entry_pos].is_meta);
    switch(opcode)
    {
    case 0:
        m_entries[index][entry_pos].bitmap |= 0b10;
        break;
    case 1:
        m_entries[index][entry_pos].bitmap |= 0b11;
        break;
    default:
        fprintf(stderr, "L2 lookup: unknown opcode %u\n", opcode);
        exit(-1);
    }
    if (m_type == l1_d)
    {
        return m_next_d->lookup(vaddr, opcode, page_shift);
    }
    else
    {
        return l2_miss;
    }
}

int DTLB::allocate(uint64_t vaddr, uint8_t opcode, uint32_t page_shift)
{
    uint64_t index, tag;
    uint64_t index_mask = m_set_num - 1; // make sure m_set_num is a power of 2

    // uint32_t index_len = ceilLog2(m_set_num); 

    uint64_t raw_vaddr = vaddr & PTR_MASK;

    // hardware may concatenate remaining bits instead of using the whole vpn as tag
    int basic_shift = page_shift + sim_cfg.cluster_factor;
    tag = raw_vaddr >> basic_shift;
    if (m_type == l1_d)
    {
        index = (raw_vaddr >> basic_shift) & index_mask;
    }
    else
    {
        index = (raw_vaddr >> (basic_shift + m_index_mod)) & index_mask;
        // add one extra bit to indicate whether or not a meta entry
        // useful to distinguish a meta entry from the translation of a 64K-aligned page
        // for a translation entry, this bit is always 0
        tag = tag << 1;
    }

    int i = 0;
    int evict_stamp = 0, evict_idx = 0;
    for (i = 0; i < m_associativity; i++)
    {
        // try to find a free entry
        if (!m_entries[index][i].valid)
        {
            // found a free entry
            m_entries[index][i].valid = true;
            m_entries[index][i].tag = tag;
            m_entries[index][i].is_meta = false;
            // update lru_stamps
            for (int j = 0; j < m_associativity; j++)
            {
                if (!m_entries[index][j].valid)
                    continue;
                if (j != i)
                    m_entries[index][j].lru_stamp++;
            }
            return i;
        }

        // decide if this entry should be evicted
        if (m_entries[index][i].lru_stamp > evict_stamp)
        {
            evict_stamp = m_entries[index][i].lru_stamp;
            evict_idx = i;
        }
    }

    // // evict in L1: update the corresponding entry in L2
    // // evict in L2: write back to page table if D is set
    // if (m_type == l1_d)
    // {
    //     // Maybe we can omit updating for entries whose D is 0
    //     m_next_d->update_trans(raw_vaddr, 
    //                            m_entries[index][evict_idx].bitmap,
    //                            page_shift);
    // }
    // else
    // {
    //     if (m_entries[index][evict_idx].is_meta)
    //     {
    //         m_meta2tr++;
    //         m_evmeta_to_mem++;
    //     }
    //     else
    //     {
    //         m_tr2tr++;
    //         if (m_entries[index][evict_idx].bitmap & 0x1)
    //         {
    //             m_evtr_to_mem++;
    //         }
    //     }
    // }
    
    // no free entry, select one entry to evict
    m_entries[index][evict_idx].tag = tag;
    m_entries[index][evict_idx].lru_stamp = 0;
    m_entries[index][evict_idx].is_meta = false;
    for (int j = 0; j < m_associativity; j++)
    {
        if (!m_entries[index][j].valid)
            continue;

        if (j != evict_idx)
            m_entries[index][j].lru_stamp++;
    }
    return evict_idx;
}

// void DTLB::update_trans(uint64_t vaddr, uint64_t bitmap, uint32_t page_shift)
// {
//     assert(m_type == l2_d);
//     uint64_t index, tag;

//     uint64_t index_mask = m_set_num - 1; // make sure m_set_num is a power of 2
    
//     index = (vaddr >> (page_shift + m_index_mod)) & index_mask;
//     // hardware may concatenate remaining bits instead of using the whole vpn as tag
//     tag = vaddr >> page_shift;
//     // add one extra bit to indicate whether or not a meta entry
//     // useful to distinguish a meta entry from the translation of a 64K-aligned page
//     // for a translation entry, this bit is always 0
//     tag = tag << 1;

//     for (int i = 0; i < m_associativity; i++)
//     {
//         if (!m_entries[index][i].valid)
//             continue;
//         // printf("  entry tag: 0x%lx\n", m_entries[index][i].tag);
//         if (m_entries[index][i].tag == tag)
//         {
//             assert(!m_entries[index][i].is_meta);
            
//             // Should we update the lur_stamp for an updating request?
//             // a hit, update lru_stamps
//             // for (int j = 0; j < m_associativity; j++)
//             // {
//             //     if (!m_entries[index][j].valid)
//             //         continue;
//             //     if (j != i)
//             //         m_entries[index][j].lru_stamp++;
//             // }
//             m_entries[index][i].bitmap = bitmap;
//             return;
//         }
//     }
//     // Since our TLB is non-inclusive and non-exclusive (NINE), an L2 entry may be evicted
//     // while the corresponding L1 entry is still in. In that case, update_trans() cannot find
//     // the entry to update, the evicted L1 entry should be written back to the memory.
//     // From another perspective, we can view the L2 TLB as a filter to cancel unnecessary 
//     // memory updates.
//     if (bitmap & 0x1)
//     {
//         m_evtr_to_mem++;
//         m_ev_passl2++;
//     }
// }

// void DTLB::lookup_meta(uint64_t vaddr, uint8_t opcode)
// {
//     assert(m_type == l2_d);

//     uint64_t index, tag;
//     uint64_t raw_vaddr = vaddr & PTR_MASK;

//     int basic_shift = sim_cfg.page_shift + sim_cfg.cluster_factor;

//     raw_vaddr &= (~sim_cfg.meta_idx_mask << basic_shift);

//     uint64_t index_mask = m_set_num - 1; // make sure m_set_num is a power of 2
    
//     index = (raw_vaddr >> (basic_shift + m_index_mod)) & index_mask;
//     // hardware may concatenate remaining bits instead of using the whole vpn as tag
//     tag = raw_vaddr >> basic_shift;
//     // add one extra bit to indicate whether or not a meta entry
//     // useful to distinguish a meta entry from the translation of a 64K-aligned page
//     // for a meta entry, this bit is always 1
//     tag = (tag << 1) + 1;

//     bool hit = false;
//     int pos = -1;
//     for (int i = 0; i < m_associativity; i++)
//     {
//         if (!m_entries[index][i].valid)
//             continue;
//         if (m_entries[index][i].tag == tag)
//         {
//             assert(m_entries[index][i].is_meta);
//             // a hit, update lru_stamps
//             // Do meta entries need standalone lru_stamps?
//             for (int j = 0; j < m_associativity; j++)
//             {
//                 if (!m_entries[index][j].valid)
//                     continue;
//                 if (j != i)
//                     m_entries[index][j].lru_stamp++;
//             }
//             m_meta_hit++;
//             hit = true;
//             pos = i;
//         }
//     }
//     // a miss
//     if (!hit)
//     {
//         m_meta_miss++;
//         pos = allocate_meta(vaddr);
//     }
//     uint8_t meta_idx = (vaddr >> basic_shift) & sim_cfg.meta_idx_mask;
//     uint64_t bits_change = 0;
//     switch (opcode)
//     {
//     case 1: // write
//         bits_change = 0b11;
//         break;
//     case 0: // read
//         bits_change = 0b10;
//         break;
//     default:
//         fprintf(stderr, "lookup_meta: unknown opcode: %u\n", opcode);
//         exit(-1);
//     }
//     assert(pos >= 0);
//     m_entries[index][pos].bitmap |= (bits_change << (meta_idx << 1));
//     return;
// }

// int DTLB::allocate_meta(uint64_t vaddr)
// {
//     assert(m_type == l2_d);
//     uint64_t index, tag;
//     uint64_t raw_vaddr = vaddr & PTR_MASK;

//     int basic_shift = sim_cfg.page_shift + sim_cfg.cluster_factor;

//     raw_vaddr &= (~sim_cfg.meta_idx_mask << basic_shift); // clear the lowest 16 bits

//     uint64_t index_mask = m_set_num - 1; // make sure m_set_num is a power of 2


//     index = (raw_vaddr >> (basic_shift + m_index_mod)) & index_mask;
//     // hardware may concatenate remaining bits instead of using the whole vpn as tag
//     tag = raw_vaddr >> basic_shift;
//     // add one extra bit to indicate whether or not a meta entry
//     // useful to distinguish a meta entry from the translation of a 64K-aligned page
//     // for a meta entry, this bit is always 1
//     tag = (tag << 1) + 1;

//     int i = 0;
//     int evict_stamp_meta = 0, evict_idx_meta = -1;
//     int evict_stamp_tr = 0, evict_idx_tr = -1;
//     for (i = 0; i < m_associativity; i++)
//     {
//         // try to find a free entry
//         if (!m_entries[index][i].valid)
//         {
//             // found a free entry
//             m_entries[index][i].valid = true;
//             m_entries[index][i].tag = tag;
//             m_entries[index][i].is_meta = true;
//             m_entries[index][i].bitmap = 0;
//             // update lru_stamps
//             for (int j = 0; j < m_associativity; j++)
//             {
//                 if (!m_entries[index][j].valid)
//                     continue;
//                 if (j != i)
//                     m_entries[index][j].lru_stamp++;
//             }
//             return i;
//         }

//         // decide if this entry should be evicted
//         if (m_entries[index][i].is_meta)
//         {
//             if (m_entries[index][i].lru_stamp > evict_stamp_meta)
//             {
//                 evict_stamp_meta = m_entries[index][i].lru_stamp;
//                 evict_idx_meta = i;
//             }
//         }
//         else
//         {
//             if (m_entries[index][i].lru_stamp > evict_stamp_tr)
//             {
//                 evict_stamp_tr = m_entries[index][i].lru_stamp;
//                 evict_idx_tr = i;
//             }
//         }
//     }

//     // only choose translation entries when no meta entry present
//     if (evict_idx_meta >= 0)
//     {
//         m_meta2meta++;
//         m_evmeta_to_mem++;

//         m_entries[index][evict_idx_meta].tag = tag;
//         m_entries[index][evict_idx_meta].lru_stamp = 0;
//         m_entries[index][evict_idx_meta].is_meta = true;
//         for (int j = 0; j < m_associativity; j++)
//         {
//             if (!m_entries[index][j].valid)
//                 continue;
//             if (j != evict_idx_meta)
//                 m_entries[index][j].lru_stamp++;
//         }
//         return evict_idx_meta;
//     }
//     else
//     {
//         m_tr2meta++;
//         if (m_entries[index][evict_idx_tr].bitmap & 0x1)
//             m_evtr_to_mem++;

//         m_entries[index][evict_idx_tr].tag = tag;
//         m_entries[index][evict_idx_tr].lru_stamp = 0;
//         m_entries[index][evict_idx_tr].is_meta = true;
//         for (int j = 0; j < m_associativity; j++)
//         {
//             if (!m_entries[index][j].valid)
//                 continue;
//             if (j != evict_idx_tr)
//                 m_entries[index][j].lru_stamp++;
//         }
//         return evict_idx_tr;
//     }

//     // should never reach
//     // return -1;
// }

void DTLB::set_next_level(DTLB *next_ptr)
{
    assert(next_ptr != NULL);
    assert(m_type == l1_d);
    m_next_d = next_ptr;
}

void DTLB::print_meta_stat()
{
    printf("\nmeta entry hit: %lu\n", m_meta_hit);
    printf("meta entry miss: %lu\n", m_meta_miss);
    printf("\nevicts:\n");
    printf("  translation to meta: %lu\n", m_tr2meta);
    printf("  translation to translation: %lu\n", m_tr2tr);
    printf("  meta to translation: %lu\n", m_meta2tr);
    printf("  meta to meta: %lu\n\n", m_meta2meta);

    printf("  translation write back to mem: %lu\n", m_evtr_to_mem);
    printf("    pass through L2 TLB: %lu\n", m_ev_passl2);
    printf("  meta write back to mem: %lu\n", m_evmeta_to_mem);
    return;
}

// ----------------------------------------------------
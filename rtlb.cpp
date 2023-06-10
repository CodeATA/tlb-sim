#include "tlb.h"

RTLB::RTLB()
{
    m_range_table = NULL;
    m_entries = NULL;
    m_meta_d = NULL;

    m_enable_l2_rtlb = false;
    m_l2_entries = NULL;

    m_enable_meta_l2 = false;
}

RTLB::RTLB(int l1_entries, bool meta_l2)
{
    m_range_table = NULL;
    m_meta_d = NULL;
    m_enable_meta_l2 = meta_l2;

    // fully-associative cache
    m_set_num = 1;
    m_associativity = l1_entries;

    m_entries = new rtlb_entry*[m_set_num];
    for (int i = 0; i < m_set_num; i++)
    {
        m_entries[i] = new rtlb_entry[m_associativity];
        for (int j = 0; j < m_associativity; j++)
        {
            m_entries[i][j].valid = false;
        }
    }

    m_enable_l2_rtlb = false;
    m_l2_entries = NULL;

    return;
}

RTLB::RTLB(int l1_entries, int l2_set_num, int l2_associativity, bool meta_l2)
{
    m_range_table = NULL;
    m_meta_d = NULL;
    m_enable_meta_l2 = meta_l2;

    // fully-associative cache
    m_set_num = 1;
    m_associativity = l1_entries;

    m_entries = new rtlb_entry*[m_set_num];
    for (int i = 0; i < m_set_num; i++)
    {
        m_entries[i] = new rtlb_entry[m_associativity];
        for (int j = 0; j < m_associativity; j++)
        {
            m_entries[i][j].valid = false;
        }
    }

    m_enable_l2_rtlb = true;
    assert(isPower2(l2_set_num));
    m_l2_set_num = l2_set_num;
    m_l2_associativity = l2_associativity;
    m_l2_entries = new rtlb_entry*[m_l2_set_num];
    for (int i = 0; i < m_l2_set_num; i++)
    {
        m_l2_entries[i] = new rtlb_entry[m_l2_associativity];
        for (int j = 0; j < m_l2_associativity; j++)
        {
            m_l2_entries[i][j].valid = false;
        }
    }

    return;
}

RTLB::~RTLB()
{
    for (int i = 0; i < m_set_num; i++)
    {
        delete m_entries[i];
    }
    delete m_entries;
    
    if (m_enable_l2_rtlb)
    {
        assert(m_l2_entries != NULL);
        for (int i = 0; i < m_l2_set_num; i++)
        {
            delete m_l2_entries[i];
        }
        delete m_l2_entries;
    }
}

void RTLB::setRangeTable (RangeTable* range_table)
{
    assert(range_table != NULL);
    m_range_table = range_table;
}

void RTLB::setMetaL2 (DTLB* meta_l2)
{
    assert(m_enable_meta_l2);
    assert(meta_l2 != NULL);
    m_meta_d = meta_l2;
}

tlb_hit_where RTLB::lookup (uint32_t id, uint64_t vaddr, uint8_t opcode)
{
    uint64_t raw_vaddr = vaddr & PTR_MASK;

    tlb_hit_where ret = r_miss;
    rtlb_entry* where_to_put = NULL;
    int access_pos = -1;

    for (int i = 0; i < m_associativity; i++)
    {
        if (m_entries[0][i].valid &&
            m_entries[0][i].main_tag == id)
        {
            if (raw_vaddr >= m_entries[0][i].seg_start &&
                raw_vaddr <  m_entries[0][i].seg_end)
            {
                ret = l1_r_hit;
                access_pos = i;
                break;
            }
        }
    }

    // send the addr to L2 TLB for metadata update
    // if (m_enable_meta_l2)
    //     m_meta_d->lookup_meta(vaddr, opcode);

    // a hit, update lru_stamps and return
    if (ret == l1_r_hit)
    {
        assert(access_pos >= 0);
        m_entries[0][access_pos].lru_stamp = 0;
        for (int i = 0; i < m_associativity; i++)
        {
            if (!m_entries[0][i].valid)
                continue;
            if (i != access_pos)
                m_entries[0][i].lru_stamp++;
        }
        return ret;
    }

    // a miss, find a new slot
    assert(ret == r_miss);
    where_to_put = allocateEntry();
    if (m_enable_l2_rtlb)
    {
        // if L2 RTLB enabled, search the range in L2 RTLB
        if (lookupL2(id, vaddr, where_to_put))
            ret = l2_r_hit;
        // lookupL2() will handle the miss in L2 RTLB
        else
            ret = r_miss;
    }
    else
    {
        // no L2 RTLB, fetch from the range table
        // fprintf(stderr, "RTLB fetch %u, 0x%lx, %u\n", id, vaddr, opcode);
        m_range_table->fetch(id, vaddr, where_to_put);
    }
    return ret;
}

bool RTLB::lookupL2(uint32_t id, uint64_t vaddr, rtlb_entry* where_to_put)
{
    uint64_t raw_vaddr = vaddr & PTR_MASK;

    uint64_t l2_set_idx_mask = m_l2_set_num - 1;
    int l2_set_idx = id & l2_set_idx_mask;

    int access_pos = -1;
    bool ret = false;
    for (int i = 0; i < m_l2_associativity; i++)
    {
        if (m_l2_entries[l2_set_idx][i].valid &&
            m_l2_entries[l2_set_idx][i].main_tag == id)
        {
            if (raw_vaddr >= m_l2_entries[l2_set_idx][i].seg_start &&
                raw_vaddr <  m_l2_entries[l2_set_idx][i].seg_end)
            {
                ret = true;
                access_pos = i;
                break;
            }
        }
    }

    if (ret)
    {
        // a hit in L2 RTLB
        // update lru_stamps in the same set (*only one set*)
        assert(access_pos >= 0);
        m_l2_entries[l2_set_idx][access_pos].lru_stamp = 0;
        for (int i = 0; i < m_l2_associativity; i++)
        {
            if (!m_l2_entries[l2_set_idx][i].valid)
                continue;
            if (i != access_pos)
                m_l2_entries[l2_set_idx][i].lru_stamp++;
        }
        // update the L1 RTLB entry
        where_to_put->valid = true;
        where_to_put->seg_start = m_l2_entries[l2_set_idx][access_pos].seg_start;
        where_to_put->seg_end = m_l2_entries[l2_set_idx][access_pos].seg_end;
        where_to_put->main_tag = m_l2_entries[l2_set_idx][access_pos].main_tag;
    }
    else
    {
        // a miss in L2 RTLB
        // find a slot in current set and fetch from the range table
        rtlb_entry *l2_where_to_put = NULL;
        l2_where_to_put = allocateEntryL2(l2_set_idx);
        m_range_table->fetch(id, vaddr, l2_where_to_put);
        // update the L1 RTLB entry
        where_to_put->valid = true;
        where_to_put->seg_start = l2_where_to_put->seg_start;
        where_to_put->seg_end = l2_where_to_put->seg_end;
        where_to_put->main_tag = l2_where_to_put->main_tag;
    }
    return ret;
}


// decides where to put the new entry
// the entry pointer will be passed to fetch()
// fetch() is responsible for filling the fields 
rtlb_entry* RTLB::allocateEntry()
{
    uint32_t sel_idx = 0, sel_stamp = 0;
    for (int i = 0; i < m_associativity; i++)
    {
        if (!m_entries[0][i].valid)
        {
            sel_idx = i;
            break;
        }
        if (m_entries[0][i].lru_stamp > sel_stamp)
        {
            sel_idx = i;
            sel_stamp = m_entries[0][i].lru_stamp;
        }
    }

    // update lru_stamps
    m_entries[0][sel_idx].lru_stamp = 0;
    for (int k = 0; k < m_associativity; k++)
    {
        if (!m_entries[0][k].valid)
            continue;
        if (k != sel_idx)
            m_entries[0][k].lru_stamp++;
    }
    return &m_entries[0][sel_idx];
}

// decide where to put the new entry in a specific L2 RTLB set
// RangeTable::fetch() will later fill the entry
rtlb_entry* RTLB::allocateEntryL2(int set_idx)
{
    int sel_idx = 0, sel_stamp = 0;
    for (int i = 0; i < m_l2_associativity; i++)
    {
        if (!m_l2_entries[set_idx][i].valid)
        {
            // a free slot
            sel_idx = i;
            break;
        }
        if (m_l2_entries[set_idx][i].lru_stamp > sel_stamp)
        {
            sel_idx = i;
            sel_stamp = m_l2_entries[set_idx][i].lru_stamp;
        }
    }

    m_l2_entries[set_idx][sel_idx].lru_stamp = 0;
    for (int i = 0; i < m_l2_associativity; i++)
    {
        if (!m_l2_entries[set_idx][i].valid)
            continue;
        if (i != sel_idx)
            m_l2_entries[set_idx][i].lru_stamp++;
    }
    return &m_l2_entries[set_idx][sel_idx];
}

// This function is used for unmap, and remap generating child segments
void RTLB::flushEntry(uint32_t id)
{
    // method: find all entries with the corresponding range ID, invalidate them all
    for (int i = 0; i < m_associativity; i++)
    {
        if (m_entries[0][i].valid &&
            m_entries[0][i].main_tag == id)
        {
            m_entries[0][i].valid = false;
        }
    }

    if (m_enable_l2_rtlb)
    {
        for (int i = 0; i < m_l2_set_num; i++)
        {
            for (int j = 0; j < m_l2_associativity; j++)
            {
                if (m_l2_entries[i][j].valid &&
                    m_l2_entries[i][j].main_tag == id)
                {
                    m_l2_entries[i][j].valid = false;
                }
            }
        }
    }

    return;
}
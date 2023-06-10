#include "range_table.h"

// const uint32_t RangeTable::TAG_SHIFT = 48;
// const uint64_t RangeTable::TAG_MASK = 0xFFFF;
// const uint64_t RangeTable::PTR_MASK = 0xFFFFFFFFFFFF;

using namespace std;

RangeTable::RangeTable()
{
    max_count = cur_count = 0;
    max_search_len = -1;
    search_count = 0;
    search_len_bucket = NULL;
}

RangeTable::RangeTable(char *mapping_file, char *trace_dir)
{
    max_count = cur_count = 0;
    max_search_len = -1;
    search_count = 0;
    search_len_bucket = NULL;
    
    // strcpy(_mapping_dir, mapping_dir);

    char buffer[400];
    sprintf(buffer, "%s/access_info", trace_dir);
    FILE *a_file = fopen(buffer, "r");
    if (a_file == NULL)
    {
        fprintf(stderr, "Cannot open access range info file: %s\n", buffer);
        exit(-1);
    }
    int count;
    fgets(buffer, 400, a_file);
    sscanf(buffer, "%d", &count);
    for (int i = 0; i < count; i++)
    {
        if (fgets(buffer, 400, a_file) == NULL)
        {
            fprintf(stderr, "Error reading range info, no info for ID %d\n", i + 1);
            exit(-1);
        }
        mapping_info t_info;
        sscanf(buffer, "%u: %ld %ld", &t_info.id, &t_info.access_low, &t_info.access_high);
        t_info.access_high++; // the accessed area is [access_low, access_high)
        
        m_mappings.push_back(t_info);
    }
    fclose(a_file);

    // sprintf(buffer, "%s/mapping.info", mapping_dir);
    FILE *m_file = fopen(mapping_file, "r");
    if (m_file == NULL)
    {
        fprintf(stderr, "Cannot open access range info file: %s\n", mapping_file);
        exit(-1);
    }
    bool exceed = false;
    while (fgets(buffer, 400, m_file) != NULL)
    {
        uint32_t id;
        uint64_t size_1, size_2;
        int sub_count;
        mapping_seg t_seg;
        switch(buffer[0])
        {
        case 'a':
            sscanf(buffer, "a %u %lu %d", &id, &size_1, &sub_count);
            if (id > m_mappings.size())
            {
                exceed = true;
                break;
            }
            
            t_seg.num_pages = size_1 >> 12;
            t_seg.sub_count = sub_count;
            assert(m_mappings[id - 1].id == id);
            m_mappings[id - 1].subs.push_back(t_seg);
            break;
        case 'r':
            sscanf(buffer, "r %u %lu %lu %d", &id, &size_1, &size_2, &sub_count);
            t_seg.num_pages = (size_2 - size_1) >> 12;
            t_seg.sub_count = sub_count;
            assert(m_mappings[id - 1].id == id);
            m_mappings[id - 1].subs.push_back(t_seg);
            break;
        default:
            break;
        }
        if (exceed)
            break;
    }
    fclose(m_file);
}

RangeTable::~RangeTable()
{
    for (int i = 0; i < m_entries.size(); i++)
    {
        m_entries[i].subrange.clear();
    }
    m_entries.clear();
    for (int i = 0; i < m_mappings.size(); i++)
    {
        m_mappings[i].subs.clear();
    }
    m_mappings.clear();
}


void RangeTable::_insertMappings(range_entry &r_info, int p_low, int p_high, int a_low, int a_high, int seg_count, bool activate)
{
    // [a_low, a_high) [p_low, p_high)
    int overlap_low, overlap_high;
    if (p_high <= a_low || p_low >= a_high)
    {
        // no overlapping part, insert according to [p_low, p_high)
        overlap_low = p_low;
        overlap_high = p_high;
    }
    else
    {
        // choose the bigger one as the low end
        if (p_low < a_low)
            overlap_low = a_low;
        else
            overlap_low = p_low;
        // choose the smaller one as the high end
        if (a_high < p_high)
            overlap_high = a_high;
        else
            overlap_high = p_high;
    }

    assert(overlap_high > overlap_low);
    int step = (overlap_high - overlap_low) / seg_count;
    if (step == 0)
        step = 1;
    vector<int> seg_points;
    int point = overlap_low;
    for (int i = 0; i < seg_count - 1; i++)
    {
        point += step;
        if (point < overlap_high)
            seg_points.push_back(point);
        else
            break;
    }
    uint64_t cur_start = r_info.r_start + ((uint64_t)p_low << 12);
    subrange_entry t_entry;
    for (int i = 0; i < seg_points.size(); i++)
    {
        t_entry.active = activate;
        t_entry.s_start = cur_start;
        t_entry.s_end = r_info.r_start + ((uint64_t)seg_points[i] << 12);
        r_info.subrange.push_back(t_entry);
        cur_start = t_entry.s_end;
    }
    t_entry.active = activate;
    t_entry.s_start = cur_start;
    t_entry.s_end = r_info.r_start + ((uint64_t)p_high << 12);
    r_info.subrange.push_back(t_entry);

    // fprintf(stderr, "insert mappings for %u: [0x%lx - 0x%lx]\n", r_info.range_id, r_info.r_start, r_info.r_end);
    // fprintf(stderr, "  access range: [%d, %d)\n", a_low, a_high);
    // fprintf(stderr, "  mapping range: [%d, %d)\n", p_low, p_high);
    // for (int i = 0; i < r_info.subrange.size(); i++)
    // {
    //     if (r_info.subrange[i].active)
    //         fprintf(stderr, "  [0x%lx - 0x%lx], on\n", r_info.subrange[i].s_start, r_info.subrange[i].s_end);
    //     else
    //         fprintf(stderr, "  [0x%lx - 0x%lx], off\n", r_info.subrange[i].s_start, r_info.subrange[i].s_end);
    // }
}


void RangeTable::_readMappings(range_entry &r_info)
{
    mapping_info m_info = m_mappings[r_info.range_id - 1];
    assert(m_info.id == r_info.range_id);
    subrange_entry sub_info;
    if (m_info.access_low < 0)
    {
        // No insruction in the trace accessed this range, no need to record it
        sub_info.active = true;
        sub_info.s_start = r_info.r_start;
        sub_info.s_end = r_info.r_end;
        r_info.subrange.push_back(sub_info);
        return;
    }

    int cur_offset = 0;
    int next_offset;
    bool enable;
    for (int i = 0; i < m_info.subs.size(); i++)
    {
        next_offset = cur_offset + m_info.subs[i].num_pages;
        if (i == 0)
            enable = true;
        else
            enable = false;
        _insertMappings(r_info, cur_offset, next_offset, m_info.access_low, m_info.access_high, m_info.subs[i].sub_count, enable);
        cur_offset = next_offset;
    }

    return;
}

void RangeTable::_activateMappings(range_entry &r_info)
{
    uint64_t cur_end = r_info.r_start;

    for (int i = 0; i < r_info.subrange.size(); i++)
    {
        uint64_t sub_size = r_info.subrange[i].s_end - r_info.subrange[i].s_start;
        r_info.subrange[i].s_start = cur_end;
        r_info.subrange[i].s_end = cur_end + sub_size;
        cur_end = r_info.subrange[i].s_end;

        if (r_info.subrange[i].s_end <= r_info.r_end)
            r_info.subrange[i].active = true;
    }
}

// The ID recorded in the trace is directly used as the tag for a range
void RangeTable::insertEntry(uint32_t id, uint64_t va_start, uint64_t r_size)
{
    uint64_t raw_addr = va_start & PTR_MASK;

    range_entry r_info;
    r_info.range_id = id;
    r_info.r_start = raw_addr;
    r_info.r_end = raw_addr + r_size;
    r_info.r_size = r_size;

    m_entries.push_back(r_info);
    cur_count++;
    if (cur_count > max_count)
        max_count = cur_count;

    if (sim_cfg.enable_mapping)
        _readMappings(*m_entries.rbegin());
    else
    {
        subrange_entry s_info;
        s_info.active = true;
        s_info.s_start = raw_addr;
        s_info.s_end = raw_addr + r_size;
        m_entries.rbegin()->subrange.push_back(s_info);
    }
}

void RangeTable::freeEntry(uint32_t id, uint64_t va_start, uint64_t r_size)
{
    int pos = -1;
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].range_id == id)
        {
            pos = i;
            break;
        }
    }
    if (pos < 0)
    {
        fprintf(stderr, "freeEntry: Cannot find entry for %u\n", id);
        for (int i = 0; i < m_entries.size(); i++)
        {
            fprintf(stderr, "  %u: [0x%lx - 0x%lx]\n", m_entries[i].range_id, m_entries[i].r_start, m_entries[i].r_end);
        }
        exit(-1);
    }

    uint64_t raw_addr = va_start & PTR_MASK;
    assert(m_entries[pos].r_start == raw_addr);
    assert(m_entries[pos].r_size == r_size);

    m_entries[pos].subrange.clear();
    m_entries.erase(m_entries.begin() + pos);

    cur_count--;
}

void RangeTable::remapEntry(uint32_t id, uint64_t old_va_start, uint64_t new_va_start, uint64_t old_r_size, uint64_t new_r_size)
{
    int pos = -1;
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].range_id == id)
        {
            pos = i;
            break;
        }
    }
    if (pos < 0)
    {
        fprintf(stderr, "remapEntry: Cannot find entry for %u\n", id);
        for (int i = 0; i < m_entries.size(); i++)
        {
            fprintf(stderr, "  %u: [0x%lx - 0x%lx]\n", m_entries[i].range_id, m_entries[i].r_start, m_entries[i].r_end);
        }
        exit(-1);
    }

    uint64_t raw_old = old_va_start & PTR_MASK;
    uint64_t raw_new = new_va_start & PTR_MASK;

    assert(m_entries[pos].r_start == raw_old);
    assert(m_entries[pos].r_size == old_r_size);

    m_entries[pos].r_start = raw_new;
    m_entries[pos].r_size = new_r_size;
    m_entries[pos].r_end = raw_new + new_r_size;

    if (sim_cfg.enable_mapping)
        _activateMappings(m_entries[pos]);
    else
    {
        m_entries[pos].subrange[0].s_start = raw_new;
        m_entries[pos].subrange[0].s_end = raw_new + new_r_size;
    }
}

void RangeTable::fetch(uint32_t id, uint64_t vaddr, rtlb_entry *res)
{
    int pos = -1;
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (m_entries[i].range_id == id)
        {
            pos = i;
            break;
        }
    }
    if (pos < 0)
    {
        fprintf(stderr, "range fetch: Cannot find entry for %u\n", id);
        for (int i = 0; i < m_entries.size(); i++)
        {
            fprintf(stderr, "  %u: [0x%lx - 0x%lx]\n", m_entries[i].range_id, m_entries[i].r_start, m_entries[i].r_end);
        }
        exit(-1);
    }

    uint64_t raw_addr = vaddr & PTR_MASK;
    assert(raw_addr >= m_entries[pos].r_start);
    assert(raw_addr <  m_entries[pos].r_end);

    int search_len = -1;
    for (int i = 0; i < m_entries[pos].subrange.size(); i++)
    {
        if (raw_addr >= m_entries[pos].subrange[i].s_start &&
            raw_addr <  m_entries[pos].subrange[i].s_end)
        {
            res->valid = true;
            res->main_tag = id;
            res->seg_start = m_entries[pos].subrange[i].s_start;
            res->seg_end = m_entries[pos].subrange[i].s_end;

            // fprintf(stderr, "    fetch from (%u, %d)\n", id, i);

            search_len = i + 1;
            break;
        }
    }
    
    if (search_len <= 0)
    {
        fprintf(stderr, "[Fetch] serarch len problem (search_len=%d)\n", search_len);
        fprintf(stderr, "  vaddr=0x%lx, tag=%u\n  subranges:\n", vaddr, id);
        fprintf(stderr, "  raw addr=0x%lx\n", raw_addr);
        for (int i = 0; i < m_entries[pos].subrange.size(); i++)
        {
            fprintf(stderr, "    [0x%lx - 0x%lx]\n", m_entries[pos].subrange[i].s_start, m_entries[pos].subrange[i].s_end);
        }
        exit(-1);
    }

    search_count++;

    if (sim_cfg.enable_mapping)
    {
        if (search_len > max_search_len)
        {
            search_len_bucket = (int*)realloc(search_len_bucket, (search_len+1)*sizeof(int));
            for (int i = max_search_len+1; i < search_len+1; i++)
                search_len_bucket[i] = 0;
            max_search_len = search_len;
        }
        search_len_bucket[search_len]++;
        // fprintf(stderr, "search len %d\n", search_len);
    }
}

void RangeTable::printStat()
{
    printf("\nmax active range: %d\n", max_count);
    
    if (sim_cfg.enable_mapping)
    {
        printf("range table search times: %d\n", search_count);
        printf("max search length: %d\n", max_search_len);
        
        int64_t len_sum = 0;
        int64_t count_sum = 0;
        double len_median = -1;
        double len_mean;

        for (int i = 1; i < max_search_len+1; i++)
        {
            len_sum += i * search_len_bucket[i];
            count_sum += search_len_bucket[i];
            if (len_median < 0 && count_sum >= search_count/2)
            {
                // fprintf(stderr, "  count: %d\n", count_sum);
                if (count_sum >= search_count/2+1)
                    len_median = i;
                // count_sum == search_count/2
                else if ((search_count & 0x1) == 0)
                {
                    for (int j = i+1; j < max_search_len+1; j++)
                    {
                        if (search_len_bucket[j] > 0)
                        {
                            len_median = ((double)i + (double)j) / 2;
                            break;
                        }
                    }
                }
                else
                {
                    // fprintf(stderr, "here? %d\n", (search_count & 0x1));
                    len_median = i;
                }
            }
        }
        len_mean = (double)len_sum / (double)search_count;
        
        printf("search length median: %lf\n", len_median);
        printf("search length mean: %lf\n", len_mean);
    }
}
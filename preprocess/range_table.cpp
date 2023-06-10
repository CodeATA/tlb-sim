#include "range_table.h"

bool r_entry_less (const range_entry &a, const range_entry &b)
{
    return a.r_start < b.r_start;
}

RangeTable::RangeTable()
{}

RangeTable::~RangeTable()
{}

// during preprocess, all the vaddr have no tag
void RangeTable::insertEntry(uint32_t id, uint64_t va_start, uint64_t r_size)
{
    // if (id > TAG_MASK)
    // {
    //     printf("(%u) mmap 0x%lx %lu\n", id, va_start, r_size);
    //     printf("  ID exceeds unused bits: %u > 0x%lu\n", id, TAG_MASK);
    //     exit(-1);
    // }
    // fprintf(stderr, "    range insert %u, 0x%lx, %lu\n", id, va_start, r_size);

    assert(id > 0);

    range_entry tmp;
    tmp.id = id;
    tmp.r_start = va_start;
    tmp.r_size = r_size;
    tmp.r_end = va_start + r_size;
    m_entries.push_back(tmp);

    range_access a_tmp;
    a_tmp.id = id;
    a_tmp.page_low = -1;
    a_tmp.page_high = -1;
    access_info.push_back(a_tmp);

    // 需要保证用ID可以直接取出实际访问上界下界信息，不要再做一次搜索
    assert(access_info[id - 1].id == id);
    sort(m_entries.begin(), m_entries.end(), r_entry_less);
}

void RangeTable::freeEntry(uint32_t id, uint64_t va_start, uint64_t r_size)
{
    // fprintf(stderr, "    range free %u\n", id);

    int entry_pos = -1;
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (id == m_entries[i].id)
        {
            entry_pos = i;
            break;
        }
    }

    assert(entry_pos >= 0);
    assert(va_start == m_entries[entry_pos].r_start);
    assert(r_size == m_entries[entry_pos].r_size);

    m_entries.erase(m_entries.begin() + entry_pos);
}

void RangeTable::remapEntry(uint32_t id, uint64_t old_va_start, uint64_t old_r_size, uint64_t new_va_start, uint64_t new_r_size)
{
    // fprintf(stderr, "    range remap %u\n", id);

    int entry_pos = -1;
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (id == m_entries[i].id)
        {
            entry_pos = i;
            break;
        }
    }

    assert(entry_pos >= 0);
    assert(old_va_start == m_entries[entry_pos].r_start);
    assert(old_r_size == m_entries[entry_pos].r_size);

    m_entries[entry_pos].r_start = new_va_start;
    m_entries[entry_pos].r_size = new_r_size;
    m_entries[entry_pos].r_end = new_va_start + new_r_size;
    sort(m_entries.begin(), m_entries.end(), r_entry_less);
}

int RangeTable::lookupAddr(uint64_t vaddr)
{
    if (m_entries.size() == 0)
        return 0;

    int left, right;
    left = 0;
    right = m_entries.size() - 1;
    while (left < right)
    {
        int pos = (left + right) / 2;
        if (vaddr < m_entries[pos].r_start)
        {
            right = pos - 1;
        }
        else if (vaddr < m_entries[pos].r_end)
        {
            left = pos;
            break;
        }
        else
        {
            left = pos + 1;
        }
    }
    int idx = left;
    if (vaddr >= m_entries[idx].r_start && vaddr < m_entries[idx].r_end)
    {
        int64_t offset = (vaddr - m_entries[idx].r_start) >> 12;
        if (access_info[m_entries[idx].id - 1].page_low < 0)
        {
            access_info[m_entries[idx].id - 1].page_low = offset;
            access_info[m_entries[idx].id - 1].page_high = offset;
        }
        else
        {
            if (offset < access_info[m_entries[idx].id - 1].page_low)
                access_info[m_entries[idx].id - 1].page_low = offset;
            if (offset > access_info[m_entries[idx].id - 1].page_high)
                access_info[m_entries[idx].id - 1].page_high = offset;
        }
        return m_entries[idx].id;
    }
    else
        return 0;
}

bool RangeTable::lookupID(uint32_t id)
{
    for (int i = 0; i < m_entries.size(); i++)
    {
        if (id == m_entries[i].id)
        {
            return true;
        }
    }
    return false;
}

void RangeTable::printStat()
{
    for (int i = 0; i < m_entries.size(); i++)
    {
        printf("Seg %d: [0x%lx - 0x%lx]\n", i, m_entries[i].r_start, m_entries[i].r_end);
    }
}

void RangeTable::printAccessStat(const char *a_path)
{
    char tmp[300];
    sprintf(tmp, "%s/access_info", a_path);
    FILE *a_file = fopen(tmp, "w");
    if (a_file == NULL)
    {
        fprintf(stderr, "Fail to open: %s\n", tmp);
        exit(-1);
    }
    fprintf(a_file, "%lu\n", access_info.size());
    for (int i = 0; i < access_info.size(); i++)
    {
        fprintf(a_file, "%u: %ld %ld\n", access_info[i].id, access_info[i].page_low, access_info[i].page_high);
    }
    fclose(a_file);
}
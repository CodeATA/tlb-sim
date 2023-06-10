#include "utils.h"

uint64_t THRESHOLD;

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
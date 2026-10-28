/* Paging mmu. Owner: P5. */
#include "mmu.h"

uint32_t ms_page_offset_bits(uint32_t page_size) {
    if (page_size == 0 || (page_size & (page_size - 1)) != 0) {
        return MS_NONE;
    }
    uint32_t k = 0;
    while ((UINT32_C(1) << k) != page_size) {
        k++;
    }
    return k;
}

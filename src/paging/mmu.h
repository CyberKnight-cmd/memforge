/* Paging mmu (private). Owner: P5. */
#ifndef MEMSIM_PAGING_MMU_H_
#define MEMSIM_PAGING_MMU_H_

#include "memsim/common/types.h"

/* k = log2(page_size), or MS_NONE when page_size is not a nonzero power of 2. */
uint32_t ms_page_offset_bits(uint32_t page_size);

/* vpn = va / page_size, offset = va mod page_size, pa = frame * page_size + offset. */
static inline uint64_t ms_page_vpn_of(uint64_t va, uint32_t k) {
    return va >> k;
}
static inline uint64_t ms_page_offset_of(uint64_t va, uint32_t k) {
    return va & ((UINT64_C(1) << k) - 1);
}
static inline uint64_t ms_page_pa(uint32_t frame, uint64_t offset, uint32_t k) {
    return ((uint64_t)frame << k) | offset;
}

#endif /* MEMSIM_PAGING_MMU_H_ */

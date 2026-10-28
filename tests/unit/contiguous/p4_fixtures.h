/* Layouts shared by the P4 unit tests. Owner: P4. */
#ifndef MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_FIXTURES_H_
#define MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_FIXTURES_H_

#include "memsim/common/types.h"

enum { PID_SYS = 0, PID_P1 = 1, PID_P2 = 2, PID_P3 = 3 };
enum { ID_S1 = 10, ID_S2, ID_S3, ID_S4, ID_A1 = 21, ID_A2, ID_A3 };

#define FREE_BLOCK(st, sz) {(st), (sz), MS_BLOCK_FREE, MS_NONE, MS_NONE, 0}
#define USED_BLOCK(st, sz, pid, id, req) {(st), (sz), MS_BLOCK_ALLOCATED, (pid), (id), (req)}

/* AC-1 First-Fit, just before the 426 request fails (README section 9). Memory is 1740 units:
 * Silberschatz's holes 100, 500, 200, 300, 600 between four 10-unit SYS blocks, then
 * 212 -> start 110, 417 -> start 1140, 112 -> start 322. */
static const ms_block AC1_FIRST_FIT[] = {
    FREE_BLOCK(0, 100),
    USED_BLOCK(100, 10, PID_SYS, ID_S1, 10),
    USED_BLOCK(110, 212, PID_P1, ID_A1, 212),
    USED_BLOCK(322, 112, PID_P3, ID_A3, 112),
    FREE_BLOCK(434, 176),
    USED_BLOCK(610, 10, PID_SYS, ID_S2, 10),
    FREE_BLOCK(620, 200),
    USED_BLOCK(820, 10, PID_SYS, ID_S3, 10),
    FREE_BLOCK(830, 300),
    USED_BLOCK(1130, 10, PID_SYS, ID_S4, 10),
    USED_BLOCK(1140, 417, PID_P2, ID_A2, 417),
    FREE_BLOCK(1557, 183),
};
#define AC1_COUNT (sizeof AC1_FIRST_FIT / sizeof AC1_FIRST_FIT[0])

#endif /* MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_FIXTURES_H_ */

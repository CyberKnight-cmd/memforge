#include "memsim/common/types.h"
#include "test.h"

static void test_version_is_0_x(void) {
    CHECK_EQ_INT(MEMSIM_VERSION_MAJOR, 0);
}

int main(void) {
    RUN_TEST(test_version_is_0_x);
    TEST_EXIT();
}

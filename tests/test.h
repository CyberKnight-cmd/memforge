/* Minimal test harness: no dependencies, one executable per test file. Owner: P8. */
#ifndef MEMSIM_TEST_H_
#define MEMSIM_TEST_H_

#include <stdio.h>

static int test_failures_ = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            test_failures_++;                                                                      \
        }                                                                                          \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                                         \
    do {                                                                                           \
        long long a_ = (long long)(a), b_ = (long long)(b);                                        \
        if (a_ != b_) {                                                                            \
            fprintf(stderr, "%s:%d: CHECK_EQ_INT failed: %s == %s (%lld vs %lld)\n", __FILE__,     \
                    __LINE__, #a, #b, a_, b_);                                                     \
            test_failures_++;                                                                      \
        }                                                                                          \
    } while (0)

#define RUN_TEST(fn)                                                                               \
    do {                                                                                           \
        int before_ = test_failures_;                                                              \
        fn();                                                                                      \
        fprintf(stderr, "%s %s\n", test_failures_ == before_ ? "PASS" : "FAIL", #fn);              \
    } while (0)

#define TEST_EXIT() return test_failures_ == 0 ? 0 : 1

#endif /* MEMSIM_TEST_H_ */

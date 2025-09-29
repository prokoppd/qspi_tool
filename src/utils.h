#ifndef UTILS_H
#define UTILS_H

#ifdef TESTING
#define STATIC
#else
#define STATIC static
#endif

#define lengthof(X) (sizeof(X) / sizeof((X)[0]))

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#endif

#define LUT_IDX(SEQ, NUM) (4*(SEQ) + (NUM))

// clang-format off
#define LUT_NULL(SEQ) \
    [LUT_IDX(SEQ, 0)] = 0, \
    [LUT_IDX(SEQ, 1)] = 0, \
    [LUT_IDX(SEQ, 2)] = 0, \
    [LUT_IDX(SEQ, 3)] = 0
// clang-format on

#endif // UTILS_H

#ifndef __UNIT_TEST_H__
#define __UNIT_TEST_H__

#include <stdio.h>

typedef struct {
  int t_passes, t_fails;
  int a_passes, a_fails;
} test_results_t;

#define TEST_PRINT(format, ...) \
  do { printf(format, ##__VA_ARGS__); fflush(stdout); } while(0)


#define _TEST(c, s)                                          \
  if(!(c)) {                                                 \
    if(!__a_fails) TEST_PRINT("failed\n");                   \
    __a_fails++; TEST_PRINT("  '%s' assertion failed\n", s); \
  } else { __a_passes++; }

#define TEST_ASSERT(c) _TEST((c), #c)


#define TEST_MAIN_BEGIN()                    \
  int __a_passes = 0, __a_fails = 0;         \
  int main(void) {                           \
    const char *__t_name = "";               \
    const char *__s_name = NULL;             \
    test_results_t _r = {0,0,0,0}, *r = &_r

#define TEST_MAIN_END()                                              \
  TEST_PRINT("----------------------------\n");                      \
  TEST_PRINT("test results:\n");                                     \
  TEST_PRINT("tests run:          %8d\n", r->t_passes + r->t_fails); \
  TEST_PRINT("test passes:        %8d\n", r->t_passes);              \
  TEST_PRINT("test failures:      %8d\n", r->t_fails);               \
  TEST_PRINT("assertion run:      %8d\n", r->a_passes + r->a_fails); \
  TEST_PRINT("assertion passes:   %8d\n", r->a_passes);              \
  TEST_PRINT("assertion failures: %8d\n", r->a_fails);               \
  return 0;                                                          \
}

#define TEST_SUITE_DEFINE(name) \
  extern void _test_suite_##name(test_results_t *r);

#define TEST_SUITE_BEGIN(name) void _test_suite_##name(test_results_t *r) { \
    int __a_passes = 0, __a_fails = 0;                                      \
    const char *__s_name = #name;                                           \
    const char *__t_name = "";     

#define TEST_SUITE_END()     \
  r->a_passes += __a_passes; \
  r->a_fails += __a_fails; }

#define TEST_SUITE_RUN(name) _test_suite_##name(r);

#define TEST_BEGIN(name) do {                                             \
  r->a_passes += __a_passes;                                              \
  r->a_fails += __a_fails;                                                \
  __a_passes = 0, __a_fails = 0;                                          \
  __t_name = #name;                                                       \
  if(__s_name) TEST_PRINT("running test \"%s.%s\"... ", __s_name, #name); \
  else TEST_PRINT("running test \"%s\"... ", #name)

#define TEST_END()                                 \
  if(!__a_fails) TEST_PRINT("passed\n");           \
  if(__a_fails) r->t_fails++; else r->t_passes++;  \
  r->a_passes += __a_passes;                       \
  r->a_fails += __a_fails;                         \
  __a_passes = 0, __a_fails = 0; } while(0)


#endif

/* g++ -g -Wno-format -DUNIT_TESTS -o retval_cache_unit_tests retval_cache_unit_tests.cpp  drltrace_retval_cache.cpp */

#pragma GCC diagnostic ignored "-Wformat"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "drltrace_retval_cache.h"


#define ASSERT(x) { if (! (x)) { printf("Assert failed: " #x "\n"); return -1; } }


void begin_test(unsigned int test_num) {
  printf("Test #%u:\n\n", test_num);
}

void finish_test() {
  printf("\nSuccess.\n\n\n");
}

int main(int ac, char **av) {
  retval_cache_init(stdout, 0, true);

  /* Test 1: add single entry to cache, then match its return value. */
  begin_test(1);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_set_return_value("x1", 2, (void *)666);
  ASSERT(is_cache_empty());
  finish_test();

  /* Test 2: add three entires, then match their return values. */
  begin_test(2);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  ASSERT(!is_cache_empty());
  ASSERT(get_cache_size() == 3);
  retval_cache_set_return_value("x3", 2, (void *)3);
  retval_cache_set_return_value("x2", 2, (void *)2);
  retval_cache_set_return_value("x1", 2, (void *)1);
  ASSERT(is_cache_empty());
  finish_test();

  /* Test 3: exhaust the cache. */
  begin_test(3);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  retval_cache_append("x5", 2, "x5 lol", 6);
  retval_cache_append("x6", 2, "x6 lol", 6);
  retval_cache_append("x7", 2, "x7 lol", 6);
  retval_cache_append("x8", 2, "x8 lol", 6);
  ASSERT(is_cache_empty());
  finish_test();

  /* Test 4: ensure that retval_cache_output(1) fully clears cache. */
  begin_test(4);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  ASSERT(get_cache_size() == 4);
  ASSERT(!is_cache_empty());

  retval_cache_output(1);
  ASSERT(is_cache_empty());
  finish_test();

  /* Test 5: test max_cache setting. */
  begin_test(5);

  retval_cache_free();
  retval_cache_init(stdout, 5, true);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  retval_cache_append("x5", 2, "x5 lol", 6);
  ASSERT(get_cache_size() == 5);
  ASSERT(!is_cache_empty());

  retval_cache_append("x6", 2, "x6 lol", 6);
  ASSERT(get_cache_size() == 1);
  ASSERT(!is_cache_empty());

  retval_cache_output(1);
  finish_test();

  /*
  retval_cache_set_return_value("x7", 2, (void *)7);
  retval_cache_set_return_value("x6", 2, (void *)6);
  retval_cache_set_return_value("x5", 2, (void *)5);
  retval_cache_set_return_value("x4", 2, (void *)4);
  retval_cache_set_return_value("x3", 2, (void *)3);
  retval_cache_set_return_value("x2", 2, (void *)2);
  retval_cache_set_return_value("x1", 2, (void *)1);
  */

  /*
  for (unsigned int i = 0; i < 32; i++) {
    char name[8] = {0};
    char log[16] = {0};

    snprintf(name, sizeof(name) - 1, "x%u", i);
    snprintf(log, sizeof(log) - 1, "x%u lol", i);
    printf("appending [%s][%s]\n", name, log);
    retval_cache_append(name, strlen(name), log, strlen(log));
  }
  */

  retval_cache_free();
  return 0;
}

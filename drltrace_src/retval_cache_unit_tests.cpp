/* g++ -fsanitize=address -fno-omit-frame-pointer -g -Wno-format -DUNIT_TESTS -o retval_cache_unit_tests retval_cache_unit_tests.cpp  drltrace_retval_cache.cpp */


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
  unsigned int thread1 = 55, thread2 = 66, thread3 = 77;
  retval_cache_init(stdout, 0, true);


  /* Test 1: add single entry to cache, then match its return value. */
  begin_test(1);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)666);
  ASSERT(is_cache_empty());
  finish_test();


  /* Test 2: add three entires, then match their return values. */
  begin_test(2);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  ASSERT(!is_cache_empty());
  ASSERT(get_cache_size() == 3);
  retval_cache_set_return_value(thread1, "x3", 2, (void *)3);
  retval_cache_set_return_value(thread1, "x2", 2, (void *)2);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)1);
  ASSERT(is_cache_empty());
  finish_test();


  /* Test 3: exhaust the cache. */
  begin_test(3);

  /* All one thread. */
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  retval_cache_append(thread1, "x5", 2, "x5 lol", 6);
  retval_cache_append(thread1, "x6", 2, "x6 lol", 6);
  retval_cache_append(thread1, "x7", 2, "x7 lol", 6);
  retval_cache_append(thread1, "x8", 2, "x8 lol", 6);
  ASSERT(is_cache_empty());

  /* Multiple threads. */
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread2, "y2", 2, "y2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  retval_cache_append(thread3, "z5", 2, "z5 lol", 6);
  retval_cache_append(thread2, "y6", 2, "y6 lol", 6);
  retval_cache_append(thread1, "x7", 2, "x7 lol", 6);
  retval_cache_append(thread1, "x8", 2, "x8 lol", 6);
  ASSERT(is_cache_empty());
  finish_test();


  /* Test 4: ensure that retval_cache_dump_all() fully clears cache. */
  begin_test(4);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  ASSERT(get_cache_size() == 4);
  ASSERT(!is_cache_empty());

  retval_cache_dump_all();
  ASSERT(is_cache_empty());
  finish_test();


  /* Test 5: test max_cache setting. */
  begin_test(5);

  ASSERT(retval_cache_free());
  retval_cache_init(stdout, 5, true);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  retval_cache_append(thread1, "x5", 2, "x5 lol", 6);
  ASSERT(get_cache_size() == 5);
  ASSERT(!is_cache_empty());

  retval_cache_append(thread1, "x6", 2, "x6 lol", 6);
  ASSERT(get_cache_size() == 1);
  ASSERT(!is_cache_empty());

  retval_cache_output(thread1, true);
  ASSERT(is_cache_empty());

  /* Reset the cache back to unlimited entries. */
  ASSERT(retval_cache_free());
  retval_cache_init(stdout, 0, true);
  finish_test();


  /* Test 6: cache return values from two threads (different functions). */
  begin_test(6);
  ASSERT(get_cache_size() == 0);
  retval_cache_append(thread2, "y1", 2, "y1 lol", 6);
  retval_cache_append(thread2, "y2", 2, "y2 lol", 6);
  retval_cache_append(thread2, "y3", 2, "y3 lol", 6);
  ASSERT(get_cache_size() == 3);

  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  ASSERT(get_cache_size() == 6);

  retval_cache_set_return_value(thread2, "y3", 2, (void *)33);
  retval_cache_set_return_value(thread2, "y2", 2, (void *)22);
  retval_cache_set_return_value(thread2, "y1", 2, (void *)11);
  ASSERT(get_cache_size() == 3);

  retval_cache_set_return_value(thread1, "x3", 2, (void *)3);
  retval_cache_set_return_value(thread1, "x2", 2, (void *)2);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)1);
  ASSERT(get_cache_size() == 0);
  finish_test();


  /* Test 7: two threads calling the same functions with different args. */
  begin_test(7);

  retval_cache_append(thread2, "x1", 2, "x1 t2", 5);
  retval_cache_append(thread2, "x2", 2, "x2 t2", 5);
  retval_cache_append(thread1, "x1", 2, "x1 t1", 5);
  retval_cache_append(thread1, "x2", 2, "x2 t1", 5);
  retval_cache_append(thread2, "x3", 2, "x3 t2", 5);
  retval_cache_append(thread1, "x3", 2, "x3 t1", 5);
  ASSERT(get_cache_size() == 6);

  retval_cache_set_return_value(thread1, "x3", 2, (void *)3);
  retval_cache_set_return_value(thread1, "x2", 2, (void *)2);

  retval_cache_set_return_value(thread2, "x3", 2, (void *)6);
  retval_cache_set_return_value(thread2, "x2", 2, (void *)5);

  retval_cache_set_return_value(thread1, "x1", 2, (void *)1);
  ASSERT(get_cache_size() == 3);

  retval_cache_set_return_value(thread2, "x1", 2, (void *)4);
  ASSERT(get_cache_size() == 0);
  ASSERT(is_cache_empty());
  finish_test();


  /* Test 8: three threads with intertwined call stacks. */
  begin_test(8);

  retval_cache_append(thread1, "f1", 2, "f1 t1", 5);
  retval_cache_append(thread2, "f1", 2, "f1 t2", 5);
  retval_cache_append(thread2, "f2", 2, "f2 t2", 5);
  retval_cache_append(thread1, "f2", 2, "f2 t1", 5);
  retval_cache_append(thread3, "f4", 2, "f4 t3", 5);
  retval_cache_append(thread1, "f3", 2, "f3 t1", 5);
  retval_cache_append(thread3, "f5", 2, "f5 t3", 5);
  retval_cache_set_return_value(thread3, "f5", 2, (void *)9999);
  retval_cache_set_return_value(thread3, "f4", 2, (void *)10000); /* t3 fully unwinds */
  ASSERT(get_cache_size() == 5);

  retval_cache_append(thread2, "f3", 2, "f3 t2", 5);
  retval_cache_append(thread2, "f4", 2, "f4 t2", 5);

  retval_cache_set_return_value(thread2, "f4", 2, (void *)669); /* t2 partly unwinds */
  retval_cache_set_return_value(thread1, "f3", 2, (void *)3);   /* t1 partly unwinds */
  ASSERT(get_cache_size() == 7); /* Size doesn't decrease because no full unwind. */

  retval_cache_set_return_value(thread1, "f2", 2, (void *)2);
  retval_cache_set_return_value(thread1, "f1", 2, (void *)1); /* t1 fully unwinds */
  ASSERT(get_cache_size() == 4);

  retval_cache_set_return_value(thread2, "f3", 2, (void *)668);
  retval_cache_set_return_value(thread2, "f2", 2, (void *)667);
  retval_cache_set_return_value(thread2, "f1", 2, (void *)666); /* t2 fully unwinds */
  ASSERT(is_cache_empty());
  finish_test();


  ASSERT(retval_cache_free());
  return 0;
}

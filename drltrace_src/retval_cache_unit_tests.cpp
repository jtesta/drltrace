/* g++ -fsanitize=address -fno-omit-frame-pointer -g -Wno-format -DUNIT_TESTS -o retval_cache_unit_tests retval_cache_unit_tests.cpp  drltrace_retval_cache.cpp */

#pragma GCC diagnostic ignored "-Wformat"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "drltrace_retval_cache.h"


#define ASSERT(x) { if (!(x)) { printf("Assert failed: " #x "\n"); return -1; } }


void begin_test(unsigned int test_num) {
  printf("Test #%u:\n\n", test_num);
}

void finish_test() {
  printf("\nSuccess.\n\n\n");
}

int main(int ac, char **av) {
  unsigned int thread1 = 0, thread2 = 0, thread3 = 0, thread4 = 0;


  retval_cache_init(stdout, 0, true);

  /* Test 1: add single entry to cache from thread ID 55, then match its return
   * value. */
  begin_test(1);
  thread1 = 55;
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  ASSERT(get_num_threads() == 1);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)666);
  ASSERT(is_thread_cache_empty(thread1));
  ASSERT(is_global_cache_empty());
  finish_test();

  /* Test 2: add three entires, then match their return values. */
  begin_test(2);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  ASSERT(!is_thread_cache_empty(thread1));
  ASSERT(get_cache_size() == 3);
  retval_cache_set_return_value(thread1, "x3", 2, (void *)3);
  retval_cache_set_return_value(thread1, "x2", 2, (void *)2);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)1);
  ASSERT(is_thread_cache_empty(thread1));
  ASSERT(is_global_cache_empty());
  finish_test();

  /* Test 3: exhaust the cache. */
  begin_test(3);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  retval_cache_append(thread1, "x5", 2, "x5 lol", 6);
  retval_cache_append(thread1, "x6", 2, "x6 lol", 6);
  retval_cache_append(thread1, "x7", 2, "x7 lol", 6);
  retval_cache_append(thread1, "x8", 2, "x8 lol", 6);
  ASSERT(get_cache_size() == 8);
  retval_cache_append(thread1, "x9", 2, "x9 lol", 6);
  ASSERT(get_cache_size() == 1);
  retval_cache_set_return_value(thread1, "x9", 2, (void *)9);
  ASSERT(is_thread_cache_empty(thread1));
  ASSERT(is_global_cache_empty());
  finish_test();

  /* Test 4: ensure that retval_cache_output(X, true) fully clears cache. */
  begin_test(4);
  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  ASSERT(get_cache_size() == 4);
  ASSERT(!is_thread_cache_empty(thread1));

  retval_cache_output(0, true);
  ASSERT(is_thread_cache_empty(thread1));
  ASSERT(is_global_cache_empty());
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
  ASSERT(!is_thread_cache_empty(thread1));

  retval_cache_append(thread1, "x6", 2, "x6 lol", 6);
  ASSERT(get_cache_size() == 1);
  ASSERT(!is_thread_cache_empty(thread1));

  retval_cache_dump_all();
  ASSERT(is_global_cache_empty());

  /* Reset the max_cache setting to unlimited. */
  ASSERT(retval_cache_free());
  retval_cache_init(stdout, 0, true);

  finish_test();

  /* Test 6: caching two threads. */
  begin_test(6);
  ASSERT(get_num_threads() == 0);

  thread2 = 66;
  retval_cache_append(thread2, "y1", 2, "y1 lol", 6);
  retval_cache_append(thread2, "y2", 2, "y2 lol", 6);
  retval_cache_append(thread2, "y3", 2, "y3 lol", 6);
  ASSERT(get_num_threads() == 1);

  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);
  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  ASSERT(get_num_threads() == 2);

  /* Process return values slightly out of order. */
  retval_cache_set_return_value(thread1, "x3", 2, (void *)3);
  retval_cache_set_return_value(thread1, "x2", 2, (void *)2);
  retval_cache_set_return_value(thread2, "y3", 2, (void *)6);
  retval_cache_set_return_value(thread1, "x1", 2, (void *)1);
  ASSERT(is_thread_cache_empty(thread1));

  retval_cache_set_return_value(thread2, "y2", 2, (void *)5);
  retval_cache_set_return_value(thread2, "y1", 2, (void *)4);
  ASSERT(is_thread_cache_empty(thread2));

  ASSERT(get_num_threads() == 2);
  ASSERT(is_global_cache_empty());
  finish_test();

  /* Test 7: Filling one thread's cache only empty's that thread's cache. */
  begin_test(7);
  thread3 = 77;

  retval_cache_append(thread1, "x1", 2, "x1 lol", 6);
  retval_cache_append(thread1, "x2", 2, "x2 lol", 6);

  retval_cache_append(thread2, "y1", 2, "y1 lol", 6);

  retval_cache_append(thread1, "x3", 2, "x3 lol", 6);
  retval_cache_append(thread1, "x4", 2, "x4 lol", 6);
  retval_cache_append(thread1, "x5", 2, "x5 lol", 6);

  retval_cache_append(thread3, "z1", 2, "z1 lol", 6);
  retval_cache_append(thread2, "y2", 2, "y2 lol", 6);
  
  retval_cache_append(thread1, "x6", 2, "x6 lol", 6);
  retval_cache_append(thread1, "x7", 2, "x7 lol", 6);
  retval_cache_append(thread1, "x8", 2, "x8 lol", 6);

  ASSERT(get_cache_size() == 11);
  
  retval_cache_append(thread1, "x9", 2, "x9 lol", 6);
  ASSERT(get_cache_size() == 4);
  ASSERT(!is_thread_cache_empty(thread2));
  ASSERT(!is_thread_cache_empty(thread3));

  retval_cache_set_return_value(thread1, "x9", 2, (void *)9);
  ASSERT(get_cache_size() == 3);

  retval_cache_set_return_value(thread2, "y2", 2, (void *)2);
  retval_cache_set_return_value(thread2, "y1", 2, (void *)1);
  ASSERT(get_cache_size() == 1);
  
  retval_cache_set_return_value(thread3, "z1", 2, (void *)11);
  ASSERT(get_cache_size() == 0);
  ASSERT(is_thread_cache_empty(thread1));
  ASSERT(is_thread_cache_empty(thread2));
  ASSERT(is_thread_cache_empty(thread3));
  ASSERT(is_global_cache_empty());
  finish_test();

  /* Test 8: Allocating maximum threads flushes all caches. */
  begin_test(8);
  ASSERT(get_num_threads() == 3);

  retval_cache_append(thread2, "y1", 2, "y1 lol", 6);
  retval_cache_append(thread3, "z1", 2, "z1 lol", 6);
  ASSERT(get_cache_size() == 2);

  thread4 = 88;
  retval_cache_append(thread4, "a1", 2, "x1 lol", 6);
  ASSERT(get_cache_size() == 1);

  printf("num_threads(): %u\n", get_num_threads());
  ASSERT(get_num_threads() == 1);
  
  finish_test();
  
  /* Test 9: Unused thread slot gets re-purposed. */
  
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

  ASSERT(retval_cache_free());
  return 0;
}

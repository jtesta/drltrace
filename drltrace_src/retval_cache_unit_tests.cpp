#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "drltrace_retval_cache.h"


/*
int
fast_strcmp(char *s1, size_t s1_len, char *s2, size_t s2_len) {
  if (s1_len != s2_len)
    return -1;

#ifdef WINDOWS
  return memcmp(s1, s2, s1_len); * VC2013 doesn't have bcmp(), sadly. *
#else
  return bcmp(s1, s2, s1_len);  * Fastest option. *
#endif
}
*/

#define ASSERT(x) { if (! (x)) { printf("Assert failed: " #x "\n"); } }


int main(int ac, char **av) {
  retval_cache_init(stdout, 0);

  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_set_return_value("x1", 2, (void *)666);
  ASSERT(is_cache_empty());

  printf("---\n");

  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  ASSERT(!is_cache_empty());
  retval_cache_set_return_value("x3", 2, (void *)69);
  retval_cache_set_return_value("x2", 2, (void *)5);
  retval_cache_set_return_value("x1", 2, (void *)6);
  ASSERT(is_cache_empty());

  printf("---\n");
  
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  retval_cache_append("x5", 2, "x5 lol", 6);
  retval_cache_append("x6", 2, "x6 lol", 6);
  retval_cache_append("x7", 2, "x7 lol", 6);
  retval_cache_append("x8", 2, "x8 lol", 6);

  // Page 0 is now filled.
  ASSERT(get_start_index() == 0);
  ASSERT(get_last_used_array() == 1);
  ASSERT(get_free_index() == 0);

  retval_cache_append("x9", 2, "x9 lol", 6);

  // One entry on page 1 is filled.
  ASSERT(get_start_index() == 0);
  ASSERT(get_last_used_array() == 1);
  ASSERT(get_free_index() == 1);

  retval_cache_set_return_value("x9", 2, (void *)9);

  // First entry on page 1 now has return value.
  ASSERT(get_start_index() == 0);
  ASSERT(get_last_used_array() == 1);
  ASSERT(get_free_index() == 1);
  //ASSERT(cached_function_calls[1][0].retval_set == 1);
  
  retval_cache_set_return_value("x8", 2, (void *)8);

  // Entry 7 on page 0 has a return value.
  ASSERT(get_start_index() == 0);
  ASSERT(get_last_used_array() == 1);
  ASSERT(get_free_index() == 1);
  //ASSERT(cached_function_calls[0][7].retval_set == 1);

  retval_cache_set_return_value("x7", 2, (void *)7);
  retval_cache_set_return_value("x6", 2, (void *)6);
  retval_cache_set_return_value("x5", 2, (void *)5);
  retval_cache_set_return_value("x4", 2, (void *)4);
  retval_cache_set_return_value("x3", 2, (void *)3);
  retval_cache_set_return_value("x2", 2, (void *)2);
  retval_cache_set_return_value("x1", 2, (void *)1);

  // Cache should be empty.
  ASSERT(is_cache_empty() == 1);
  
  printf("---\n");

  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  retval_cache_append("x5", 2, "x5 lol", 6);
  retval_cache_append("x6", 2, "x6 lol", 6);
  retval_cache_append("x7", 2, "x7 lol", 6);
  retval_cache_append("x8", 2, "x8 lol", 6);

  // Page 0 is now filled.
  ASSERT(get_start_index() == 0);
  ASSERT(get_last_used_array() == 1);
  ASSERT(get_free_index() == 0);
  ASSERT(get_cache_size() == 8);

  retval_cache_append("x9", 2, "x9 lol", 6);

  retval_cache_set_return_value("x9", 2, (void *)9);
  retval_cache_set_return_value("x8", 2, (void *)8);
  retval_cache_set_return_value("x7", 2, (void *)7);
  retval_cache_set_return_value("x6", 2, (void *)6);
  retval_cache_set_return_value("x5", 2, (void *)5);
  retval_cache_set_return_value("x4", 2, (void *)4);
  retval_cache_set_return_value("x3", 2, (void *)3);
  retval_cache_set_return_value("x2", 2, (void *)2);
  retval_cache_set_return_value("x1", 2, (void *)1);

  // Cache should be empty.
  ASSERT(is_cache_empty() == 1);

  printf("---\n");
  
  for (unsigned int i = 0; i < 32; i++) {
    char name[8] = {0};
    char log[16] = {0};

    snprintf(name, sizeof(name) - 1, "x%u", i);
    snprintf(log, sizeof(log) - 1, "x%u lol", i);
    printf("appending [%s][%s]\n", name, log);
    retval_cache_append(name, strlen(name), log, strlen(log));
  }

  /* This fills up the arrays, triggering a cache dump.  Hence, it should be empty
   * now. */
  ASSERT(is_cache_empty() == 1);

  printf("---\n");

  set_max_cache_size(5);
  retval_cache_append("x1", 2, "x1 lol", 6);
  retval_cache_append("x2", 2, "x2 lol", 6);
  retval_cache_append("x3", 2, "x3 lol", 6);
  retval_cache_append("x4", 2, "x4 lol", 6);
  retval_cache_append("x5", 2, "x5 lol", 6);
  retval_cache_append("x6", 2, "x6 lol", 6);

  /* With a max_cache_size of 5, the 6th entry triggers a dump before adding it.  So
   * the cache size should only be 1 now. */
  ASSERT(get_cache_size() == 1);
  
  retval_cache_set_return_value("x6", 2, (void *)6);
  retval_cache_set_return_value("x5", 2, (void *)5);
  retval_cache_set_return_value("x4", 2, (void *)4);
  retval_cache_set_return_value("x3", 2, (void *)3);
  retval_cache_set_return_value("x2", 2, (void *)2);
  retval_cache_set_return_value("x1", 2, (void *)1);

  
  
  retval_cache_free();
  return 0;
}

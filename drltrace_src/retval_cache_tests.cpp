#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0
#define dr_fprintf fprintf
#define outf stdout
#define STDERR stdout

int
fast_strcmp(char *s1, size_t s1_len, char *s2, size_t s2_len) {
  if (s1_len != s2_len)
    return -1;

#ifdef WINDOWS
  return memcmp(s1, s2, s1_len); /* VC2013 doesn't have bcmp(), sadly. */
#else
  return bcmp(s1, s2, s1_len);  /* Fastest option. */
#endif
}


static unsigned int max_cache_size = 0;
static unsigned int cache_dump_triggered = 0;

/* The size of one array of cached_function_call elements. */
#define CACHED_FUNCTION_CALLS_ARRAY_SIZE 8

/* The number of cached_function_call arrays (each of size
 * CACHED_FUNCTION_CALLS_ARRAY_SIZE). */
#define CACHED_FUNCTION_CALLS_NUM_ARRAYS 4

struct _cached_function_call {
  char *function_call; /* Contains the module name, function name, and arguments. */
  unsigned int function_call_len;  /* Length of the function_call string. */
  unsigned int retval_set;   /* Set to 1 when retval is set, otherwise 0. */
  void *retval;              /* The return value of the function. */
};
typedef struct _cached_function_call cached_function_call;


static cached_function_call **cached_function_calls = NULL;

/* Points to the first index in array 0 that is valid (previous entries are no longer
 * valid, as they have left the cache).  This is where searching begins when a return
 * value is received. */
static unsigned int cached_function_call_start_index = 0;

/* The index of the last used array in cached_function_calls. */
static unsigned int cached_function_call_last_used_array = 0;

/* Points to the first free index in the last used array (i.e.: the slot to put new
 * cache entries). */
static unsigned int cached_function_call_free_index = 0;

static unsigned int cache_size = 0;

static void
cached_function_call_output_cache(unsigned int clear_all) {
  /* If the caller wants to dump the cache, set this flag so that later return value
   * calls don't print error messages when the entry can't be found. */
  if (clear_all)
    cache_dump_triggered = 1;
  
  while(true) {
    cached_function_call *cfc_array = cached_function_calls[0];
    while (cached_function_call_start_index < CACHED_FUNCTION_CALLS_ARRAY_SIZE) {
      //for (; cached_function_call_start_index < CACHED_FUNCTION_CALLS_ARRAY_SIZE; \
      //  cached_function_call_start_index++) {

      /* Print the function and return value. */
      unsigned int retval_set = cfc_array[cached_function_call_start_index].retval_set;
      if (retval_set || clear_all) {
	//dr_fprintf(outf, "DONE: ");
	dr_fprintf(outf, "%s", cfc_array[cached_function_call_start_index].function_call);
	void *retval = cfc_array[cached_function_call_start_index].retval;
	if (true) { // -grepable
	  if (retval_set)
	    dr_fprintf(outf, " = 0x%"PRIx64"\n", (long unsigned int)retval);
	  else
	    dr_fprintf(outf, " = ?\n");
	} else {
	  if (retval_set)
	    dr_fprintf(outf, "\n    ret: 0x%"PRIx64, (long unsigned int)retval);
	  else
	    dr_fprintf(outf, "\n    ret: ?");
	}

	free(cfc_array[cached_function_call_start_index].function_call);
	cache_size--;
	cached_function_call_start_index++;
	//dr_fprintf(outf, "  Incremented start_index to: %u\n", cached_function_call_start_index);

      /* We found the first entry that doesn't have a set return value.  So we're
       * done. */
      } else
	goto done;

      //dr_fprintf(outf, "  last_used_array: %u; start_index: %u; free_index: %u; ARRAY_SIZE: %u\n", cached_function_call_last_used_array, cached_function_call_start_index, cached_function_call_free_index, CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      /* If we reached the end of the cache... */
      if ((cached_function_call_last_used_array == 0) &&		\
	  ((cached_function_call_start_index == cached_function_call_free_index) ||
	   (cached_function_call_start_index == CACHED_FUNCTION_CALLS_ARRAY_SIZE))) {
	//dr_fprintf(outf, "\nCACHE IS EMPTY\n\n");
	/* Since the cache is now empty, set the start index to zero so new entries
	 * are added from there. */
	cached_function_call_start_index = 0;
	cached_function_call_free_index = 0;
	goto done;
      }
    }

    /* Reset the start index, since we will be looking at the next array. */
    //dr_fprintf(outf, "\nBONK\n\n");
    cached_function_call_start_index = 0;

    /* Since we looked through the entire first array, we now need to shift all
     * arrays down one index.  Then we rotate array zero to the end. */
    cached_function_call *cfc_finished = cached_function_calls[0];
    for (unsigned int i = 1; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++)
      cached_function_calls[i - 1] = cached_function_calls[i];

    cached_function_calls[CACHED_FUNCTION_CALLS_NUM_ARRAYS - 1] = cfc_finished;

    /* The logic above should have caught this case, but if it somehow doesn't... */
    if (cached_function_call_last_used_array == 0) {
      dr_fprintf(outf, "ERROR: fallback case encountered.  start_index: %u; free index: %u; CACHED_FUNCTION_CALLS_ARRAY_SIZE: %u\n", cached_function_call_start_index, cached_function_call_free_index, CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      goto done;
    }

    /* Since we shifted all the arrays down by one, above, the last used array
     * index needs to be decremented as well. */
    cached_function_call_last_used_array--;
    //printf("lua: %u\n", cached_function_call_last_used_array);
  }

 done:
  return;
}

static void
cached_function_call_append(char *module_and_function_name, size_t module_and_function_name_len, char *function_call, size_t function_call_len) {
  /*dr_fprintf(outf, "%s", function_call);
    dr_fprintf(outf, "\n");*/

  /* If the cache size hits the user-defined limit, immediately dump it all as-is into
   * the log, then we'll continue. */
  if ((max_cache_size > 0) && (cache_size >= max_cache_size))  /* TODO */
    cached_function_call_output_cache(1);
  
  /* The post-function callback for these functions are never called.  So instead of
   * letting them clog up the cache, we'll just dump them out immediately. */
  if ((fast_strcmp(module_and_function_name, module_and_function_name_len, \
           (char *)"libc.so.6!__libc_start_main", 27) == 0) || \
      (fast_strcmp(module_and_function_name, module_and_function_name_len, \
           (char *)"libc.so.6!exit", 14) == 0)) {
    //dr_fprintf(outf, "\nNot caching [%s].\n\n", function_call);
    dr_fprintf(outf, "%s", function_call);

    if (true) // -grepable
      dr_fprintf(outf, " = ??\n");
    else
      dr_fprintf(outf, "\n    ret: ??\n");

    return;
  }

  cached_function_call *last_used_array = cached_function_calls[ cached_function_call_last_used_array ];

  last_used_array[ cached_function_call_free_index ].function_call = strdup(function_call);
  last_used_array[ cached_function_call_free_index ].function_call_len = function_call_len;
  last_used_array[ cached_function_call_free_index ].retval_set = 0;
  cache_size++;
  cached_function_call_free_index++;

  if (cached_function_call_free_index >= CACHED_FUNCTION_CALLS_ARRAY_SIZE) {
    cached_function_call_free_index = 0;
    cached_function_call_last_used_array++;
    if (cached_function_call_last_used_array == CACHED_FUNCTION_CALLS_NUM_ARRAYS) {
      //dr_fprintf(outf, "Well, this is awkward... we somehow ran out of free cache entries (%lu)!  The only solution is to tell the developer and ask them to raise the limit.  Sorry.\n", CACHED_FUNCTION_CALLS_NUM_ARRAYS * CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      //exit(-1);

      /* We went past the last index of arrays, so back up by one; we won't be adding
       * more to the cache before clearing it below. */
      cached_function_call_last_used_array--;
      // TODO: print the first cache entry, since this is responsible for the hold-up.
      dr_fprintf(STDERR, "WARNING: return value cache exhausted!  Dumping all " \
                 "entries into the log file as-is.  Please report this message to " \
                 "the developer: %u %u %u\n", cache_size, \
                 CACHED_FUNCTION_CALLS_NUM_ARRAYS, CACHED_FUNCTION_CALLS_ARRAY_SIZE);

      cached_function_call_output_cache(1);
    }
  }
}

static void
cached_function_call_set_return_value(char *function_call, size_t function_call_len, void *retval) {

#define MIN(x,y) (((x) > (y)) ? (y) : (x))

  
  //dr_fprintf(outf, "\n\nSetting return value for [[%s]] to 0x%"PRIx64":\n", function_call, retval);
  
  unsigned int min_len;
  int j = cached_function_call_free_index - 1;
  //dr_fprintf(outf, "\n");
  for (int i = cached_function_call_last_used_array; i >= 0; i--) {
    cached_function_call *cfc_array = cached_function_calls[i];
    //dr_fprintf(outf, "i: %u\n", i);
    for (; j >= 0; j--) {
      //dr_fprintf(outf, "  j: %u\n", j);

      if (cfc_array[j].retval_set) {
	//dr_fprintf(outf, "  X retval already set.\n");
	continue; // !!!!!
      }
      
      /* Only compare the shortest prefix of the two strings. */
      min_len = MIN(function_call_len, cfc_array[j].function_call_len);

      //dr_fprintf(outf, "  fast_strcmp(\"%s\", %u, \"%s\", %u)\n", function_call, min_len, cfc_array[j].function_call, min_len);

      if (fast_strcmp(function_call, min_len, \
          cfc_array[j].function_call, min_len) == 0) {
	//dr_fprintf(outf, "  --> HIT\n\n");
	cfc_array[j].retval = retval;
	cfc_array[j].retval_set = 1;
	goto success;
      } else {
	//dr_fprintf(outf, "  --> MISS\n");
      }

      /* If we reached the end (or rather... the beginning...) of the cache... */
      if ((i == 0) && ((j == 0) || (j == cached_function_call_start_index)))
	goto failed;
    }
    j = CACHED_FUNCTION_CALLS_ARRAY_SIZE - 1;
  }

  /* If cached_function_call_output_cache(1) is called (i.e.: a cache dump), then
   * we might not find any matches above and we'll land here.  In that case, we
   * shouldn't complain. */
  if (!cache_dump_triggered) {
    /* Should never be reached... */
    dr_fprintf(outf, "Uh-oh.  This line shouldn't have been reached!\n");
  }


  goto failed;

 success:
  /* Check if we just stored the return value of the oldest entry in the cache.  If
   * so, then print out its log along with all sequential logs that have return
   * values set.  Stop when either a function with no return value is found, or the
   * end of the cache is reached. */
  //dr_fprintf(outf, "  j: %u; start_index: %u\n\n", j, cached_function_call_start_index);
  if (j == cached_function_call_start_index)
    cached_function_call_output_cache(0);

  return;

 failed:
  /* If cached_function_call_output_cache(1) is called (i.e.: a cache dump), then
   * we might not find any matches above and we'll land here.  In that case, we
   * shouldn't complain. */
  if (!cache_dump_triggered) {
    dr_fprintf(outf, "ERROR: failed to find cache entry for %s (return value: 0x%" PRIx64 ")\n", function_call, (long unsigned int)retval);
  }
  return;
}

static void
init_cached_function_calls() {

  if (true) { // -print_return_values
    cached_function_calls = (cached_function_call **)calloc( \
        CACHED_FUNCTION_CALLS_NUM_ARRAYS, sizeof(cached_function_call *));
    if (cached_function_calls == NULL) {
      fprintf(stderr, "Failed to create cached_function_calls array.\n");
      exit(-1);
    }

    for (unsigned int i = 0; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++) {
      cached_function_calls[i] = (cached_function_call *)calloc( \
          CACHED_FUNCTION_CALLS_ARRAY_SIZE, sizeof(cached_function_call));
      if (cached_function_calls[i] == NULL) {
	fprintf(stderr, "Failed to create cached_function_calls array.\n");
	exit(-1);
      }
    }
  }

}

/* TODO: free all entries in here too! */
static void
free_cached_function_calls() {
  if (cached_function_calls == NULL)
    return;

  if (cache_size != 0) {
    fprintf(stderr, "WARNING: freeing return value cache even though it is not " \
        "empty!: %u\n", cache_size);
  }
  
  for (unsigned int i = 0; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++) {
    free(cached_function_calls[i]);  cached_function_calls[i] = NULL;
  }

  free(cached_function_calls);  cached_function_calls = NULL;
}

#define ASSERT(x) { if (! (x)) { printf("Assert failed: " #x "\n"); } }

static int is_cache_empty() {
  if ((cache_size == 0) &&
     (cached_function_call_start_index == 0) &&
     (cached_function_call_last_used_array == 0) &&
     (cached_function_call_free_index == 0))
    return 1;
  else
    return 0;
}

int main(int ac, char **av) {
  init_cached_function_calls();

  cached_function_call_append("x1", 2, "x1 lol", 6);
  cached_function_call_set_return_value("x1", 2, (void *)666);
  ASSERT(is_cache_empty());

  printf("---\n");

  cached_function_call_append("x1", 2, "x1 lol", 6);
  cached_function_call_append("x2", 2, "x2 lol", 6);
  cached_function_call_append("x3", 2, "x3 lol", 6);
  ASSERT(!is_cache_empty());
  cached_function_call_set_return_value("x3", 2, (void *)69);
  cached_function_call_set_return_value("x2", 2, (void *)5);
  cached_function_call_set_return_value("x1", 2, (void *)6);
  ASSERT(is_cache_empty());

  printf("---\n");
  
  cached_function_call_append("x1", 2, "x1 lol", 6);
  cached_function_call_append("x2", 2, "x2 lol", 6);
  cached_function_call_append("x3", 2, "x3 lol", 6);
  cached_function_call_append("x4", 2, "x4 lol", 6);
  cached_function_call_append("x5", 2, "x5 lol", 6);
  cached_function_call_append("x6", 2, "x6 lol", 6);
  cached_function_call_append("x7", 2, "x7 lol", 6);
  cached_function_call_append("x8", 2, "x8 lol", 6);

  // Page 0 is now filled.
  ASSERT(cached_function_call_start_index == 0);
  ASSERT(cached_function_call_last_used_array == 1);
  ASSERT(cached_function_call_free_index == 0);

  cached_function_call_append("x9", 2, "x9 lol", 6);

  // One entry on page 1 is filled.
  ASSERT(cached_function_call_start_index == 0);
  ASSERT(cached_function_call_last_used_array == 1);
  ASSERT(cached_function_call_free_index == 1);

  cached_function_call_set_return_value("x9", 2, (void *)9);

  // First entry on page 1 now has return value.
  ASSERT(cached_function_call_start_index == 0);
  ASSERT(cached_function_call_last_used_array == 1);
  ASSERT(cached_function_call_free_index == 1);
  ASSERT(cached_function_calls[1][0].retval_set == 1);
  
  cached_function_call_set_return_value("x8", 2, (void *)8);

  // Entry 7 on page 0 has a return value.
  ASSERT(cached_function_call_start_index == 0);
  ASSERT(cached_function_call_last_used_array == 1);
  ASSERT(cached_function_call_free_index == 1);
  ASSERT(cached_function_calls[0][7].retval_set == 1);

  cached_function_call_set_return_value("x7", 2, (void *)7);
  cached_function_call_set_return_value("x6", 2, (void *)6);
  cached_function_call_set_return_value("x5", 2, (void *)5);
  cached_function_call_set_return_value("x4", 2, (void *)4);
  cached_function_call_set_return_value("x3", 2, (void *)3);
  cached_function_call_set_return_value("x2", 2, (void *)2);
  cached_function_call_set_return_value("x1", 2, (void *)1);

  // Cache should be empty.
  ASSERT(is_cache_empty() == 1);
  
  printf("---\n");

  cached_function_call_append("x1", 2, "x1 lol", 6);
  cached_function_call_append("x2", 2, "x2 lol", 6);
  cached_function_call_append("x3", 2, "x3 lol", 6);
  cached_function_call_append("x4", 2, "x4 lol", 6);
  cached_function_call_append("x5", 2, "x5 lol", 6);
  cached_function_call_append("x6", 2, "x6 lol", 6);
  cached_function_call_append("x7", 2, "x7 lol", 6);
  cached_function_call_append("x8", 2, "x8 lol", 6);

  // Page 0 is now filled.
  ASSERT(cached_function_call_start_index == 0);
  ASSERT(cached_function_call_last_used_array == 1);
  ASSERT(cached_function_call_free_index == 0);
  ASSERT(cache_size == 8);

  cached_function_call_append("x9", 2, "x9 lol", 6);

  cached_function_call_set_return_value("x9", 2, (void *)9);
  cached_function_call_set_return_value("x8", 2, (void *)8);
  cached_function_call_set_return_value("x7", 2, (void *)7);
  cached_function_call_set_return_value("x6", 2, (void *)6);
  cached_function_call_set_return_value("x5", 2, (void *)5);
  cached_function_call_set_return_value("x4", 2, (void *)4);
  cached_function_call_set_return_value("x3", 2, (void *)3);
  cached_function_call_set_return_value("x2", 2, (void *)2);
  cached_function_call_set_return_value("x1", 2, (void *)1);

  // Cache should be empty.
  ASSERT(is_cache_empty() == 1);

  printf("---\n");
  
  for (unsigned int i = 0; i < 32; i++) {
    char name[8] = {0};
    char log[16] = {0};

    snprintf(name, sizeof(name) - 1, "x%u", i);
    snprintf(log, sizeof(log) - 1, "x%u lol", i);
    printf("appending [%s][%s]\n", name, log);
    cached_function_call_append(name, strlen(name), log, strlen(log));
  }

  /* This fills up the arrays, triggering a cache dump.  Hence, it should be empty
   * now. */
  ASSERT(is_cache_empty() == 1);

  printf("---\n");

  max_cache_size = 5;
  cached_function_call_append("x1", 2, "x1 lol", 6);
  cached_function_call_append("x2", 2, "x2 lol", 6);
  cached_function_call_append("x3", 2, "x3 lol", 6);
  cached_function_call_append("x4", 2, "x4 lol", 6);
  cached_function_call_append("x5", 2, "x5 lol", 6);
  cached_function_call_append("x6", 2, "x6 lol", 6);

  /* With a max_cache_size of 5, the 6th entry triggers a dump before adding it.  So
   * the cache size should only be 1 now. */
  ASSERT(cache_size == 1);
  
  cached_function_call_set_return_value("x6", 2, (void *)6);
  cached_function_call_set_return_value("x5", 2, (void *)5);
  cached_function_call_set_return_value("x4", 2, (void *)4);
  cached_function_call_set_return_value("x3", 2, (void *)3);
  cached_function_call_set_return_value("x2", 2, (void *)2);
  cached_function_call_set_return_value("x1", 2, (void *)1);

  
  
  free_cached_function_calls();
  return 0;
}

/*
cached_function_call_start_index
cached_function_call_last_used_array
cached_function_call_free_index
*/

/*
 * Copyright (C) 2020  Joe Testa <jtesta@positronsecurity.com>
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "drltrace_retval_cache.h"
#include "drltrace_utils.h"


#ifdef UNIT_TESTS /* For unit testing the return value caching code. */

  #define CACHED_FUNCTION_CALLS_ARRAY_SIZE 8
  #define CACHED_FUNCTION_CALLS_NUM_ARRAYS 4

  #define dr_fprintf fprintf
  #define STDERR stderr

  FILE *out_stream;
#else

  /* The size of one array of cached_function_call elements. */
  /* Note: this must be less than 2^31, since a signed counter iterates over it */
  #define CACHED_FUNCTION_CALLS_ARRAY_SIZE (1024 * 16)

  /* The number of cached_function_call arrays (each of size
   * CACHED_FUNCTION_CALLS_ARRAY_SIZE). */
  /* Note: this must be less than 2^31, since a signed counter iterates over it */
  #define CACHED_FUNCTION_CALLS_NUM_ARRAYS 16

  file_t out_stream;
#endif

bool grepable_output = false;

static cached_function_call **retval_cache_arrays = NULL;

/* Points to the first index in array 0 that is valid (previous entries are no longer
 * valid, as they have left the cache).  This is where searching begins when a return
 * value is received. */
static unsigned int start_index = 0;

/* The index of the last used array in retval_cache_arrays. */
static unsigned int last_used_array = 0;

/* Points to the first free index in the last used array (i.e.: the slot to put new
 * cache entries). */
static unsigned int free_index = 0;

// TODO: document this.
static unsigned int cache_size = 0;
static unsigned int max_cache_size = 0;
static unsigned int cache_dump_triggered = 0;


/* Outputs and clears as many entries with return values from the cache as possible
 * (which may be zero).  If 'clear_all' is set, then all entries are outputted &
 * cleared. */
void
retval_cache_output(unsigned int clear_all) {

  /* If the caller wants to dump the cache, set this flag so that later return value
   * calls don't print error messages when the entry can't be found. */
  if (clear_all)
    cache_dump_triggered = 1;

  while(true) {
    cached_function_call *cfc_array = retval_cache_arrays[0];
    while (start_index < CACHED_FUNCTION_CALLS_ARRAY_SIZE) {
      //for (; start_index < CACHED_FUNCTION_CALLS_ARRAY_SIZE; \
      //  start_index++) {

      /* Print the function and return value. */
      unsigned int retval_set = cfc_array[start_index].retval_set;
      if (retval_set || clear_all) {
	//dr_fprintf(outf, "DONE: ");
	dr_fprintf(out_stream, "%s", cfc_array[start_index].function_call);
	void *retval = cfc_array[start_index].retval;
	if (true) { // -grepable
	  if (retval_set)
	    dr_fprintf(out_stream, " = 0x%"PRIx64"\n", retval);
	  else
	    dr_fprintf(out_stream, " = ?\n");
	} else {
	  if (retval_set)
	    dr_fprintf(out_stream, "\n    ret: 0x%"PRIx64, retval);
	  else
	    dr_fprintf(out_stream, "\n    ret: ?");
	}

	free(cfc_array[start_index].function_call);
	cache_size--;
	start_index++;
	//dr_fprintf(outf, "  Incremented start_index to: %u\n", start_index);

      /* We found the first entry that doesn't have a set return value.  So we're
       * done. */
      } else
	goto done;

      //dr_fprintf(outf, "  last_used_array: %u; start_index: %u; free_index: %u; ARRAY_SIZE: %u\n", last_used_array, start_index, free_index, CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      /* If we reached the end of the cache... */
      if ((last_used_array == 0) &&		\
	  ((start_index == free_index) ||
	   (start_index == CACHED_FUNCTION_CALLS_ARRAY_SIZE))) {
	//dr_fprintf(outf, "\nCACHE IS EMPTY\n\n");
	/* Since the cache is now empty, set the start index to zero so new entries
	 * are added from there. */
	start_index = 0;
	free_index = 0;
	goto done;
      }
    }

    /* Reset the start index, since we will be looking at the next array. */
    //dr_fprintf(outf, "\nBONK\n\n");
    start_index = 0;

    /* Since we looked through the entire first array, we now need to shift all
     * arrays down one index.  Then we rotate array zero to the end. */
    cached_function_call *cfc_finished = retval_cache_arrays[0];
    for (unsigned int i = 1; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++)
      retval_cache_arrays[i - 1] = retval_cache_arrays[i];

    retval_cache_arrays[CACHED_FUNCTION_CALLS_NUM_ARRAYS - 1] = cfc_finished;

    /* The logic above should have caught this case, but if it somehow doesn't... */
    if (last_used_array == 0) {
      dr_fprintf(out_stream, "ERROR: fallback case encountered.  start_index: %lu; free index: %lu; CACHED_FUNCTION_CALLS_ARRAY_SIZE: %lu\n", start_index, free_index, CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      goto done;
    }

    /* Since we shifted all the arrays down by one, above, the last used array
     * index needs to be decremented as well. */
    last_used_array--;
  }

 done:
  return;
}


// TODO: rename "function_call" to "func_full_name_and_args"
void
retval_cache_append(const char *module_and_function_name, size_t module_and_function_name_len, const char *function_call, size_t function_call_len) {
  /*dr_fprintf(outf, "%s", function_call);
    dr_fprintf(outf, "\n");*/

  /* If the cache size hits the user-defined limit, immediately dump it all as-is into
   * the log, then we'll continue. */
  if ((max_cache_size > 0) && (cache_size >= max_cache_size))
    retval_cache_output(1);

  /* The post-function callback for these functions are never called.  So instead of
   * letting them clog up the cache, we'll just dump them out immediately. */
  if ((fast_strcmp(module_and_function_name, module_and_function_name_len, \
           "libc.so.6!__libc_start_main", 27) == 0) || \
      (fast_strcmp(module_and_function_name, module_and_function_name_len, \
           "libc.so.6!exit", 14) == 0)) {
    //dr_fprintf(outf, "\nNot caching [%s].\n\n", function_call);
    dr_fprintf(out_stream, "%s", function_call);

    if (true) // -grepable
      dr_fprintf(out_stream, " = ??\n");
    else
      dr_fprintf(out_stream, "\n    ret: ??\n");

    return;
  }

  cached_function_call *arr = retval_cache_arrays[ last_used_array ];

  arr[free_index].function_call = strdup(function_call);
  arr[free_index].function_call_len = function_call_len;
  arr[free_index].retval_set = 0;
  cache_size++;
  free_index++;

  if (free_index == CACHED_FUNCTION_CALLS_ARRAY_SIZE) {
    free_index = 0;
    last_used_array++;
    if (last_used_array >= CACHED_FUNCTION_CALLS_NUM_ARRAYS) {
      //dr_fprintf(outf, "Well, this is awkward... we somehow ran out of free cache entries (%lu)!  The only solution is to tell the developer and ask them to raise the limit.  Sorry.\n", CACHED_FUNCTION_CALLS_NUM_ARRAYS * CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      //exit(-1);

      /* We went past the last index of arrays, so back up by one; we won't be adding
       * more to the cache before clearing it below. */
      last_used_array--;

      // TODO: print the first cache entry, since this is responsible for the hold-up.
      dr_fprintf(STDERR, "WARNING: return value cache exhausted!  Dumping all " \
                 "entries into the log file as-is.  Please report this message to " \
                 "the developer: %u %u %u\n", cache_size, \
                 CACHED_FUNCTION_CALLS_NUM_ARRAYS, CACHED_FUNCTION_CALLS_ARRAY_SIZE);
      retval_cache_output(1);
    }
  }
}

// TODO: rename "function_call" to "module_name_and_function"
void
retval_cache_set_return_value(char *function_call, size_t function_call_len, void *retval) {

  //dr_fprintf(outf, "\n\nSetting return value for [[%s]] to 0x%"PRIx64":\n", function_call, retval);

  unsigned int min_len;
  int j = free_index - 1;
  //dr_fprintf(outf, "\n");
  for (int i = last_used_array; i >= 0; i--) {
    cached_function_call *cfc_array = retval_cache_arrays[i];
    //dr_fprintf(outf, "i: %u\n", i);
    for (; j >= 0; j--) {
      //dr_fprintf(outf, "  j: %u\n", j);

      if (cfc_array[j].retval_set) {
	//dr_fprintf(outf, "  X retval already set.\n");
	continue;
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
      if ((i == 0) && ((j == 0) || (j == start_index)))
	goto failed;
    }
    j = CACHED_FUNCTION_CALLS_ARRAY_SIZE - 1;
  }

  /* Should never be reached... */
  dr_fprintf(out_stream, "Uh-oh.  This line shouldn't have been reached!\n");
  goto failed;

 success:
  /* Check if we just stored the return value of the oldest entry in the cache.  If
   * so, then print out its log along with all sequential logs that have return
   * values set.  Stop when either a function with no return value is found, or the
   * end of the cache is reached. */
  //dr_fprintf(outf, "  j: %u; start_index: %u\n\n", j, start_index);
  if (j == start_index)
    retval_cache_output(0);

  return;

 failed:
  dr_fprintf(out_stream, "ERROR: failed to find cache entry for %s (return value: 0x%" PRIx64 ")\n", function_call, retval);
  return;
}

#ifdef UNIT_TESTS
void
retval_cache_init(FILE *_out_stream, unsigned int _max_cache_size, bool _grepable_output) {
#else
void
retval_cache_init(file_t _out_stream, unsigned int _max_cache_size, bool _grepable_output) {
#endif
  out_stream = _out_stream;
  max_cache_size = _max_cache_size;
  grepable_output = _grepable_output;
  
  if (true) { // -print_return_values
    retval_cache_arrays = (cached_function_call **)calloc( \
        CACHED_FUNCTION_CALLS_NUM_ARRAYS, sizeof(cached_function_call *));
    if (retval_cache_arrays == NULL) {
      fprintf(stderr, "Failed to create retval_cache_arrays array.\n");
      exit(-1);
    }

    for (unsigned int i = 0; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++) {
      retval_cache_arrays[i] = (cached_function_call *)calloc( \
          CACHED_FUNCTION_CALLS_ARRAY_SIZE, sizeof(cached_function_call));
      if (retval_cache_arrays[i] == NULL) {
	fprintf(stderr, "Failed to create retval_cache_arrays array.\n");
	exit(-1);
      }
    }
  }

}


/* TODO: free all entries in here too! */
void
retval_cache_free() {
  if (retval_cache_arrays == NULL)
    return;

  if (cache_size != 0) {
    fprintf(stderr, "WARNING: freeing return value cache even though it is not " \
        "empty!: %u\n", cache_size);
  }
  
  for (unsigned int i = 0; i < CACHED_FUNCTION_CALLS_NUM_ARRAYS; i++) {
    free(retval_cache_arrays[i]);  retval_cache_arrays[i] = NULL;
  }

  free(retval_cache_arrays);  retval_cache_arrays = NULL;
}


#ifdef UNIT_TESTS

unsigned int
is_cache_empty() {
  if ((cache_size == 0) && (start_index == 0) && \
      (last_used_array == 0) && (free_index == 0))
    return 1;
  else
    return 0;
}

unsigned int
get_cache_size() {
  return cache_size;
}

unsigned int
get_start_index() {
  return start_index;
}

unsigned int
get_last_used_array() {
  return last_used_array;
}

unsigned int
get_free_index() {
  return free_index;
}

void
set_max_cache_size(unsigned int _max_cache_size) {
  max_cache_size = _max_cache_size;
}
#endif

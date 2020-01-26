/*
 * Copyright (C) 2020  Joe Testa <jtesta@positronsecurity.com>
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "drltrace_retval_cache.h"

// TODO: find out about thread-safety!


#ifdef UNIT_TESTS
  #define RETVAL_CACHE_SIZE 8
  #define dr_fprintf fprintf
#else
  #include "drltrace_utils.h"

  /* There are 128K entries in the cache.  Each entry is 16 bytes, so the cache takes
   * up 2MB memory at minimum (not counting the function strings stored). */
  #define RETVAL_CACHE_SIZE (128 * 1024)

#endif

file_t out_stream;

bool grepable_output = false;

static cached_function_call *retval_cache = NULL;

// TODO: document this.
static unsigned int cache_size = 0;
static unsigned int max_cache_size = 0;
static unsigned int cache_dump_triggered = 0;


/* Outputs and clears as many entries with return values from the cache as possible
 * (which may be zero).  If 'clear_all' is set, then all entries are outputted &
 * cleared. */
void
retval_cache_output(unsigned int thread_id, bool clear_all) {

  /* If the caller wants to dump the cache, set this flag so that later return value
   * calls don't print error messages when the entry can't be found. */
  int i = 0;
  int first_slot = -1;
  if (clear_all)
    cache_dump_triggered = 1;
  else {

    /* Find the first index matching this thread ID. */
    for (; (i < cache_size); i++)
      if (retval_cache[i].thread_id == thread_id) {

	/* If the first matching index doesn't have its return value set, there's
         * nothing to do. */
	if (!retval_cache[i].retval_set)
	  return;
	else
	  break;
      }

    /* If no matching thread ID was found, there's nothing to do. */
    if (i == cache_size)
      return;
  }

  first_slot = i;
  for (; (i < cache_size); i++) {
    if ((thread_id == retval_cache[i].thread_id) || clear_all) {

      /* Print the function and return value. */
      dr_fprintf(out_stream, "%s", retval_cache[i].function_call);
      unsigned int retval_set = retval_cache[i].retval_set;
      void *retval = retval_cache[i].retval;

      if (grepable_output) {
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

      free(retval_cache[i].function_call);
    }
  }

  if (clear_all) {
    cache_size = 0;
  } else {
    int free_slot = first_slot;

    for (i = first_slot + 1; i < cache_size; i++) {
      if (thread_id != retval_cache[i].thread_id) {
	memcpy(&(retval_cache[free_slot]), &(retval_cache[i]), sizeof(cached_function_call));
	free_slot++;
      }
    }

    cache_size = free_slot;
  }

  return;
}


void
retval_cache_append(unsigned int thread_id, const char *module_and_function_name, size_t module_and_function_name_len, const char *function_call, size_t function_call_len) {
  /*dr_fprintf(outf, "%s", function_call);
    dr_fprintf(outf, "\n");*/

  /* If the cache size hits the user-defined limit, immediately dump it all as-is into
   * the log, then we'll continue. */
  if ((max_cache_size > 0) && (cache_size >= max_cache_size))
    retval_cache_dump_all();

  /* The post-function callback for these functions are never called.  So instead of
   * letting them clog up the cache, we'll just dump them out immediately. */
  if ((fast_strcmp(module_and_function_name, module_and_function_name_len, \
           "libc.so.6!__libc_start_main", 27) == 0) || \
      (fast_strcmp(module_and_function_name, module_and_function_name_len, \
           "libc.so.6!exit", 14) == 0)) {
    //dr_fprintf(outf, "\nNot caching [%s].\n\n", function_call);
    dr_fprintf(out_stream, "%s", function_call);

    if (grepable_output)
      dr_fprintf(out_stream, " = ??\n");
    else
      dr_fprintf(out_stream, "\n    ret: ??\n");

    return;
  }

  retval_cache[cache_size].thread_id = thread_id;
  retval_cache[cache_size].function_call = strdup(function_call);
  retval_cache[cache_size].function_call_len = function_call_len;
  retval_cache[cache_size].retval_set = 0;
  cache_size++;

  if (cache_size == RETVAL_CACHE_SIZE)
    retval_cache_dump_all();
}

void
retval_cache_set_return_value(unsigned int thread_id, const char *module_name_and_function, size_t module_name_and_function_len, void *retval) {
  bool found_entry = false;
  unsigned int min_len;
  int i = cache_size - 1;

  for (; (i >= 0) && (found_entry == false); i--) {
    if (retval_cache[i].retval_set || (retval_cache[i].thread_id != thread_id))
      continue;

    /* Only compare the shortest prefix of the two strings. */
    min_len = MIN(module_name_and_function_len, retval_cache[i].function_call_len);
    if (fast_strcmp(module_name_and_function, min_len, \
          retval_cache[i].function_call, min_len) == 0) {

      retval_cache[i].retval = retval;
      retval_cache[i].retval_set = 1;
      found_entry = true;
    }
  }

  if (found_entry)
    retval_cache_output(thread_id, false);
  else if (!found_entry && !cache_dump_triggered)
    dr_fprintf(out_stream, "ERROR: failed to find cache entry for %s (return value: 0x%" PRIx64 ")\n", module_name_and_function, retval);

  return;
}

void
retval_cache_init(file_t _out_stream, unsigned int _max_cache_size, \
                  bool _grepable_output) {

  out_stream = _out_stream;
  max_cache_size = _max_cache_size;
  grepable_output = _grepable_output;

  retval_cache = (cached_function_call *)calloc(RETVAL_CACHE_SIZE, \
      sizeof(cached_function_call));
  if (retval_cache == NULL) {
    fprintf(stderr, "Failed to create retval_cache array.\n");
    exit(-1);
  }
}

bool
retval_cache_free() {
  bool ret = true;

  if (retval_cache == NULL)
    return false;

  if (cache_size != 0) {
    ret = false;
    fprintf(stderr, "WARNING: freeing return value cache even though it is not " \
        "empty!: %u\n", cache_size);
  }

  free(retval_cache);  retval_cache = NULL;
  return ret;
}


#ifdef UNIT_TESTS

unsigned int
is_cache_empty() {
  if (cache_size == 0)
    return 1;
  else
    return 0;
}

unsigned int
get_cache_size() {
  return cache_size;
}

#endif

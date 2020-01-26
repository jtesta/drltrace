/*
 * Copyright (C) 2020  Joe Testa <jtesta@positronsecurity.com>
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "drltrace_retval_cache.h"

// TODO: find out about thread-safety!


#ifdef UNIT_TESTS
  #define MAX_NUM_THREADS 3
  #define RETVAL_CACHE_SIZE 8
  #define THREAD_CACHE_TIMEOUT 2
  #define dr_fprintf fprintf
#else
  #include "drltrace_utils.h"

  #define MAX_NUM_THREADS 128

  /* There are 128K entries in the cache.  Each entry is 16 bytes, so the cache takes
   * up 2MB memory at minimum (not counting the function strings stored). */
  #define RETVAL_CACHE_SIZE (128 * 1024)

  #define THREAD_CACHE_TIMEOUT 15
#endif

file_t out_stream;
bool grepable_output = false;
thread_cache_manager *tcm = NULL;
unsigned int tcm_num_threads = 0;
static unsigned int cache_size = 0;
static unsigned int max_cache_size = 0;


void
retval_cache_dump_all() {
  for(unsigned int i = 0; i < tcm_num_threads; i++)
    retval_cache_output(i, true);
}


/* Outputs and clears as many entries with return values from the cache as possible
 * (which may be zero).  If 'clear_all' is set, then all entries are outputted &
 * cleared. */
void
retval_cache_output(unsigned int thread_slot, bool clear_all) {
  cached_function_call *retval_cache = tcm[thread_slot].retval_cache;

  /* If the caller wants to dump the cache, set this flag so that later return value
   * calls don't print error messages when the entry can't be found. */
  if (clear_all)
    tcm[thread_slot].cache_dump_triggered = 1;
  else if (!retval_cache[0].retval_set)
    return;
  
  for (int i = 0; (i < tcm[thread_slot].free_index) && (i < RETVAL_CACHE_SIZE); i++) {

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
    cache_size--;
  }

  tcm[thread_slot].free_index = 0;
  tcm[thread_slot].last_cleared = time(NULL);
  return;
}


// TODO: rename "function_call" to "func_full_name_and_args"
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

  time_t now = time(NULL);
  int thread_slot = -1, old_slot = -1;
  unsigned int i = 0;
  for(; (i < tcm_num_threads) && (thread_slot == -1); i++) {
    if (tcm[i].retval_cache != NULL) {
      if (tcm[i].thread_id == thread_id)
	thread_slot = i;
      else if ((tcm[i].last_cleared != 0) && \
          ((now - tcm[i].last_cleared) > THREAD_CACHE_TIMEOUT))
	old_slot = i;
    }
  }

  cached_function_call *cfc;
  unsigned int free_index;
  if (thread_slot != -1) {

    free_index = tcm[thread_slot].free_index;
    if (free_index == RETVAL_CACHE_SIZE) {
      retval_cache_output(thread_slot, true);
      tcm[thread_slot].free_index = free_index = 0;
    }

    cfc = &(tcm[thread_slot].retval_cache[free_index]);
    tcm[thread_slot].free_index++;

  } else if (old_slot != -1) {

    cfc = &(tcm[old_slot].retval_cache[0]);
    if (tcm[old_slot].free_index != 0) {
      dr_fprintf(out_stream, "Error: free_index of old slot is not zero!\n");
      exit(-1);
    }

    tcm[old_slot].free_index = 1;
    tcm[old_slot].last_cleared = 0;
    tcm[old_slot].thread_id = thread_id;

  } else if (tcm_num_threads < MAX_NUM_THREADS) {

    tcm[tcm_num_threads].retval_cache = (cached_function_call *)calloc(RETVAL_CACHE_SIZE, sizeof(cached_function_call));
    if (tcm[tcm_num_threads].retval_cache == NULL) {
      dr_fprintf(out_stream, "Failed to create cached_function_call array.\n");
      exit(-1);
    }
    cfc = &(tcm[tcm_num_threads].retval_cache[0]);
    tcm[tcm_num_threads].free_index = 1;
    tcm[tcm_num_threads].thread_id = thread_id;

    tcm_num_threads++;
  } else {
    dr_fprintf(out_stream, "Number of threads for return value cache exhausted (%u). "
                           "Dumping entire cache...\n", MAX_NUM_THREADS);
    retval_cache_dump_all();

    /* Now that the entire cache has been dumped, we can take over the first slot. */
    cfc = &(tcm[0].retval_cache[0]);
    tcm[0].free_index = 1;
    tcm[0].thread_id = thread_id;
  }

  cfc->function_call = strdup(function_call);
  cfc->function_call_len = function_call_len;
  cfc->retval_set = 0;
  cache_size++;
}

void
retval_cache_set_return_value(unsigned int thread_id, const char *module_name_and_function, size_t module_name_and_function_len, void *retval) {

  int thread_slot = -1;
  int i = 0;
  for(; (i < tcm_num_threads) && (thread_slot == -1); i++) {
    if (tcm[i].retval_cache != NULL) {
      if (tcm[i].thread_id == thread_id)
	thread_slot = i;
    }
  }

  if (thread_slot == -1) {
    dr_fprintf(out_stream, "Error: return value given for thread ID that doesn't "
                           "exist!: %u\n", thread_id);
    return;
  }


  cached_function_call *retval_cache = tcm[thread_slot].retval_cache;
  unsigned int free_index = tcm[thread_slot].free_index;
  unsigned int found_entry = 0;
  unsigned int min_len;

  i = free_index - 1;
  for (; (i >= 0) && (found_entry == 0); i--) {
    if (retval_cache[i].retval_set)
      continue;

    /* Only compare the shortest prefix of the two strings. */
    min_len = MIN(module_name_and_function_len, retval_cache[i].function_call_len);
    if (fast_strcmp(module_name_and_function, min_len, \
          retval_cache[i].function_call, min_len) == 0) {

      retval_cache[i].retval = retval;
      retval_cache[i].retval_set = 1;
      found_entry = 1;
    }
  }

  /* If we just stored the return value of the first entry, then we can print out
   * return values. */
  if (found_entry && (i < 0)) {
    retval_cache_output(thread_slot, false);

  /* If we didn't find the entry, and a dump was never triggered, then this is an
   * error... */
  } else if (!found_entry && !tcm[thread_slot].cache_dump_triggered)
    dr_fprintf(out_stream, "ERROR: failed to find cache entry for %s (return value: 0x%" PRIx64 ")\n", module_name_and_function, retval);

  return;
}

void
retval_cache_init(file_t _out_stream, unsigned int _max_cache_size, \
                  bool _grepable_output) {

  out_stream = _out_stream;
  max_cache_size = _max_cache_size;
  grepable_output = _grepable_output;

  tcm = (thread_cache_manager *)calloc(MAX_NUM_THREADS, sizeof(thread_cache_manager));
  if (tcm == NULL) {
    fprintf(stderr, "Failed to create thread cache manager array.\n");
    exit(-1);
  }
}

bool
retval_cache_free() {
  bool ret = true;
  if (tcm == NULL)
    return false;

  if (cache_size != 0) {
    fprintf(stderr, "WARNING: freeing return value cache even though it is not " \
        "empty!: %u\n", cache_size);
    ret = false;
  }

  for(unsigned int i = 0; i < MAX_NUM_THREADS; i++) {
    cached_function_call *retval_cache = tcm[i].retval_cache;
    unsigned int free_index = tcm[i].free_index;
    if (retval_cache != NULL) {
      for(unsigned int j = 0; j < free_index; j++) {
	free(retval_cache[j].function_call); retval_cache[j].function_call = NULL;
      }

      free(retval_cache); tcm[i].retval_cache = NULL;
    }
  }

  free(tcm);  tcm = NULL;
  tcm_num_threads = 0;
  cache_size = 0;
  return ret;
}


/* Not used in production; for unit testing only... */
#ifdef UNIT_TESTS

bool
is_thread_cache_empty(unsigned int thread_id) {
  if (get_free_index(thread_id) == 0)
    return true;
  else
    return false;
}

bool
is_global_cache_empty() {
  bool ret = true;
  for (unsigned int i = 0; i < get_num_threads(); i++) {
    if (tcm[i].retval_cache != NULL) {
      ret &= (tcm[i].free_index == 0);
    }
  }

  ret &= (cache_size == 0);
  return ret;
}


unsigned int
get_cache_size() {
  return cache_size;
}

unsigned int
get_free_index(unsigned int thread_id) {
  return tcm[ resolve_thread_id_to_slot(thread_id) ].free_index;
}

unsigned int
get_num_threads() {
  return tcm_num_threads;
}

unsigned int
resolve_thread_id_to_slot(unsigned int thread_id) {
  int thread_slot = -1;
  for(unsigned int i = 0; (i < get_num_threads()) && (thread_slot == -1); i++) {
    if ((tcm[i].retval_cache != NULL) && (tcm[i].thread_id == thread_id))
      thread_slot = i;
  }

  if (thread_slot == -1) {
    dr_fprintf(out_stream, "ERROR: failed to resolve thread ID (%u) to slot.\n", \
               thread_id);
    exit(-1);
  } else
    return thread_slot;
}

#endif

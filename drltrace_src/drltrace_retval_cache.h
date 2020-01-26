#ifndef _DRLTRACE_RETVAL_CACHE_H
#define _DRLTRACE_RETVAL_CACHE_H

#ifdef UNIT_TESTS
  #include <stdio.h>
  #define file_t FILE*

  #define MIN(x,y) (((x) > (y)) ? (y) : (x))

  inline int fast_strcmp(const char *s1, size_t s1_len, const char *s2, size_t s2_len) {
  if (s1_len != s2_len)
    return -1;

  #ifdef WINDOWS
    return memcmp(s1, s2, s1_len); /* VC2013 doesn't have bcmp(), sadly. */
  #else
    return bcmp(s1, s2, s1_len);  /* Fastest option. */
  #endif
  }

#else
  #include "dr_api.h"
#endif


struct _cached_function_call {
  char *function_call; /* Contains the module name, function name, and arguments. */
  unsigned int function_call_len;  /* Length of the function_call string. */
  unsigned int retval_set;   /* Set to 1 when retval is set, otherwise 0. */
  void *retval;              /* The return value of the function. */
};
typedef struct _cached_function_call cached_function_call;

struct _thread_cache_manager {
  cached_function_call *retval_cache; /* If NULL, this record is not valid. */
  unsigned int free_index;
  unsigned int cache_dump_triggered;
  unsigned int thread_id;
  time_t last_cleared;
};
typedef struct _thread_cache_manager thread_cache_manager;


void
retval_cache_dump_all();

void
retval_cache_output(unsigned int thread_slot, bool clear_all);

void
retval_cache_append(unsigned int thread_id, const char *module_and_function_name, size_t module_and_function_name_len, const char *function_call, size_t function_call_len);

void
retval_cache_set_return_value(unsigned int thread_id, const char *function_call, size_t function_call_len, void *retval);

void
retval_cache_init(file_t _out_stream, unsigned int _max_cache_size, bool grepable_output);

bool
retval_cache_free();


/* Functions only used during unit testing of the retval cache system. */
#ifdef UNIT_TESTS
bool
is_thread_cache_empty(unsigned int thread_id);

bool
is_global_cache_empty();

unsigned int
get_cache_size();

unsigned int
get_free_index(unsigned int thread_id);

unsigned int
get_num_threads();
#endif

unsigned int
resolve_thread_id_to_slot(unsigned int thread_id);

#endif /* _DRLTRACE_RETVAL_CACHE_H */

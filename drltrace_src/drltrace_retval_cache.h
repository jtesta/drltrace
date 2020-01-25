#ifndef _DRLTRACE_RETVAL_CACHE_H
#define _DRLTRACE_RETVAL_CACHE_H

#ifndef UNIT_TESTS
  #include "dr_api.h"
#endif

struct _cached_function_call {
  char *function_call; /* Contains the module name, function name, and arguments. */
  unsigned int function_call_len;  /* Length of the function_call string. */
  unsigned int retval_set;   /* Set to 1 when retval is set, otherwise 0. */
  void *retval;              /* The return value of the function. */
};
typedef struct _cached_function_call cached_function_call;


void
retval_cache_output(unsigned int clear_all);

void
retval_cache_append(char *module_and_function_name, size_t module_and_function_name_len, char *function_call, size_t function_call_len);

void
retval_cache_set_return_value(char *function_call, size_t function_call_len, void *retval);

#ifdef UNIT_TESTS
void
retval_cache_init(FILE *_out_stream, unsigned int _max_cache_size, bool grepable_output);
#else
void
retval_cache_init(file_t _out_stream, unsigned int _max_cache_size, bool grepable_output);
#endif

void
retval_cache_free();


#ifdef UNIT_TESTS

unsigned int
is_cache_empty();
unsigned int
get_cache_size();
unsigned int
get_start_index();
unsigned int
get_last_used_array();
unsigned int
get_free_index();
void
set_max_cache_size();
#endif

#endif /* _DRLTRACE_RETVAL_CACHE_H */

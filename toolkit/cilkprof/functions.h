#ifndef INCLUDED_FUNCTIONS_H
#define INCLUDED_FUNCTIONS_H

#include <stdbool.h>
#include <inttypes.h>

typedef struct {
  bool on_stack;
  bool recursive;
  uintptr_t function;
} function_record_t;

typedef struct {
  int lg_capacity;
  int table_size;
  function_record_t records[0];
} function_table_t;

/**
 * Exposed call site hashtable methods
 */
function_table_t* function_table_create(void);
function_record_t*
get_function_record_const(uintptr_t function, function_table_t *tab);
int32_t add_to_function_table(function_table_t **tab,
                              uintptr_t function);

#endif

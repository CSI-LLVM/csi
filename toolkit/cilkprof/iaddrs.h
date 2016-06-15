#ifndef INCLUDED_IADDRS_H
#define INCLUDED_IADDRS_H

#ifndef IADDR_CACHE
#define IADDR_CACHE 0
#endif

#include <stdbool.h>
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"

typedef struct iaddr_record_t {
  /* bool on_stack; */
  /* bool recursive; */
  int32_t index;
  FunctionType_t func_type;
  uintptr_t iaddr;
  struct iaddr_record_t *next;
} iaddr_record_t;

#if IADDR_CACHE
typedef struct iaddr_cache_el_t {
  iaddr_record_t *record;
  struct iaddr_cache_el_t *next;
} iaddr_cache_el_t;
#endif

typedef struct {
#if IADDR_CACHE
  iaddr_cache_el_t* iaddr_cache;
#endif
  int lg_capacity;
  int table_size;
  iaddr_record_t* records[0];
} iaddr_table_t;

/**
 * Exposed iaddr hashtable methods
 */
iaddr_table_t* iaddr_table_create(void);
iaddr_record_t*
get_iaddr_record_const(uintptr_t iaddr, FunctionType_t func_type, iaddr_table_t *tab);
int32_t add_to_iaddr_table(iaddr_table_t **tab, uintptr_t iaddr, FunctionType_t func_type);
void iaddr_table_free(iaddr_table_t *tab);

#endif

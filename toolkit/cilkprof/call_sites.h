#ifndef INCLUDED_CALL_SITES_H
#define INCLUDED_CALL_SITES_H

#include <stdbool.h>
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef struct call_site_record_t {
  bool on_stack;
  bool recursive;
  uint32_t index;
  uintptr_t call_site;
  struct call_site_record_t *next;
} call_site_record_t;

typedef struct call_site_cache_el_t {
  call_site_record_t *record;
  struct call_site_cache_el_t *next;
} call_site_cache_el_t;

typedef struct {
  call_site_cache_el_t* call_site_cache;
  int lg_capacity;
  int table_size;
  call_site_record_t* records[0];
} call_site_table_t;

/**
 * Exposed call site hashtable methods
 */
call_site_table_t* call_site_table_create(void);
call_site_record_t*
get_call_site_record_const(uintptr_t call_site, call_site_table_t *tab);
int32_t add_to_call_site_table(call_site_table_t **tab,
                               uintptr_t call_site);

#endif

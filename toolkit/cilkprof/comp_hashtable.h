#ifndef INCLUDED_COMP_HASHTABLE_H
#define INCLUDED_COMP_HASHTABLE_H

#include <inttypes.h>
#include "span_hashtable.h"

typedef struct comp_hashtable_entry_t comp_hashtable_entry_t;
typedef struct comp_hashtable_t comp_hashtable_t;
  
/**
 * Forward declaration of nonstatic methods
 */
bool empty_entry_comp_hashtable_p(const comp_hashtable_entry_t *entry);
comp_hashtable_t* comp_hashtable_create(void);
void clear_comp_hashtable(comp_hashtable_t *tab);
void flush_comp_hashtable(comp_hashtable_t **tab);
bool add_to_work_comp_hashtable(comp_hashtable_t **tab,
                                uintptr_t function,
                                span_hashtable_t *on_wrk);
bool add_to_span_comp_hashtable(comp_hashtable_t **tab,
                                uintptr_t function,
                                span_hashtable_t *on_spn);
comp_hashtable_t* combine_comp_hashtables(comp_hashtable_t **left,
                                          comp_hashtable_t **right);

#endif

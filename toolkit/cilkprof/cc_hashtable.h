#ifndef INCLUDED_CC_HASHTABLE_H
#define INCLUDED_CC_HASHTABLE_H

#include <stdbool.h>
#include <inttypes.h>

#include "util.h"

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Function depth.  This is currently used to distinguish recursive
  // and non-recursive functions.
  /* int32_t depth; */

  /* // Flag denoting whether this call site ever calls itself */
  /* bool is_recursive; */


  // Store whether this entry is initialized
  uint32_t initialized;

#ifndef NDEBUG
  // Function type.  Least-significant bit indicates whether the
  // function is recursive.
  FunctionType_t func_type;
  // Return address that identifies call site
  uintptr_t rip;
#endif

  // Total number of invocations of rip.
  uint32_t local_count;
  // Local work associated with rip
  uint64_t local_wrk;
  // Local span associated with rip
  uint64_t local_spn;

  // Number of invocations of rip, excluding recursive instances.
  uint32_t count;
  // Number of top-level invocations of rip.
  uint32_t top_count;

  // Work associated with rip, excluding recursive instances
  uint64_t wrk;
  // Span associated with rip, excluding recursive instances
  uint64_t spn;

  // Work associated with top-level invocations of rip
  uint64_t top_wrk;
  // Span associated with top-level invocations of rip
  uint64_t top_spn;

} cc_hashtable_entry_t;

// Structure for making a linked list of cc_hashtable entries
typedef struct cc_hashtable_list_el_t {
  // Index in table for this entry
  uint32_t index;

  // Pointer to next entry in table
  struct cc_hashtable_list_el_t* next;

  // Hashtable entry data
  cc_hashtable_entry_t entry;

} cc_hashtable_list_el_t;

// Structure for the hashtable
typedef struct {
  // Lg of capacity of hash table
  int lg_capacity;

  // Number of elements in list
  int list_size;

  // Number of elements in table
  int table_size;

  // Linked list of entries to add to hashtable
  cc_hashtable_list_el_t *head;
  cc_hashtable_list_el_t *tail;

  // Array storing indices of entries[] that are nonzero
  int *populated;

  // Entries of the hash table
  cc_hashtable_entry_t entries[0];

} cc_hashtable_t;

extern cc_hashtable_list_el_t *ll_free_list;  

/**
 * Exposed hashtable methods
 */
bool empty_cc_entry_p(const cc_hashtable_entry_t *entry);
cc_hashtable_t* cc_hashtable_create(void);
void clear_cc_hashtable(cc_hashtable_t *tab);
void flush_cc_hashtable(cc_hashtable_t **tab);
bool add_to_cc_hashtable(cc_hashtable_t **tab,
                         /* InstanceType_t inst_type, */
                         bool is_top_fn,
                         uint32_t index,
#ifndef NDEBUG
                         uintptr_t rip,
#endif
                         uint64_t wrk, uint64_t spn,
                         uint64_t local_wrk, uint64_t local_spn);
bool add_local_to_cc_hashtable(cc_hashtable_t **tab,
                               uint32_t index,
#ifndef NDEBUG
                               uintptr_t rip,
#endif
                               uint64_t local_wrk, uint64_t local_spn);
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left,
				  cc_hashtable_t **right);
void free_cc_hashtable(cc_hashtable_t *tab);
bool cc_hashtable_is_empty(const cc_hashtable_t *tab);

#endif

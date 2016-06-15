#ifndef INCLUDED_STRAND_HASHTABLE_H
#define INCLUDED_STRAND_HASHTABLE_H

#include <stdbool.h>
#include <inttypes.h>

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Start of strand
  uintptr_t start;
  // End of strand
  uintptr_t end;

  // Work associated with strand
  uint64_t wrk;

  // Update counts associated with this strand
  uint32_t count;

} strand_hashtable_entry_t;

// Structure for making a linked list of strand_hashtable entries
typedef struct strand_hashtable_list_el_t {
  // Hashtable entry data
  strand_hashtable_entry_t entry;

  // Pointer to next entry in table
  struct strand_hashtable_list_el_t* next;

} strand_hashtable_list_el_t;

// Structure for the hashtable
typedef struct {
  // Lg of capacity of hash table
  int lg_capacity;

  // Number of elements in list
  int list_size;

  // Number of elements in table
  int table_size;

  // Linked list of entries to add to hashtable
  strand_hashtable_list_el_t *head;
  strand_hashtable_list_el_t *tail;

  // Entries of the hash table
  strand_hashtable_entry_t entries[0];

} strand_hashtable_t;
  

/**
 * Exposed hashtable methods
 */
bool empty_strand_entry_p(const strand_hashtable_entry_t *entry);
strand_hashtable_t* strand_hashtable_create(void);
void clear_strand_hashtable(strand_hashtable_t *tab);
void flush_strand_hashtable(strand_hashtable_t **tab);
strand_hashtable_entry_t*
get_strand_hashtable_entry_const(uintptr_t start, uintptr_t end,
                                 strand_hashtable_t *tab);
bool add_to_strand_hashtable(strand_hashtable_t **tab,
                             uintptr_t start, uintptr_t end,
                             uint64_t wrk);
strand_hashtable_t* add_strand_hashtables(strand_hashtable_t **left,
                                          strand_hashtable_t **right);
void free_strand_hashtable(strand_hashtable_t *tab);
bool strand_hashtable_is_empty(const strand_hashtable_t *tab);

#endif

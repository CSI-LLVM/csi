#include "strand_hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

#ifndef DEBUG_RESIZE
#define DEBUG_RESIZE 0
#endif

static int min(int a, int b) {
  return a < b ? a : b;
}

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
static const int START_LG_CAPACITY = 2;

// Threshold fraction of table size that can be in the linked list.
static const int LG_FRAC_SIZE_THRESHOLD = 0;

// Torlate entries being displaced by a constant amount
static const size_t MAX_DISPLACEMENT = 1024 / sizeof(strand_hashtable_entry_t);


// Use a standardish trick from hardware caching to hash strand
// start-end pairs.
static const uint64_t TAG_OFFSET = 2;
static const uint64_t PRIME = (uint32_t)(-5);
static const uint64_t ASEED = 0x8c678e6b;
static const uint64_t BSEED = 0x9c16f733;
static const int MIX_ROUNDS = 4;

static inline size_t hash(uintptr_t start, uintptr_t end, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  uint64_t key = (uint32_t)((start ^ end) >> TAG_OFFSET);
  uint32_t h = (uint32_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
bool empty_strand_entry_p(const strand_hashtable_entry_t *entry) {
  return (0 == entry->start);
}


// Create an empty hashtable entry
static inline void make_empty_strand_entry(strand_hashtable_entry_t *entry) {
  entry->start = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
static strand_hashtable_t* strand_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  strand_hashtable_t *table =
      (strand_hashtable_t*)malloc(sizeof(strand_hashtable_t)
                                  + (capacity * sizeof(strand_hashtable_entry_t)));

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_strand_entry(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
strand_hashtable_t* strand_hashtable_create(void) {
  return strand_hashtable_alloc(START_LG_CAPACITY);
}

static inline bool can_override_entry(const strand_hashtable_entry_t *entry,
                                      uintptr_t new_start, uintptr_t new_end) {
  return (entry->start == new_start) && (entry->end == new_end);
}

// Helper function to get the entry in tab corresponding to strand.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
strand_hashtable_entry_t*
get_strand_hashtable_entry_const(uintptr_t start, uintptr_t end,
                                 strand_hashtable_t *tab) {
  
  assert(((uintptr_t)NULL != start) && ((uintptr_t)NULL != end));

  strand_hashtable_entry_t *entry = &(tab->entries[hash(start, end, tab->lg_capacity)]);

  assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    /* fprintf(stderr, "get_strand_hashtable_entry_const(): disp = %d\n", disp); */

    if (empty_strand_entry_p(entry) || can_override_entry(entry, start, end)) {
      break;
    }
    ++entry;
    // Wrap around to the beginning
    if (&(tab->entries[1 << tab->lg_capacity]) == entry) {
      entry = &(tab->entries[0]);
    }
  }

  // Return false if the entry was not found in the target area.
  if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) {
    return NULL;
  }

  return entry;
}


// Return a hashtable with the contents of tab and more capacity.
static strand_hashtable_t* increase_table_capacity(const strand_hashtable_t *tab) {

  /* fprintf(stderr, "calling increase_table_capacity()\n"); */

  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  strand_hashtable_t *new_tab;

  do {
    new_tab = strand_hashtable_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const strand_hashtable_entry_t *old = &(tab->entries[i]);
      if (empty_strand_entry_p(old)) {
	continue;
      }

      strand_hashtable_entry_t *new
          = get_strand_hashtable_entry_const(old->start, old->end, new_tab);

      if (NULL == new) {
	++new_lg_capacity;
	free(new_tab);
	break;
      } else {
	assert(empty_strand_entry_p(new));
	*new = *old;
	++elements_added;
      }
    }

  } while (elements_added < tab->table_size);

  new_tab->table_size = tab->table_size;

  new_tab->list_size = tab->list_size;
  new_tab->head = tab->head;
  new_tab->tail = tab->tail;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static strand_hashtable_entry_t*
get_strand_hashtable_entry(uintptr_t start, uintptr_t end,
                           strand_hashtable_t **tab) {
  /* fprintf(stderr, "get_strand_hashtable_entry"); */
#if DEBUG_RESIZE
  int old_table_cap = 1 << (*tab)->lg_capacity;
#endif

  strand_hashtable_entry_t *entry;
  while (NULL == (entry = get_strand_hashtable_entry_const(start, end, *tab))) {

    strand_hashtable_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free(*tab);
    *tab = new_tab;
  }
#if DEBUG_RESIZE
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_strand_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
#endif
  return entry;
}


static void flush_strand_hashtable_list(strand_hashtable_t **tab) {
  /* fprintf(stderr, "flush_strand_hashtable_list\n"); */

  // Flush list into table
  strand_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    strand_hashtable_entry_t *entry = &(lst_entry->entry);

    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    strand_hashtable_entry_t *tab_entry;

    tab_entry = get_strand_hashtable_entry(entry->start, entry->end, tab);
    assert(NULL != tab_entry);
    assert(empty_strand_entry_p(tab_entry) ||
           can_override_entry(entry, tab_entry->start, tab_entry->end));

    if (empty_strand_entry_p(tab_entry)) {
      // the compiler will do a struct copy
      *tab_entry = *entry;
      ++(*tab)->table_size;
    } else {  // same start and end
      tab_entry->wrk += entry->wrk;
      tab_entry->count += 1;
    }

    entries_added++;
    strand_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_strand_hashtable(strand_hashtable_t **tab) {
  if (NULL != (*tab)->head)
    flush_strand_hashtable_list(tab);
  else
    assert((*tab)->list_size == 0);
}

// Add the given strand_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_strand_hashtable(strand_hashtable_t **tab,
                             uintptr_t start, uintptr_t end,
                             uint64_t wrk) {
  /* fprintf(stderr, "add_to_strand_hashtable\n"); */

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    strand_hashtable_list_el_t *lst_entry
        = (strand_hashtable_list_el_t*)malloc(sizeof(strand_hashtable_list_el_t));

    lst_entry->entry.start = start;
    lst_entry->entry.end = end;
    lst_entry->entry.wrk = wrk;
    lst_entry->entry.count = 1;
    lst_entry->next = NULL;

    if (NULL == (*tab)->tail) {
      (*tab)->tail = lst_entry;
      assert(NULL == (*tab)->head);
      (*tab)->head = lst_entry;
    } else {
      (*tab)->tail->next = lst_entry;
      (*tab)->tail = lst_entry;
    }

    // Increment list size
    ++(*tab)->list_size;

  } else {
    
    if ((*tab)->list_size > 0) {
      /* fprintf(stderr, "calling flush_strand_hashtable_list()\n"); */
      assert((*tab)->head != NULL);
      flush_strand_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    strand_hashtable_entry_t *entry = get_strand_hashtable_entry(start, end, tab);
    assert(empty_strand_entry_p(entry) || can_override_entry(entry, start, end));

    if (NULL == entry) {
      return false;
    }
  
    if (empty_strand_entry_p(entry)) {
      entry->start = start;
      entry->end = end;
      entry->wrk = wrk;
      entry->count = 1;
      ++(*tab)->table_size;
    } else {  // same start and end
      entry->wrk += wrk;
      entry->count += 1;
    }
  }

  return true;
}

// Add the strand_hashtable **right into the strand_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
strand_hashtable_t* add_strand_hashtables(strand_hashtable_t **left, strand_hashtable_t **right) {

  /* fprintf(stderr, "add_strand_hashtables(%p, %p)\n", left, right); */

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    strand_hashtable_t *tmp = *left;
    *left = *right;
    *right = tmp;
  }

  /* fprintf(stderr, "\tleft list_size = %d, right list_size = %d\n", */
  /* 	  (*left)->list_size, (*right)->list_size); */

  if (NULL != (*left)->tail) {
    (*left)->tail->next = (*right)->head;
  } else {
    assert(NULL == (*left)->head);
    (*left)->head = (*right)->head;
    // XXX: Why not just do this?  Does it matter if both are NULL?
    /* (*left)->tail = (*right)->tail; */
  }
  (*left)->list_size += (*right)->list_size;
  if (NULL != (*right)->tail) {
    (*left)->tail = (*right)->tail;
  }
  (*right)->head = NULL;
  (*right)->tail = NULL;

  /* fprintf(stderr, "list_size = %d, table_size = %d, lg_capacity = %d\n", */
  /* 	  (*left)->list_size, (*left)->table_size, (*left)->lg_capacity); */

  if ((*left)->list_size > 0 &&
      (*left)->list_size + (*left)->table_size
      >= (1 << ((*left)->lg_capacity - LG_FRAC_SIZE_THRESHOLD))) {
    /* fprintf(stderr, "add_strand_hashtables: flush_strand_hashtable_list(%p)\n", left); */
    flush_strand_hashtable_list(left);
  }

  strand_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_strand_entry_p(r_entry)) {

      l_entry = get_strand_hashtable_entry(r_entry->start, r_entry->end, left);
      assert (NULL != l_entry);
      assert(empty_strand_entry_p(l_entry)
             || can_override_entry(l_entry, r_entry->start, r_entry->end));

      if (empty_strand_entry_p(l_entry)) {
        // let the compiler do the struct copy
        *l_entry = *r_entry;
	++(*left)->table_size;
      } else {  // same start and end
	l_entry->wrk += r_entry->wrk;
	l_entry->count += 1;
      }
    }
  }

  return *left;
}

// Clear all entries in tab.
void clear_strand_hashtable(strand_hashtable_t *tab) {
  // Clear the linked list
  strand_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    strand_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }
  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_strand_entry(&(tab->entries[i]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_strand_hashtable(strand_hashtable_t *tab) {
  // Clear the linked list
  strand_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    strand_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  free(tab);
}

bool strand_hashtable_is_empty(const strand_hashtable_t *tab) {
  return tab->table_size == 0 && tab->list_size == 0;
}

#include "cc_hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^2 entries.
/* static */ const int START_CC_LG_CAPACITY = 2;

// Threshold fraction of table size that can be in the linked list.
/* static */ const int TABLE_CONSTANT = 4;

/* int MIN_LG_CAPACITY = START_CC_LG_CAPACITY; */
int MIN_CAPACITY = 1;

cc_hashtable_list_el_t *ll_free_list = NULL;

// Return true if this entry is empty, false otherwise.
bool empty_cc_entry_p(const cc_hashtable_entry_t *entry) {
  return (0 == entry->initialized);
}


// Create an empty hashtable entry
static void make_empty_cc_entry(cc_hashtable_entry_t *entry) {
  entry->initialized = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
static cc_hashtable_t* cc_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_CC_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  cc_hashtable_t *table =
    (cc_hashtable_t*)malloc(sizeof(cc_hashtable_t)
			    + (capacity * sizeof(cc_hashtable_entry_t)));

  int *populated_entries = (int*)malloc(sizeof(int) * capacity);

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;
  table->populated = populated_entries;

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
cc_hashtable_t* cc_hashtable_create(void) {
  cc_hashtable_t *tab = cc_hashtable_alloc(START_CC_LG_CAPACITY);
  for (size_t i = 0; i < (1 << START_CC_LG_CAPACITY); ++i) {
    make_empty_cc_entry(&(tab->entries[i]));
  }
  return tab;
}

#ifndef NDEBUG
static inline
int can_override_entry(cc_hashtable_entry_t *entry, uintptr_t new_rip) {
  // used to be this:
  // entry->rip == new_rip && entry->height == new_height
  return (entry->rip == new_rip);
}
#endif

static inline
void combine_entries(cc_hashtable_entry_t *entry,
                     const cc_hashtable_entry_t *entry_add) {
  entry->local_wrk += entry_add->local_wrk;
  entry->local_spn += entry_add->local_spn;
  entry->local_count += entry_add->local_count;
  entry->wrk += entry_add->wrk;
  entry->spn += entry_add->spn;
  entry->count += entry_add->count;
  entry->top_wrk += entry_add->top_wrk;
  entry->top_spn += entry_add->top_spn;
  entry->top_count += entry_add->top_count;
}


// Return a hashtable with the contents of tab and more capacity.
static cc_hashtable_t* increase_cc_table_capacity(const cc_hashtable_t *tab) {

  int new_lg_capacity;
  if ((1 << tab->lg_capacity) < MIN_CAPACITY) {
    uint32_t x = MIN_CAPACITY - 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    new_lg_capacity = __builtin_ctz(x + 1);
  } else {
#ifndef NDEBUG
    fprintf(stderr, "this should not be reachable\n");
#endif
    new_lg_capacity = tab->lg_capacity + 1;
  }
  cc_hashtable_t *new_tab;

  /* fprintf(stderr, "resizing table\n"); */

  new_tab = cc_hashtable_alloc(new_lg_capacity);
  size_t i = 0;

  for (i = 0; i < (1 << tab->lg_capacity); ++i) {
    new_tab->entries[i] = tab->entries[i];
  }

  for ( ; i < (1 << new_tab->lg_capacity); ++i) {
    make_empty_cc_entry(&(new_tab->entries[i]));
  }

  for (i = 0; i < tab->table_size; ++i) {
    new_tab->populated[i] = tab->populated[i];
  }

  new_tab->table_size = tab->table_size;

  new_tab->list_size = tab->list_size;
  new_tab->head = tab->head;
  new_tab->tail = tab->tail;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
/* static */ __attribute__((always_inline)) cc_hashtable_entry_t*
get_cc_hashtable_entry_at_index(uint32_t index, cc_hashtable_t **tab) {
  if (index >= (1 << (*tab)->lg_capacity)) {
    cc_hashtable_t *new_tab = increase_cc_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free((*tab)->populated);
    free(*tab);
    *tab = new_tab;
  }

  return &((*tab)->entries[index]);
}


static inline
void flush_cc_hashtable_list(cc_hashtable_t **tab) {

  // Flush list into table
  cc_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    cc_hashtable_entry_t *entry = &(lst_entry->entry);

    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    cc_hashtable_entry_t *tab_entry;

    /* tab_entry = get_cc_hashtable_entry(entry->rip, tab); */
    tab_entry = get_cc_hashtable_entry_at_index(lst_entry->index, tab);
    assert(NULL != tab_entry);
    assert(empty_cc_entry_p(tab_entry) || can_override_entry(tab_entry, entry->rip));

    if (empty_cc_entry_p(tab_entry)) {
      // the compiler will do a struct copy
      *tab_entry = *entry;
      tab_entry->initialized = 1;
      /* (*tab)->populated[(*tab)->table_size] = cc_index(entry->rip); */
      (*tab)->populated[(*tab)->table_size] = lst_entry->index;
      ++(*tab)->table_size;
    } else {
      combine_entries(tab_entry, entry);
    }

    entries_added++;
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    // free(lst_entry);
    lst_entry = next_lst_entry;
  }

  if (NULL != (*tab)->head) {
    assert(NULL != (*tab)->tail);
    (*tab)->tail->next = ll_free_list;
    ll_free_list = (*tab)->head;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_cc_hashtable(cc_hashtable_t **tab) {
  if (NULL != (*tab)->head)
    flush_cc_hashtable_list(tab);
  else
    assert((*tab)->list_size == 0);
}


// Add the given cc_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
__attribute__((always_inline))
bool add_to_cc_hashtable(cc_hashtable_t **tab,
                         /* int32_t depth, bool is_top_level, */
                         /* InstanceType_t inst_type, */
                         bool is_top_fn,
                         uint32_t index,
#ifndef NDEBUG
                         uintptr_t rip,
#endif
                         uint64_t wrk, uint64_t spn,
                         uint64_t local_wrk, uint64_t local_spn) {
  
  if (((0 == (*tab)->table_size) || (index >= (1 << (*tab)->lg_capacity))) &&
       /* (1 << (*tab)->lg_capacity) < MIN_CAPACITY && */
      ((*tab)->list_size < MIN_CAPACITY * TABLE_CONSTANT)) {
    // If table does not reflect enough updates or new entry cannot be
    // placed in existing table and we're not ready to resize the
    // table, add entry to linked list.
    cc_hashtable_list_el_t *lst_entry;
    if (NULL != ll_free_list) {
      lst_entry = ll_free_list;
      ll_free_list = ll_free_list->next;
    } else {
      lst_entry = (cc_hashtable_list_el_t*)malloc(sizeof(cc_hashtable_list_el_t));
    }

    lst_entry->index = index;

    /* lst_entry->entry.is_recursive = (0 != (RECURSIVE & inst_type)); */
#ifndef NDEBUG
    lst_entry->entry.rip = rip;
#endif
    lst_entry->entry.wrk = wrk;
    lst_entry->entry.spn = spn;
    lst_entry->entry.count = 1; /* (0 != (RECORD & inst_type)); */
    /* if (TOP & inst_type) { */
    if (is_top_fn) {
      lst_entry->entry.top_wrk = wrk;
      lst_entry->entry.top_spn = spn;
      assert(0 != wrk);
      lst_entry->entry.top_count = 1;
    } else {
      lst_entry->entry.top_wrk = 0;
      lst_entry->entry.top_spn = 0;
      lst_entry->entry.top_count = 0;
    }      
    lst_entry->entry.local_wrk = local_wrk;
    lst_entry->entry.local_spn = local_spn;
    lst_entry->entry.local_count = 1;
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
      assert((*tab)->head != NULL);
      flush_cc_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    /* cc_hashtable_entry_t *entry = get_cc_hashtable_entry(rip, tab); */
    cc_hashtable_entry_t *entry = get_cc_hashtable_entry_at_index(index, tab);
    assert(NULL != entry);
    assert(empty_cc_entry_p(entry) || can_override_entry(entry, rip));
  
    if (empty_cc_entry_p(entry)) {
      /* entry->is_recursive = (0 != (RECURSIVE & inst_type)); */
#ifndef NDEBUG
      entry->rip = rip;
#endif
      entry->initialized = 1;
      entry->wrk = wrk;
      entry->spn = spn;
      entry->count = 1; /* (0 != (RECORD & inst_type)); */
      /* if (TOP & inst_type) { */
      if (is_top_fn) {
        entry->top_wrk = wrk;
        entry->top_spn = spn;
        entry->top_count = 1;
      } else {
        entry->top_wrk = 0;
        entry->top_spn = 0;
        entry->top_count = 0;
      }
      entry->local_wrk = local_wrk;
      entry->local_spn = local_spn;
      entry->local_count = 1;
      /* (*tab)->populated[ (*tab)->table_size ] = cc_index(rip); */
      (*tab)->populated[ (*tab)->table_size ] = index;
      ++(*tab)->table_size;
    } else {
      entry->local_wrk += local_wrk;
      entry->local_spn += local_spn;
      entry->local_count += 1;
      entry->wrk += wrk;
      entry->spn += spn;
      entry->count += 1; /* (0 != (RECORD & inst_type)); */
      /* if (TOP & inst_type) { */
      if (is_top_fn) {
        entry->top_wrk += wrk;
        entry->top_spn += spn;
        entry->top_count += 1;
      }
    }
  }

  return true;
}

// Add the given cc_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
__attribute__((always_inline))
bool add_local_to_cc_hashtable(cc_hashtable_t **tab,
                               uint32_t index,
#ifndef NDEBUG
                               uintptr_t rip,
#endif
                               uint64_t local_wrk, uint64_t local_spn) {

  if (((0 == (*tab)->table_size) || (index >= (1 << (*tab)->lg_capacity))) &&
      /* (1 << (*tab)->lg_capacity) < MIN_CAPACITY && */
      ((*tab)->list_size < MIN_CAPACITY * TABLE_CONSTANT)) {
    // If table does not reflect enough updates or new entry cannot be
    // placed in existing table and we're not ready to resize the
    // table, add entry to linked list.
    cc_hashtable_list_el_t *lst_entry;
    if (NULL != ll_free_list) {
      lst_entry = ll_free_list;
      ll_free_list = ll_free_list->next;
    } else {
      lst_entry = (cc_hashtable_list_el_t*)malloc(sizeof(cc_hashtable_list_el_t));
    }

    lst_entry->index = index;

#ifndef NDEBUG
    lst_entry->entry.rip = rip;
#endif
    lst_entry->entry.wrk = 0;
    lst_entry->entry.spn = 0;
    lst_entry->entry.count = 0;
    lst_entry->entry.top_wrk = 0;
    lst_entry->entry.top_spn = 0;
    lst_entry->entry.top_count = 0;
    lst_entry->entry.local_wrk = local_wrk;
    lst_entry->entry.local_spn = local_spn;
    lst_entry->entry.local_count = 1;
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
      assert((*tab)->head != NULL);
      flush_cc_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    /* cc_hashtable_entry_t *entry = get_cc_hashtable_entry(rip, tab); */
    cc_hashtable_entry_t *entry = get_cc_hashtable_entry_at_index(index, tab);
    assert(NULL != entry);
    assert(empty_cc_entry_p(entry) || can_override_entry(entry, rip));
  
    if (empty_cc_entry_p(entry)) {
#ifndef NDEBUG
      entry->rip = rip;
#endif
      entry->initialized = 1;
      entry->wrk = 0;
      entry->spn = 0;
      entry->count = 0;
      entry->top_wrk = 0;
      entry->top_spn = 0;
      entry->top_count = 0;
      entry->local_wrk = local_wrk;
      entry->local_spn = local_spn;
      entry->local_count = 1;
      (*tab)->populated[ (*tab)->table_size ] = index;
      ++(*tab)->table_size;
    } else {
      assert(rip == entry->rip);
      entry->local_wrk += local_wrk;
      entry->local_spn += local_spn;
      entry->local_count += 1;
    }
  }

  return true;
}

// Add the cc_hashtable **right into the cc_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
__attribute__((always_inline))
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left, cc_hashtable_t **right) {

  /* fprintf(stderr, "add_cc_hashtables(%p, %p)\n", left, right); */

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    cc_hashtable_t *tmp = *left;
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
  (*right)->list_size = 0;

  /* fprintf(stderr, "list_size = %d, table_size = %d, lg_capacity = %d\n", */
  /* 	  (*left)->list_size, (*left)->table_size, (*left)->lg_capacity); */

  if ((*left)->list_size >= MIN_CAPACITY * TABLE_CONSTANT) {
    flush_cc_hashtable_list(left);
  }

  cc_hashtable_entry_t *l_entry, *r_entry;
  for (size_t i = 0; i < (*right)->table_size; ++i) {

    r_entry = &((*right)->entries[ (*right)->populated[i] ]);
    assert(!empty_cc_entry_p(r_entry));

    l_entry = &((*left)->entries[ (*right)->populated[i] ]);
    assert(NULL != l_entry);
    assert(empty_cc_entry_p(l_entry) || can_override_entry(l_entry, r_entry->rip));

    if (empty_cc_entry_p(l_entry)) {
      // let the compiler do the struct copy
      *l_entry = *r_entry;
      (*left)->populated[ (*left)->table_size ] = (*right)->populated[i];
      ++(*left)->table_size;
    } else {
      combine_entries(l_entry, r_entry);
    }

  }

  return *left;
}

// Clear all entries in tab.
void clear_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  /* cc_hashtable_list_el_t *lst_entry = tab->head; */
  /* while (NULL != lst_entry) { */
  /*   cc_hashtable_list_el_t *next_lst_entry = lst_entry->next; */
  /*   free(lst_entry); */
  /*   lst_entry = next_lst_entry; */
  /* } */
  if (NULL != tab->head) {
    assert(NULL != tab->tail);
    tab->tail->next = ll_free_list;
    ll_free_list = tab->head;
  }

  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < tab->table_size; ++i) {
    make_empty_cc_entry(&(tab->entries[ tab->populated[i] ]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  /* cc_hashtable_list_el_t *lst_entry = tab->head; */
  /* while (NULL != lst_entry) { */
  /*   cc_hashtable_list_el_t *next_lst_entry = lst_entry->next; */
  /*   free(lst_entry); */
  /*   lst_entry = next_lst_entry; */
  /* } */
  if (NULL != tab->head) {
    assert(NULL != tab->tail);
    tab->tail->next = ll_free_list;
    ll_free_list = tab->head;
  }
  free(tab->populated);
  free(tab);
}

bool cc_hashtable_is_empty(const cc_hashtable_t *tab) {
  return tab->table_size == 0 && tab->list_size == 0;
}

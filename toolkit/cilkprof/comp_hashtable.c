#include "comp_hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

/**
 * Data structures
 */

// Structure for a comp_hashtable entry
typedef struct {
  // Function to consider
  uintptr_t function;

  // Table storing (call-site, (work, sapn)) data for call sites on
  // the work of function.
  span_hashtable_t *on_wrk;

  // Table storing (call-site, (work, sapn)) data for call sites on
  // the span of function.
  span_hashtable_t *on_spn;

} comp_hashtable_entry_t;

// Structure for making a linked list of comp_hashtable entries
typedef struct comp_hashtable_list_el_t {
  // Hashtable entry data
  comp_hashtable_entry_t entry;

  // Pointer to next entry in table
  struct comp_hashtable_list_el_t* next;

} comp_hashtable_list_el_t;

// Structure for the hashtable
typedef struct {
  // Lg of capacity of hash table
  int lg_capacity;

  // Number of elements in list
  int list_size;

  // Number of elements in table
  int table_size;

  // Linked list of entries to add to hashtable
  comp_hashtable_list_el_t *head;
  comp_hashtable_list_el_t *tail;

  // Entries of the hash table
  comp_hashtable_entry_t entries[0];

} comp_hashtable_t;

/*************************************************************************/

static int min(int a, int b) {
  return a < b ? a : b;
}

/**
 * Method implementations
 */
// Starting capacity of the hash table.
const int START_LG_CAPACITY = 2;

// Lg threshold fraction of table size that can be in the linked list.
const int LG_FRAC_SIZE_THRESHOLD = 0;

// Torlate entries being displaced by a constant amount
const size_t MAX_DISPLACEMENT = 1024 / sizeof(comp_hashtable_entry_t);


// Hash to compute hadh-table index for a given function
const uint64_t TAG_OFFSET = 2;
const uint64_t PRIME = (uint32_t)(-5);
const uint64_t ASEED = 0x8c678e6b;
const uint64_t BSEED = 0x9c16f733;
const int MIX_ROUNDS = 4;

static size_t hash(uintptr_t function, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;

  uint64_t key = (uint32_t)(function >> TAG_OFFSET);
  uint32_t h = (uint32_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }

  /* fprintf(stderr, "hash = %lu, (1 << lg_capacity) = %d\n", */
  /* 	  h & mask, 1 << lg_capacity); */
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
bool empty_entry_comp_hashtable_p(const comp_hashtable_entry_t *entry) {
  return (0 == entry->function);
}


// Create an empty hashtable entry
static void make_empty_entry_comp_hashtable(comp_hashtable_entry_t *entry) {
  entry->function = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
static comp_hashtable_t* comp_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  comp_hashtable_t *table =
      (comp_hashtable_t*)malloc(sizeof(comp_hashtable_t)
                                + (capacity * sizeof(comp_hashtable_entry_t)));

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_entry_comp_hashtable(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
comp_hashtable_t* comp_hashtable_create(void) {
  return comp_hashtable_alloc(START_LG_CAPACITY);
}


// Helper function to get the entry in tab corresponding to function.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
static comp_hashtable_entry_t*
get_comp_hashtable_entry_targeted(uintptr_t function, const comp_hashtable_t *tab) {
  
  assert((uintptr_t)NULL != function);

  comp_hashtable_entry_t *entry = &(tab->entries[hash(height,
                                                      function,
                                                      tab->lg_capacity)]);

  assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    /* fprintf(stderr, "get_comp_hashtable_entry_targeted(): disp = %d\n", disp); */

    if (empty_entry_comp_hashtable_p(entry) ||
	(entry->function == function)) {
      break;
    }
    ++entry;
    // Wrap around to the beginning
    if (&(tab->entries[1 << tab->lg_capacity]) == entry) {
      entry = &(tab->entries[0]);
    }
  }

  // Return false if the entry was not found in the target area.
  if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)) <= disp) {
    return NULL;
  }

  return entry;
}


// Return a hashtable with the contents of tab and more capacity.
static comp_hashtable_t* increase_table_capacity(const comp_hashtable_t *tab) {

  /* fprintf(stderr, "calling increase_table_capacity()\n"); */

  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  comp_hashtable_t *new_tab;

  // Attempt to insert each element in the old table into a new,
  // larger table.  
  do {
    new_tab = comp_hashtable_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const comp_hashtable_entry_t *old = &(tab->entries[i]);
      if (empty_entry_comp_hashtable_p(old)) {
	continue;
      }

      comp_hashtable_entry_t *new
          = get_comp_hashtable_entry_targeted(old->function, new_tab);

      if (NULL == new) {
        // I don't think this path should ever be taken
	++new_lg_capacity;
	free(new_tab);
	break;
      } else {
	assert(empty_entry_comp_hashtable_p(new));
      
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
static comp_hashtable_entry_t*
get_comp_hashtable_entry(uintptr_t function, comp_hashtable_t **tab) {
  int old_table_cap = 1 << (*tab)->lg_capacity;

  comp_hashtable_entry_t *entry;
  while (NULL == (entry = get_comp_hashtable_entry_targeted(function, *tab))) {

    comp_hashtable_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free(*tab);
    *tab = new_tab;
  }
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_comp_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
  return entry;
}


static void flush_comp_hashtable_list(comp_hashtable_t **tab) {
  // Flush list into table
  comp_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    comp_hashtable_entry_t *entry = &(lst_entry->entry);

    /* fprintf(stderr, "inserting entry %lx:%d into table\n", entry->function, entry->height); */
    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    comp_hashtable_entry_t *tab_entry;

    tab_entry = get_comp_hashtable_entry(entry->function, tab);

    assert(NULL != tab_entry);

    if (empty_entry_comp_hashtable_p(tab_entry)) {
      tab_entry->function = entry->function;
      tab_entry->on_wrk = entry->on_wrk;
      tab_entry->on_spn = entry->on_spn;
      ++(*tab)->table_size;
    } else {
      if (NULL != entry->on_wrk) {
        combine_span_hashtables_safe(&(tab_entry->on_wrk), entry->on_wrk);
      }
      if (NULL != entry->on_spn) {
        combine_span_hashtables_safe(&(tab_entry->on_spn), entry->on_spn);
      }
    }

    entries_added++;
    comp_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_comp_hashtable(comp_hashtable_t **tab) {
  if (NULL != (*tab)->head)
    flush_comp_hashtable_list(tab);
  else
    assert((*tab)->list_size == 0);
}

// Helper method to perform a deep copy of a span_hashtable.  This
// should not get called more than a constant number of times per
// span_hashtable, and it should only get called on span_hashtable's
// not in the comp_hashtable.
static span_hashtable_t* copy_span_hashtable(const span_hashtable_t *tab) {
  // Allocate a new table
  span_hashtable_t *new_tab = span_hashtable_create(tab->lg_capacity);
  // Copy table entries
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    new_tab[i] = tab[i];
  }
  // Copy table linked list
  span_hashtable_list_el_t *lst_entry = tab->head;
  span_hashtable_list_el_t *new_entry, *last_new_entry;
  if (NULL == lst_entry) {
    new_tab->head = NULL;
    new_tab->tail = NULL;
  } else {
    new_entry = (span_hashtable_list_el_t*)malloc(sizeof(span_hashtable_list_el_t));
    *new_entry = *lst_entry;
    new_tab->head = new_entry;
    last_new_entry = new_entry;
    lst_entry = lst_entry->next;
    while (NULL != lst_entry) {
      new_entry = (span_hashtable_list_el_t*)malloc(sizeof(span_hashtable_list_el_t));
      *new_entry = *lst_entry;
      last_new_entry->next = new_entry;
      last_new_entry = new_entry;
      lst_entry = lst_entry->next;
    }
    new_tab->tail = last_new_entry;
  }
  // Copy metadata
  new_tab->list_size = tab->list_size;
  new_tab->table_size = tab->table_size;
  return new_tab;
}

// Add the given comp_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_work_comp_hashtable(comp_hashtable_t **tab,
                                uintptr_t function,
                                const span_hashtable_t *on_wrk) {

  span_hashtable_t *my_on_wrk = copy_span_hashtable(on_wrk);

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    comp_hashtable_list_el_t *lst_entry =
        (comp_hashtable_list_el_t*)malloc(sizeof(comp_hashtable_list_el_t));

    lst_entry->entry.function = function;
    lst_entry->entry.on_wrk = my_on_wrk;
    lst_entry->entry.on_spn = NULL;
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
      /* fprintf(stderr, "calling flush_comp_hashtable_list()\n"); */
      assert((*tab)->head != NULL);
      flush_comp_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    comp_hashtable_entry_t *entry = get_comp_hashtable_entry(function, tab);

    if (NULL == entry) {
      return false;
    }
  
    if (empty_entry_comp_hashtable_p(entry)) {
      entry->function = function;
      entry->on_wrk = my_on_wrk;
      entry->on_spn = NULL;
      ++(*tab)->table_size;
    } else {
      combine_span_hashtables(&(entry->on_wrk), &(my_on_wrk));
      free(my_on_wrk);
    }
  }

  return true;
}

// Add the given comp_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_span_comp_hashtable(comp_hashtable_t **tab,
                                uintptr_t function,
                                const span_hashtable_t *on_spn) {

  span_hashtable_t *my_on_spn = copy_span_hashtable(on_spn);

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    comp_hashtable_list_el_t *lst_entry =
        (comp_hashtable_list_el_t*)malloc(sizeof(comp_hashtable_list_el_t));

    lst_entry->entry.function = function;
    lst_entry->entry.on_spn = on_spn;
    lst_entry->entry.on_wrk = NULL;
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
      /* fprintf(stderr, "calling flush_comp_hashtable_list()\n"); */
      assert((*tab)->head != NULL);
      flush_comp_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    comp_hashtable_entry_t *entry = get_comp_hashtable_entry(function, tab);

    if (NULL == entry) {
      return false;
    }
  
    if (empty_entry_comp_hashtable_p(entry)) {
      entry->function = function;
      entry->on_spn = my_on_spn;
      entry->on_wrk = NULL;
      ++(*tab)->table_size;
    } else {
      combine_span_hashtables(&(entry->on_spn), &(my_on_spn));
      free(my_on_spn);
    }
  }

  return true;
}

// Add the comp_hashtable **right into the comp_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
comp_hashtable_t* combine_comp_hashtables(comp_hashtable_t **left,
                                          comp_hashtable_t **right) {

  /* fprintf(stderr, "add_comp_hashtables(%p, %p)\n", left, right); */

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    comp_hashtable_t *tmp = *left;
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
    /* fprintf(stderr, "add_comp_hashtables: flush_comp_hashtable_list(%p)\n", left); */
    flush_comp_hashtable_list(left);
  }

  comp_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_entry_comp_hashtable_p(r_entry)) {

      l_entry = get_comp_hashtable_entry(r_entry->height, r_entry->function, left);

      assert (NULL != l_entry);

      if (empty_entry_comp_hashtable_p(l_entry)) {
	l_entry->function = r_entry->function;
	l_entry->on_wrk = r_entry->on_wrk;
	l_entry->on_spn = r_entry->on_spn;
	++(*left)->table_size;
      } else {
        if (NULL != entry->on_wrk) {
          combine_span_hashtables_safe(&(l_entry->on_wrk), r_entry->on_wrk);
        }
        if (NULL != entry->on_spn) {
          combine_span_hashtables_safe(&(l_entry->on_spn), r_entry->on_spn);
        }
      }
    }
  }

  return *left;
}

// Clear all entries in tab.
void clear_comp_hashtable(comp_hashtable_t *tab) {
  // Clear the linked list
  comp_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    comp_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }
  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_entry_comp_hashtable(&(tab->entries[i]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_comp_hashtable(comp_hashtable_t *tab) {
  // Clear the linked list
  comp_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    comp_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  free(tab);
}

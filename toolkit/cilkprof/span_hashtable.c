#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "span_hashtable.h"

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Return address that identifies call site
  uintptr_t call_site;

  // Function height
  int32_t height;

  // Work associated with call_site
  uint64_t wrk;

  // Span associated with call_site
  uint64_t spn;

} span_hashtable_entry_t;

// Structure for making a linked list of span_hashtable entries
typedef struct span_hashtable_list_el_t {
  // Hashtable entry data
  span_hashtable_entry_t entry;

  // Pointer to next entry in table
  struct span_hashtable_list_el_t* next;

} span_hashtable_list_el_t;

// Structure for the hashtable
typedef struct {
  int ref_count;

  // Lg of capacity of hash table
  int lg_capacity;

  // Number of elements in list
  int list_size;

  // Number of elements in table
  int table_size;

  // Linked list of entries to add to hashtable
  span_hashtable_list_el_t *head;
  span_hashtable_list_el_t *tail;

  // Entries of the hash table
  span_hashtable_entry_t entries[0];

} span_hashtable_t;


/*************************************************************************/

static int min(int a, int b) {
  return a < b ? a : b;
}

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^4 entries.
const int START_LG_CAPACITY = 2;

// Threshold fraction of table size that can be in the linked list.
const int LG_FRAC_SIZE_THRESHOLD = 0;

// Torlate entries being displaced by a constant amount
const size_t MAX_DISPLACEMENT = 1024 / sizeof(span_hashtable_entry_t);


// Use a standardish trick from hardware caching to hash call_site-height
// pairs.
const uint64_t TAG_OFFSET = 2;
const uint64_t PRIME = (uint32_t)(-5);
const uint64_t ASEED = 0x8c678e6b;
const uint64_t BSEED = 0x9c16f733;
const int MIX_ROUNDS = 4;

static size_t hash(int32_t height, uintptr_t call_site, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  /* uint64_t key = (call_site >> TAG_OFFSET) * (height + 1); */
  /* return (size_t)(key & mask); */

  uint64_t key = (uint32_t)(call_site >> TAG_OFFSET) + height;
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
bool empty_entry_span_hashtable_p(const span_hashtable_entry_t *entry) {
  return (0 == entry->call_site);
}


// Create an empty hashtable entry
static void make_empty_entry_span_hashtable(span_hashtable_entry_t *entry) {
  entry->call_site = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
span_hashtable_t* span_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  span_hashtable_t *table =
      (span_hashtable_t*)malloc(sizeof(span_hashtable_t)
                                + (capacity * sizeof(span_hashtable_entry_t)));

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_entry_span_hashtable(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
span_hashtable_t* span_hashtable_create(void) {
  return span_hashtable_alloc(START_LG_CAPACITY);
}


// Helper function to get the entry in tab corresponding to (call_site,
// height).  Returns a pointer to the entry if it can find a place to
// store it, NULL otherwise.
static span_hashtable_entry_t*
get_span_hashtable_entry_targeted(int32_t height, uintptr_t call_site, span_hashtable_t *tab) {
  
  assert((uintptr_t)NULL != call_site);

  span_hashtable_entry_t *entry = &(tab->entries[hash(height,
                                                      call_site,
                                                      tab->lg_capacity)]);

  assert(entry >= tab->entries && entry < tab->entries + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    /* fprintf(stderr, "get_span_hashtable_entry_targeted(): disp = %d\n", disp); */

    if (empty_entry_span_hashtable_p(entry) ||
	(entry->call_site == call_site &&
	 entry->height == height)) {
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
static span_hashtable_t* increase_table_capacity(const span_hashtable_t *tab) {

  /* fprintf(stderr, "calling increase_table_capacity()\n"); */

  int new_lg_capacity = tab->lg_capacity + 1;
  int elements_added;
  span_hashtable_t *new_tab;

  do {
    new_tab = span_hashtable_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const span_hashtable_entry_t *old = &(tab->entries[i]);
      if (empty_entry_span_hashtable_p(old)) {
	continue;
      }

      span_hashtable_entry_t *new
          = get_span_hashtable_entry_targeted(old->height, old->call_site, new_tab);

      if (NULL == new) {
	++new_lg_capacity;
	free(new_tab);
	break;
      } else {
	assert(empty_entry_span_hashtable_p(new));
      
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
static span_hashtable_entry_t*
get_span_hashtable_entry(int32_t height, uintptr_t call_site, span_hashtable_t **tab) {
  int old_table_cap = 1 << (*tab)->lg_capacity;

  span_hashtable_entry_t *entry;
  while (NULL == (entry = get_span_hashtable_entry_targeted(height, call_site, *tab))) {

    span_hashtable_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);
    assert(new_tab->head == (*tab)->head);
    assert(new_tab->tail == (*tab)->tail);
    (*tab)->head = NULL;
    (*tab)->tail = NULL;

    free(*tab);
    *tab = new_tab;
  }
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_span_hashtable_entry: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
  return entry;
}


static void flush_span_hashtable_list(span_hashtable_t **tab) {
  // Flush list into table
  span_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    span_hashtable_entry_t *entry = &(lst_entry->entry);

    /* fprintf(stderr, "inserting entry %lx:%d into table\n", entry->call_site, entry->height); */
    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    span_hashtable_entry_t *tab_entry;

    tab_entry = get_span_hashtable_entry(entry->height, entry->call_site, tab);

    assert(NULL != tab_entry);

    if (empty_entry_span_hashtable_p(tab_entry)) {
      tab_entry->height = entry->height;
      tab_entry->call_site = entry->call_site;
      tab_entry->wrk = entry->wrk;
      tab_entry->spn = entry->spn;
      ++(*tab)->table_size;
    } else {
      tab_entry->wrk += entry->wrk;
      tab_entry->spn += entry->spn;
    }

    entries_added++;
    span_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

static void flush_span_hashtable_list_safe(span_hashtable_t **tab) {
  // Flush list into table
  span_hashtable_list_el_t *lst_entry = (*tab)->head;
  int entries_added = 0;

  while (NULL != lst_entry) {
    span_hashtable_entry_t *entry = &(lst_entry->entry);

    /* fprintf(stderr, "inserting entry %lx:%d into table\n", entry->call_site, entry->height); */
    if (lst_entry == (*tab)->tail) {
      assert(entries_added == (*tab)->list_size - 1);
      assert(lst_entry->next == NULL);
    }

    span_hashtable_entry_t *tab_entry;

    tab_entry = get_span_hashtable_entry(entry->height, entry->call_site, tab);

    assert(NULL != tab_entry);

    if (empty_entry_span_hashtable_p(tab_entry)) {
      tab_entry->height = entry->height;
      tab_entry->call_site = entry->call_site;
      tab_entry->wrk = entry->wrk;
      tab_entry->spn = entry->spn;
      ++(*tab)->table_size;
    } else {
      tab_entry->wrk += entry->wrk;
      tab_entry->spn += entry->spn;
    }

    entries_added++;
    span_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    /* free(lst_entry); */
    lst_entry = next_lst_entry;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_span_hashtable(span_hashtable_t **tab) {
  if (NULL != (*tab)->head)
    flush_span_hashtable_list(tab);
  else
    assert((*tab)->list_size == 0);
}

// Add the given span_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_span_hashtable(span_hashtable_t **tab,
                           int32_t height, uintptr_t call_site,
                           uint64_t wrk, uint64_t spn) {

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    span_hashtable_list_el_t *lst_entry =
        (span_hashtable_list_el_t*)malloc(sizeof(span_hashtable_list_el_t));

    lst_entry->entry.height = height;
    lst_entry->entry.call_site = call_site;
    lst_entry->entry.wrk = wrk;
    lst_entry->entry.spn = spn;
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
      /* fprintf(stderr, "calling flush_span_hashtable_list()\n"); */
      assert((*tab)->head != NULL);
      flush_span_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    span_hashtable_entry_t *entry = get_span_hashtable_entry(height, call_site, tab);

    if (NULL == entry) {
      return false;
    }
  
    if (empty_entry_span_hashtable_p(entry)) {
      entry->height = height;
      entry->call_site = call_site;
      entry->wrk = wrk;
      entry->spn = spn;
      ++(*tab)->table_size;
    } else {
      entry->wrk += wrk;
      entry->spn += spn;
    }
  }

  return true;
}

// Add the span_hashtable **right into the span_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
span_hashtable_t* combine_span_hashtables(span_hashtable_t **left,
                                          span_hashtable_t **right) {

  /* fprintf(stderr, "combine_span_hashtables(%p, %p)\n", left,
   * right); */

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    span_hashtable_t *tmp = *left;
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
    /* fprintf(stderr, "add_span_hashtables: flush_span_hashtable_list(%p)\n", left); */
    flush_span_hashtable_list(left);
  }

  span_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_entry_span_hashtable_p(r_entry)) {

      l_entry = get_span_hashtable_entry(r_entry->height, r_entry->call_site, left);

      assert (NULL != l_entry);

      if (empty_entry_span_hashtable_p(l_entry)) {
	l_entry->call_site = r_entry->call_site;
	l_entry->height = r_entry->height;
	l_entry->wrk = r_entry->wrk;
	l_entry->spn = r_entry->spn;
	++(*left)->table_size;
      } else {
	l_entry->wrk += r_entry->wrk;
	l_entry->spn += r_entry->spn;
      }
    }
  }

  return *left;
}

// Add the span_hashtable *right into the span_hashtable **left.  The
// result will appear in **left.  Unlike combine_span_hashtables(),
// this method will not modify the *right span_hashtable.
span_hashtable_t* combine_span_hashtables_safe(span_hashtable_t **left,
                                               const span_hashtable_t *right) {

  /* fprintf(stderr, "combine_span_hashtables_safe (%p, %p)\n", left,
   * right); */

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

  /* fprintf(stderr, "list_size = %d, table_size = %d, lg_capacity = %d\n", */
  /* 	  (*left)->list_size, (*left)->table_size, (*left)->lg_capacity); */

  if ((*left)->list_size > 0 &&
      (*left)->list_size + (*left)->table_size
      >= (1 << ((*left)->lg_capacity - LG_FRAC_SIZE_THRESHOLD))) {
    /* fprintf(stderr, "add_span_hashtables: flush_span_hashtable_list(%p)\n", left); */
    flush_span_hashtable_list_safe(left);
  }

  span_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_entry_span_hashtable_p(r_entry)) {

      l_entry = get_span_hashtable_entry(r_entry->height, r_entry->call_site, left);

      assert (NULL != l_entry);

      if (empty_entry_span_hashtable_p(l_entry)) {
	l_entry->call_site = r_entry->call_site;
	l_entry->height = r_entry->height;
	l_entry->wrk = r_entry->wrk;
	l_entry->spn = r_entry->spn;
	++(*left)->table_size;
      } else {
	l_entry->wrk += r_entry->wrk;
	l_entry->spn += r_entry->spn;
      }
    }
  }

  return *left;
}


// Clear all entries in tab.
void clear_span_hashtable(span_hashtable_t *tab) {
  // Clear the linked list
  span_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    span_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }
  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_entry_span_hashtable (&(tab->entries[i]));
  }
  tab->table_size = 0;
}

// Free a table.
void free_span_hashtable(span_hashtable_t *tab) {
  // Clear the linked list
  span_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    span_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  free(tab);
}

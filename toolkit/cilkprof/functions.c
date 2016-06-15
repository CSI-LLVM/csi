#include "functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// #include "SFMT-src-1.4.1/SFMT.h"

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

// Torlate entries being displaced by a constant amount
/* static const size_t MAX_DISPLACEMENT = 64 / sizeof(function_record_t); */
static const size_t MAX_DISPLACEMENT = 16;


// Use a standardish trick from hardware caching to hash rip-height
// pairs.
static const uint64_t TAG_OFFSET = 0;
static const uint64_t PRIME = (uint32_t)(-5);
static const uint64_t ASEED = 0x8c678e6b;
static const uint64_t BSEED = 0x9c16f733;
static const int MIX_ROUNDS = 4;

static size_t hash(uintptr_t function, int lg_capacity) {
  uint64_t mask = (1 << lg_capacity) - 1;
  uint64_t key = (uint32_t)(function >> TAG_OFFSET);
  uint64_t h = (uint32_t)((ASEED * key + BSEED) % PRIME);
  for (int i = 0; i < MIX_ROUNDS; ++i) {
    h = h * (2 * h + 1);
    h = (h << (sizeof(h) / 2)) | (h >> (sizeof(h) / 2));
  }
  return (size_t)(h & mask);
}


// Return true if this entry is empty, false otherwise.
static inline bool empty_record_p(const function_record_t *entry) {
  return ((uintptr_t)NULL == entry->function);
}


// Create an empty hashtable entry
static inline void make_empty_function_record(function_record_t *entry) {
  entry->function = (uintptr_t)NULL;
}


// Allocate an empty hash table with 2^lg_capacity entries
static function_table_t* function_table_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  function_table_t *table =
      (function_table_t*)malloc(sizeof(function_table_t)
                                + (capacity * sizeof(function_record_t)));

  table->lg_capacity = lg_capacity;
  table->table_size = 0;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_function_record(&(table->records[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
function_table_t* function_table_create(void) {
  return function_table_alloc(START_LG_CAPACITY);
}

// Helper function to get the entry in tab corresponding to function.
// Returns a pointer to the entry if it can find a place to store it,
// NULL otherwise.
function_record_t*
get_function_record_const(uintptr_t function, function_table_t *tab) {

  assert((uintptr_t)NULL != function);

  function_record_t *entry = &(tab->records[hash(function, tab->lg_capacity)]);

  assert(entry >= tab->records && entry < tab->records + (1 << tab->lg_capacity));

  int disp;
  for (disp = 0; disp < min(MAX_DISPLACEMENT, (1 << tab->lg_capacity)); ++disp) {
    if (empty_record_p(entry) || function == entry->function) {
      break;
    }
    ++entry;
    // Wrap around to the beginning
    if (&(tab->records[1 << tab->lg_capacity]) == entry) {
      entry = &(tab->records[0]);
    }
  }

  // Return false if the entry was not found in the target area.
  if (min(MAX_DISPLACEMENT, (1 << tab->lg_capacity))  <= disp) {
    return NULL;
  }

  return entry;
}


// Return a hashtable with the contents of tab and more capacity.
static function_table_t* increase_table_capacity(const function_table_t *tab) {

  int new_lg_capacity = tab->lg_capacity;
  int elements_added;
  function_table_t *new_tab;
  bool success;
  do {
    ++new_lg_capacity;
    success = true;
    new_tab = function_table_alloc(new_lg_capacity);
    elements_added = 0;

    for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
      const function_record_t *old = &(tab->records[i]);
      if (empty_record_p(old)) {
	continue;
      }

      function_record_t *new
	= get_function_record_const(old->function, new_tab);

      if (NULL == new) {
	free(new_tab);
        success = false;
	break;
      } else {
	assert(empty_record_p(new));
	*new = *old;
	++elements_added;
      }
    }

  } while (!success);
  /* } while (elements_added < tab->table_size); */

  if (elements_added != tab->table_size) {
    fprintf(stderr, "elements_added = %d, table_size = %d\n", elements_added, tab->table_size);
  }
  assert(elements_added == tab->table_size);
  new_tab->table_size = tab->table_size;

  return new_tab;
}


// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static function_record_t*
get_function_record(uintptr_t function, function_table_t **tab) {
#if DEBUG_RESIZE
  int old_table_cap = 1 << (*tab)->lg_capacity;
#endif

  function_record_t *entry;
  while (NULL == (entry = get_function_record_const(function, *tab))) {

    function_table_t *new_tab = increase_table_capacity(*tab);

    assert(new_tab);

    free(*tab);
    *tab = new_tab;
  }
#if DEBUG_RESIZE
  if (1 << (*tab)->lg_capacity > 2 * old_table_cap) {
    fprintf(stderr, "get_function_record: new table capacity %d\n",
    	    1 << (*tab)->lg_capacity);
  }
#endif
  return entry;
}

// Add the given function_record_t data to **tab.  Returns 1 if
// function was not previously on the stack, 0 if function is
// already in the stack, -1 on error.
int32_t add_to_function_table(function_table_t **tab,
                              uintptr_t function) {

  function_record_t *entry = get_function_record(function, tab);
  assert(empty_record_p(entry) || entry->function == function);

  if (NULL == entry) {
    return -1;
  }
  
  if (empty_record_p(entry)) {
    entry->function = function;
    entry->on_stack = true;
    entry->recursive = false;
    ++(*tab)->table_size;
    return 1;
  }
  if (entry->on_stack) {
    entry->recursive = true;
    return 0;
  }
  entry->on_stack = true;
  return 1;
}

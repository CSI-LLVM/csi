#ifndef INCLDUED_CC_HASHTABLE_H
#define INCLUDED_CC_HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

/**
 * Data structures
 */

// Structure for a hashtable entry
typedef struct {
  // Function height
  int32_t height;

  // Return address that identifies call site
  uintptr_t rip;

  // Work associated with rip
  uint64_t wrk;

  // Span associated with rip
  uint64_t spn;

} cc_hashtable_entry_t;

// Structure for making a linked list of cc_hashtable entries
typedef struct cc_hashtable_list_el_t {
  // Hashtable entry data
  cc_hashtable_entry_t entry;

  // Pointer to next entry in table
  struct cc_hashtable_list_el_t* next;

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

  // Entries of the hash table
  cc_hashtable_entry_t entries[0];

} cc_hashtable_t;
  

/**
 * Forward declaration of nonstatic methods
 */
bool empty_entry_p(const cc_hashtable_entry_t *entry);
cc_hashtable_t* cc_hashtable_create(void);
void clear_cc_hashtable(cc_hashtable_t *tab);
void flush_cc_hashtable(cc_hashtable_t **tab);
bool add_to_cc_hashtable(cc_hashtable_t **tab,
			 int32_t height, uintptr_t rip,
			 uint64_t wrk, uint64_t spn);
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left,
				  cc_hashtable_t **right);

/*************************************************************************/

/**
 * Method implementations
 */
// Starting capacity of the hash table is 2^8 entries.
const int START_LG_CAPACITY = 8;

// Threshold fraction of table size that can be in the linked list.
const int LG_FRAC_SIZE_THRESHOLD = 0;

// Torlate entries being displaced by ~4 cache lines.
const size_t MAX_DISPLACEMENT = 256 / sizeof(cc_hashtable_entry_t);

/*************************************************************************/

/**
 * Table for storing hashes
 */

typedef struct {
  // Function height
  int32_t height;

  // Return address that identifies call site
  uintptr_t rip;

  // Index into hashtable for this height and rip
  size_t index;

} cc_hash_entry_t;  


typedef struct {
  int lg_capacity;

  cc_hash_entry_t entries[0];

} cc_hash_t;

static cc_hash_t *index_tab = NULL;
static int num_indexes = 0;

const uint64_t TAG_OFFSET = 2;
static size_t get_key(int32_t height, uintptr_t rip) {
  uint64_t key = (rip >> TAG_OFFSET) * (height + 1);
  return (size_t)key;
}

// Allocate an index table
static cc_hash_t* index_tab_alloc(int lg_capacity) {
  size_t capacity = 1 << lg_capacity;
  cc_hash_t *new_index_tab = (cc_hash_t*)malloc(sizeof(cc_hash_t)
						+ (capacity * sizeof(cc_hash_entry_t)));
  new_index_tab->lg_capacity = lg_capacity;
  for (size_t i = 0; i < capacity; ++i) {
    new_index_tab->entries[i].height = 0;
    new_index_tab->entries[i].rip = (uintptr_t)NULL;
    new_index_tab->entries[i].index = 0;
  }
  return new_index_tab;
}

// Double the capacity of the index table
static void increase_index_tab_capacity(void) {
  cc_hash_t *new_index_tab = index_tab_alloc(index_tab->lg_capacity + 1);

  // Copy entries into new index_tab
  for (size_t i = 0; i < (1 << index_tab->lg_capacity); ++i) {
    cc_hash_entry_t *cc_hash = &(index_tab->entries[i]);
    if (0 == cc_hash->height && (uintptr_t)NULL == cc_hash->rip) {
      continue;
    }
    cc_hash_entry_t *new_cc_hash = 
      &(new_index_tab->entries[get_key(cc_hash->height, cc_hash->rip)
			       & ((1 << new_index_tab->lg_capacity) - 1)]);

    assert(0 == new_cc_hash->height && (uintptr_t)NULL == new_cc_hash->rip);

    new_cc_hash->height = cc_hash->height;
    new_cc_hash->rip = cc_hash->rip;
    new_cc_hash->index = cc_hash->index;
  }

  free(index_tab);
  index_tab = new_index_tab;
}

// Use a standardish trick from hardware caching to hash rip-height
// pairs.
const int MAX_CHAIN_LEN = 256 / sizeof(cc_hash_entry_t);
const int MAX_INDEX_TAB_RESIZE_ATTEMPTS = 2;
static size_t hash(int32_t height, uintptr_t rip) {
  uint64_t key = get_key(height, rip);
  uint64_t it_mask = (1 << index_tab->lg_capacity) - 1;

  /* return (size_t)(key & mask); */

  int attempt = 0;
  do {
    cc_hash_entry_t *cc_hash
      = &(index_tab->entries[(key & it_mask)]);
    // Use chaining to find the right entry in index_tab
    int chain_len = 0;
    while (chain_len < MAX_CHAIN_LEN) {

      if (height == cc_hash->height && rip == cc_hash->rip) {
	// We found the entry in the table
	return cc_hash->index;
      }

      if (0 == cc_hash->height && (uintptr_t)NULL == cc_hash->rip) {
	// We found an empty entry in the table
	cc_hash->height = height;
	cc_hash->rip = rip;
	cc_hash->index = ++num_indexes;

	return cc_hash->index;
      }

      ++cc_hash;
      // Wrap around to the start of index_tab
      if (&(index_tab->entries[1 << index_tab->lg_capacity]) == cc_hash) {
	cc_hash = &(index_tab->entries[0]);
      }

      ++chain_len;
    }
    
    // Need to resize index_tab
    increase_index_tab_capacity();
    ++attempt;

  } while (attempt <= MAX_INDEX_TAB_RESIZE_ATTEMPTS);

  // Fail
  fprintf(stderr, "Too many resize attempts on index_tab.\n");
  assert(0);
}

/*************************************************************************/

// Return true if this entry is empty, false otherwise.
bool empty_entry_p(const cc_hashtable_entry_t *entry) {
  return ((uintptr_t)NULL == entry->rip);
}


// Create an empty hashtable entry
static void make_empty_entry(cc_hashtable_entry_t *entry) {
  entry->rip = (uintptr_t)NULL;
  entry->height = 0;
  entry->wrk = 0;
  entry->spn = 0;
}


// Allocate an empty hash table with 2^lg_capacity entries
static cc_hashtable_t* cc_hashtable_alloc(int lg_capacity) {
  assert(lg_capacity >= START_LG_CAPACITY);
  size_t capacity = 1 << lg_capacity;
  cc_hashtable_t *table =
    (cc_hashtable_t*)malloc(sizeof(cc_hashtable_t)
			    + (capacity * sizeof(cc_hashtable_entry_t)));

  table->lg_capacity = lg_capacity;
  table->list_size = 0;
  table->table_size = 0;
  table->head = NULL;
  table->tail = NULL;

  for (size_t i = 0; i < capacity; ++i) {
    make_empty_entry(&(table->entries[i]));
  }

  return table;
}


// Create a new, empty hashtable.  Returns a pointer to the hashtable
// created.
cc_hashtable_t* cc_hashtable_create(void) {
  if (NULL == index_tab) {
    index_tab = index_tab_alloc(START_LG_CAPACITY);
  }
  return cc_hashtable_alloc(START_LG_CAPACITY);
}


// Return a hashtable with the contents of tab and more capacity.
static cc_hashtable_t* increase_table_capacity(const cc_hashtable_t *tab) {
  
  /* cc_hashtable_t *new_tab = cc_hashtable_alloc(tab->lg_capacity + 1); */
  // A hash table the size of index_tab should be large enough
  assert(index_tab->lg_capacity > tab->lg_capacity);
  cc_hashtable_t *new_tab = cc_hashtable_alloc(index_tab->lg_capacity);

  new_tab->list_size = tab->list_size;
  new_tab->table_size = tab->table_size;
  new_tab->head = tab->head;
  new_tab->tail = tab->tail;

  // Copy entries from old table into new
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    new_tab->entries[i] = tab->entries[i];
  }

  return new_tab;
}

// Add entry to tab, resizing tab if necessary.  Returns a pointer to
// the entry if it can find a place to store it, NULL otherwise.
static cc_hashtable_entry_t*
get_cc_hashtable_entry(int32_t height, uintptr_t rip, cc_hashtable_t **tab) {
  size_t entry_index = hash(height, rip);

  if ((1 << (*tab)->lg_capacity) <= entry_index) {
    cc_hashtable_t *new_tab = increase_table_capacity(*tab);
    free(*tab);
    *tab = new_tab;
  }

  return &((*tab)->entries[entry_index]);
}

/* // Helper function to get the entry in tab corresponding to rip. */
/* // Returns a pointer to the entry if it can find a place to store it, */
/* // NULL otherwise. */
/* static cc_hashtable_entry_t* */
/* get_cc_hashtable_entry_h(int32_t height, uintptr_t rip, cc_hashtable_t *tab) { */
  
/*   assert((uintptr_t)NULL != rip); */

/*   // Find entry via chaining */
/*   /\* fprintf(stderr, "get_cc_hashtable_entry_h(%d, %p, %p)\n", *\/ */
/*   /\* 	  height, rip, tab); *\/ */

/*   /\* fprintf(stderr, "\t hash(%d, %p, %d) = %lu\n", *\/ */
/*   /\* 	  height, rip, tab->lg_capacity, *\/ */
/*   /\* 	  hash(height, rip, tab->lg_capacity)); *\/ */

/*   cc_hashtable_entry_t *entry = &(tab->entries[hash(height, */
/* 						    rip, */
/* 						    tab->lg_capacity)]); */

/*   size_t disp; */
/*   for (disp = 0; disp < MAX_DISPLACEMENT; ++disp) { */
/*     if (empty_entry_p(entry) || */
/* 	(entry->rip == rip && */
/* 	 entry->height == height)) { */
/*       break; */
/*     } */

/*     // Wrap around to the beginning */
/*     if (&(tab->entries[1 << tab->lg_capacity]) == entry) { */
/*       entry = &(tab->entries[0]); */
/*     } */
/*   } */

/*   // Return false if the entry was not found in the target area. */
/*   if (MAX_DISPLACEMENT == disp) { */
/*     return NULL; */
/*   } */

/*   return entry; */
/* } */

/* const int MAX_GET_ENTRY_ATTEMPTS = 2; */
/* // Add entry to tab, resizing tab if necessary.  Returns a pointer to */
/* // the entry if it can find a place to store it, NULL otherwise. */
/* static cc_hashtable_entry_t* */
/* get_cc_hashtable_entry(int32_t height, uintptr_t rip, cc_hashtable_t **tab) { */
/*   int attempt = 1; */
/*   cc_hashtable_entry_t *entry; */
/*   while (NULL == (entry = get_cc_hashtable_entry_h(height, rip, *tab))) { */
/*     if (attempt >= MAX_GET_ENTRY_ATTEMPTS) { */
/*       return NULL; */
/*     } */
/*     ++attempt; */
/*     cc_hashtable_t *new_tab = increase_table_capacity(*tab); */

/*     /\* fprintf(stderr, "increased table lg_capacity to %d\n", new_tab->lg_capacity); *\/ */

/*     assert(new_tab); */
/*     free(*tab); */
/*     *tab = new_tab; */
/*   } */
/*   return entry; */
/* } */

static void flush_cc_hashtable_list(cc_hashtable_t **tab) {
  // Flush list into table
  cc_hashtable_list_el_t *lst_entry = (*tab)->head;

  while (NULL != lst_entry) {
    cc_hashtable_entry_t *entry = &(lst_entry->entry);

    cc_hashtable_entry_t *tab_entry
      = get_cc_hashtable_entry(entry->height,
			       entry->rip,
			       tab);
    assert(NULL != tab_entry);

    if (empty_entry_p(tab_entry)) {
      tab_entry->height = entry->height;
      tab_entry->rip = entry->rip;
      tab_entry->wrk = entry->wrk;
      tab_entry->spn = entry->spn;
      ++(*tab)->table_size;
    } else {
      tab_entry->wrk += entry->wrk;
      tab_entry->spn += entry->spn;
    }
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }

  (*tab)->head = NULL;
  (*tab)->tail = NULL;
  (*tab)->list_size = 0;
}

void flush_cc_hashtable(cc_hashtable_t **tab) {
  flush_cc_hashtable_list(tab);
}

// Add the given cc_hashtable_entry_t data to **tab.  Returns true if
// data was successfully added, false otherwise.
bool add_to_cc_hashtable(cc_hashtable_t **tab,
			 int32_t height, uintptr_t rip,
			 uint64_t wrk, uint64_t spn) {

  if ((*tab)->list_size + (*tab)->table_size
      < (1 << ((*tab)->lg_capacity - LG_FRAC_SIZE_THRESHOLD)) - 1) {

    // If the table_size + list_size is sufficiently small, add entry
    // to linked list.    
    cc_hashtable_list_el_t *lst_entry =
      (cc_hashtable_list_el_t*)malloc(sizeof(cc_hashtable_list_el_t));

    lst_entry->entry.height = height;
    lst_entry->entry.rip = rip;
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
      flush_cc_hashtable_list(tab);
    }

    // Otherwise, add it to the table directly
    cc_hashtable_entry_t *entry = get_cc_hashtable_entry(height, rip, tab);

    if (NULL == entry) {
      return false;
    }
  
    if (empty_entry_p(entry)) {
      entry->height = height;
      entry->rip = rip;
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

// Add the cc_hashtable **right into the cc_hashtable **left.  The
// result will appear in **left, and **right might be modified in the
// process.
cc_hashtable_t* add_cc_hashtables(cc_hashtable_t **left, cc_hashtable_t **right) {

  // Make sure that *left is at least as large as *right.
  if ((*right)->lg_capacity > (*left)->lg_capacity) {
    cc_hashtable_t *tmp = *left;
    *left = *right;
    *right = tmp;
  }

  if (NULL != (*left)->tail) {
    (*left)->tail->next = (*right)->head;
  } else {
    (*left)->head = (*right)->head;
    (*left)->tail = (*right)->tail;
  }
  (*left)->list_size += (*right)->list_size;
  (*right)->head = NULL;
  (*right)->tail = NULL;

  if ((*left)->list_size + (*left)->table_size
      >= (1 << ((*left)->lg_capacity - LG_FRAC_SIZE_THRESHOLD))) {
    flush_cc_hashtable_list(left);
  }

  cc_hashtable_entry_t *l_entry, *r_entry;

  for (size_t i = 0; i < (1 << (*right)->lg_capacity); ++i) {
    r_entry = &((*right)->entries[i]);
    if (!empty_entry_p(r_entry)) {

      /* fprintf(stderr, "\tgetting entry in big table; bt_lg_cap = %d\n", big_table->lg_capacity); */

      /* l_entry = get_cc_hashtable_entry(r_entry->height, r_entry->rip, left); */
      /* assert (NULL != l_entry); */

      l_entry = &((*left)->entries[i]);
      
      if (empty_entry_p(l_entry)) {
	l_entry->rip = r_entry->rip;
	l_entry->height = r_entry->height;
	l_entry->wrk = r_entry->wrk;
	l_entry->spn = r_entry->spn;
	++(*left)->table_size;
      } else {
	assert(l_entry->height == r_entry->height);
	assert(l_entry->rip == r_entry->rip);
	
	l_entry->wrk += r_entry->wrk;
	l_entry->spn += r_entry->spn;
      }
    }
  }

  return *left;
}

// Clear all entries in tab.
void clear_cc_hashtable(cc_hashtable_t *tab) {
  // Clear the linked list
  cc_hashtable_list_el_t *lst_entry = tab->head;
  while (NULL != lst_entry) {
    cc_hashtable_list_el_t *next_lst_entry = lst_entry->next;
    free(lst_entry);
    lst_entry = next_lst_entry;
  }
  tab->head = NULL;
  tab->tail = NULL;
  tab->list_size = 0;

  // Clear the table
  for (size_t i = 0; i < (1 << tab->lg_capacity); ++i) {
    make_empty_entry(&(tab->entries[i]));
  }
  tab->table_size = 0;
}

#endif

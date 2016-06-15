#ifndef INCLUDED_SPAN_HASHTABLE_H
#define INCLUDED_SPAN_HASHTABLE_H

typedef struct span_hashtable_entry_t span_hashtable_entry_t;
typedef struct span_hashtable_t span_hashtable_t;

/**
 * Forward declaration of nonstatic methods
 */
bool empty_entry_span_hashtable_p(const span_hashtable_entry_t *entry);
span_hashtable_t* span_hashtable_alloc(int lg_capacity);
span_hashtable_t* span_hashtable_create(void);
void clear_span_hashtable(span_hashtable_t *tab);
void flush_span_hashtable(span_hashtable_t **tab);
bool add_to_span_hashtable(span_hashtable_t **tab,
                           int32_t height, uintptr_t call_site,
                           uint64_t wrk, uint64_t spn);
span_hashtable_t* combine_span_hashtables(span_hashtable_t **left,
                                          span_hashtable_t **right);

#endif

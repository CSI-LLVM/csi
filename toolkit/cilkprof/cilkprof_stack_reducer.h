#ifndef INCLUDED_CILKPROF_STACK_REDUCER_H
#define INCLUDED_CILKPROF_STACK_REDUCER_H

#include <stdlib.h>
#include <assert.h>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cilk/reducer.h>

#include "iaddrs.h"
#include "cilkprof_stack.h"

// Worker-local structures:
//   wrk_table
//   call_site_table
//   function_table
//   c_stack
//   cs_status
//   fn_status
//   helper_sf_free_list
//   spawner_sf_free_list
// Remaining structures in reducer
typedef struct cilkprof_wls_t {
  cc_hashtable_t *wrk_table;
#if COMPUTE_STRAND_DATA
  strand_hashtable_t *strand_wrk_table;
#endif
  iaddr_table_t *call_site_table;
  iaddr_table_t *function_table;
  c_fn_frame_t *c_stack;
  cs_status_t *cs_status;
  fn_status_t *fn_status;
  cilkprof_stack_frame_t *helper_sf_free_list;
  cilkprof_stack_frame_t *spawner_sf_free_list;  
} cilkprof_wls_t;

void cilkprof_wls_init(cilkprof_wls_t *wls) {
  wls->wrk_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  wls->strand_wrk_table = strand_hashtable_create();
#endif
  wls->call_site_table = iaddr_table_create();
  wls->function_table = iaddr_table_create();
  wls->c_stack = (c_fn_frame_t*)malloc(sizeof(c_fn_frame_t) * START_C_STACK_SIZE);
  wls->cs_status = (cs_status_t*)malloc(sizeof(cs_status_t)
                                        * START_STATUS_VECTOR_SIZE);

  wls->fn_status = (fn_status_t*)malloc(sizeof(fn_status_t)
                                        * START_STATUS_VECTOR_SIZE);
  wls->helper_sf_free_list = NULL;
  wls->spawner_sf_free_list = NULL;
}


/* Identity method for cilkprof stack reducer */
void identity_cilkprof_stack(void *reducer, void *view)
{
  cilkprof_stack_init((cilkprof_stack_t*)view, SPAWNER);
}

/* Reduce method for cilkprof stack reducer */
void reduce_cilkprof_stack(void *reducer, void *l, void *r)
{
  /* fprintf(stderr, "reduce_cilkprof_stack()\n"); */

  cilkprof_stack_t *left = (cilkprof_stack_t*)l;
  cilkprof_stack_t *right = (cilkprof_stack_t*)r;

  assert(NULL == right->bot->parent);
  assert(SPAWNER == right->bot->func_type);
  assert(right->bot->func_type == left->bot->func_type);

  assert(!(left->in_user_code));
  assert(!(right->in_user_code));

  /* height is maintained as a max reducer */
  if (right->bot->height > left->bot->height) {
    left->bot->height = right->bot->height;
  }
  /* running_wrk is maintained as a sum reducer */
  left->bot->running_wrk += right->bot->running_wrk;

  /* fprintf(stderr, "\tleft work (%p) += right work (%p)\n", */
  /* 	  &(left->wrk_table), &(right->wrk_table)); */

  /* fprintf(stderr, */
  /* 	  "\tleft->list_size = %d, left->table_size = %d, left->lg_capacity = %d\n", */
  /* 	  left->wrk_table->list_size, */
  /* 	  left->wrk_table->table_size, */
  /* 	  left->wrk_table->lg_capacity); */
  /* fprintf(stderr, */
  /* 	  "\tright->list_size = %d, right->table_size = %d, right->lg_capacity = %d\n", */
  /* 	  right->wrk_table->list_size, */
  /* 	  right->wrk_table->table_size, */
  /* 	  right->wrk_table->lg_capacity); */

  /* add_cc_hashtables(&(left->wrk_table), &(right->wrk_table)); */
  /* clear_cc_hashtable(right->wrk_table); */

  if (left->bot->contin_spn + right->bot->prefix_spn + right->bot->lchild_spn
      > left->bot->lchild_spn) {

    left->bot->prefix_spn += left->bot->contin_spn + right->bot->prefix_spn;
    left->bot->lchild_spn = right->bot->lchild_spn;
    left->bot->contin_spn = right->bot->contin_spn;

    /* fprintf(stderr, "\tleft contin += right prefix\n"); */

    add_cc_hashtables(&(left->bot->contin_table), &(right->bot->prefix_table));
    clear_cc_hashtable(right->bot->prefix_table);

    /* fprintf(stderr, "\tleft prefix += left contin\n"); */

    add_cc_hashtables(&(left->bot->prefix_table), &(left->bot->contin_table));
    /* clear_cc_hashtable(left->bot->contin_table); */

    left->bot->lchild_table = right->bot->lchild_table;
    right->bot->lchild_table = NULL;
    left->bot->contin_table = right->bot->contin_table;
    right->bot->contin_table = NULL;
    
  } else {

    left->bot->contin_spn += right->bot->prefix_spn + right->bot->contin_spn;

    /* fprintf(stderr, "\tleft contin += right prefix\n"); */

    add_cc_hashtables(&(left->bot->contin_table), &(right->bot->prefix_table));
    clear_cc_hashtable(right->bot->prefix_table);

    /* fprintf(stderr, "\tleft contin += right contin\n"); */

    add_cc_hashtables(&(left->bot->contin_table), &(right->bot->contin_table));
    clear_cc_hashtable(right->bot->contin_table);

  }
}

/* Destructor for cilkprof stack reducer */
void destroy_cilkprof_stack(void *reducer, void *view)
{
  // Free component tables
  if (NULL != ((cilkprof_stack_t*)view)->wrk_table) {
    free(((cilkprof_stack_t*)view)->wrk_table);
  }
  if (NULL != ((cilkprof_stack_t*)view)->bot->prefix_table) {
    free(((cilkprof_stack_t*)view)->bot->prefix_table);
  }
  if (NULL != ((cilkprof_stack_t*)view)->bot->lchild_table) {
    free(((cilkprof_stack_t*)view)->bot->lchild_table);
  }
  if (NULL != ((cilkprof_stack_t*)view)->bot->contin_table) {
    free(((cilkprof_stack_t*)view)->bot->contin_table);
  }

  // Free the view
  free(((cilkprof_stack_t*)view)->bot);
}


#endif

#ifndef INCLUDED_CILKPROF_STACK_H
#define INCLUDED_CILKPROF_STACK_H

#include <stdbool.h>
#include <time.h>
#include <limits.h>

#ifndef COMPUTE_STRAND_DATA
#define COMPUTE_STRAND_DATA 0
#endif

// TB: Use this instead of strand_time.h to count strands.  Unlike
// time, this counts the number of strands encountered, which should
// be deterministic.
/* #include "strand_count.h" */
/* #include "strand_time.h" */
// #include "strand_time_rdtsc.h"
#include "strand_time.h"
#include "cc_hashtable.h"
#if COMPUTE_STRAND_DATA
#include "strand_hashtable.h"
#endif

// Used to size call site and function status vectors
const int START_STATUS_VECTOR_SIZE = 4;

const int START_C_STACK_SIZE = 8;
/* const int TOP_INDEX_FLAG = INT_MIN; */

typedef struct c_fn_frame_t {
  /* // We don't have that many different flags yet, */
  /* // so we can just use bools. */
  /* bool top_cs; */
  /* bool top_fn; */

  // Index for this call site
  int32_t cs_index;
  /* int fn_index; */

#ifndef NDEBUG
  // Return address of this function
  uintptr_t rip;
  // Address of this function
  uintptr_t function;
#endif

  // Work and span values to store for every function
  uint64_t local_wrk;
  /* uint64_t local_contin; */
  uint64_t running_wrk;
  uint64_t running_spn;

  /* // Parent of this C function on the same stack */
  /* struct c_fn_frame_t *parent; */
} c_fn_frame_t __attribute__((aligned(16)));

// Type for cilkprof stack frame
typedef struct cilkprof_stack_frame_t {
  /* // We don't have that many different flags yet, */
  /* // so we can just use bools. */
  /* bool top_cs; */
  /* bool top_fn; */

  // Function type
  FunctionType_t func_type;

  // Index of head C function in stack
  int32_t c_head;

  /* // Depth of the function */
  /* int32_t depth; */
  /* // Return address of this function */
  /* uintptr_t rip; */
  /* // Address of this function */
  /* uintptr_t function; */

  /* // Running work of this function and its child C functions */
  /* uint64_t running_wrk; */
  /* // Span of the continuation of the function since the spawn of its */
  /* // longest child */
  /* uint64_t contin_spn; */

  /* // Local work and span of this function and its child C functions.  These */
  /* // work and span values are maintained as a stack. */
  /* c_fn_frame_t *c_fn_frame; */

  /* // Local work of this function */
  /* uint64_t local_wrk; */

  // Local continuation span of this function
  uint64_t local_contin;
  // Local span of this function
  uint64_t local_spn;

  // Span of the prefix of this function and its child C functions
  uint64_t prefix_spn;

  // Span of the longest spawned child of this function observed so
  // far
  uint64_t lchild_spn;

  // The span of the continuation is stored in the running_spn + local_contin
  // in the topmost c_fn_frame

  // Data associated with the function's prefix
  cc_hashtable_t* prefix_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with prefix
  strand_hashtable_t* strand_prefix_table;
#endif

  // Data associated with the function's longest child
  cc_hashtable_t* lchild_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with longest child
  strand_hashtable_t* strand_lchild_table;
#endif

  // Data associated with the function's continuation
  cc_hashtable_t* contin_table;
#if COMPUTE_STRAND_DATA
  // Strand data associated with continuation
  strand_hashtable_t* strand_contin_table;
#endif

  // Pointer to the frame's parent
  struct cilkprof_stack_frame_t *parent;
} cilkprof_stack_frame_t;


// Metadata for a call site
typedef struct {
  /* uint32_t count_on_stack; */
  /* FunctionType_t func_type; */
  int32_t c_tail;
  int32_t fn_index;
  uint32_t flags;
} cs_status_t;

// Metadata for a function
typedef int32_t fn_status_t;

const uint32_t RECURSIVE = 1;
const int32_t OFF_STACK = INT32_MIN;
const int32_t UNINITIALIZED = INT32_MIN;

// Type for a cilkprof stack
typedef struct {
  // Flag to indicate whether user code is being executed.  This flag
  // is mostly used for debugging.
  bool in_user_code;

  // Capacity of call-site status vector
  int cs_status_capacity;

  // Capacity of function status vector
  int fn_status_capacity;

  // Capacity of C stack
  int c_stack_capacity;

  // Current bottom of C stack
  int32_t c_tail;

  // Tool for measuring the length of a strand
  strand_ruler_t strand_ruler;

  // Stack of C function frames
  c_fn_frame_t *c_stack;

  // Vector of status flags for different call sites
  cs_status_t *cs_status;
  // Vector of status flags for different functions
  fn_status_t *fn_status;

  // Pointer to bottom of the stack, onto which frames are pushed.
  cilkprof_stack_frame_t *bot;

  // Call-site data associated with the running work
  cc_hashtable_t* wrk_table;
#if COMPUTE_STRAND_DATA
  // Endpoints of currently executing strand
  uintptr_t strand_start;
  uintptr_t strand_end;

  // Strand data associated with running work
  strand_hashtable_t* strand_wrk_table;
#endif

  /* // Free list of C function frames */
  /* c_fn_frame_t *c_fn_free_list; */

  // Free list of cilkprof stack frames for spawn helpers.  
  cilkprof_stack_frame_t *helper_sf_free_list;

  // Free list of cilkprof stack frames for spawners
  cilkprof_stack_frame_t *spawner_sf_free_list;

} cilkprof_stack_t;


/*----------------------------------------------------------------------*/

// Resizes the C stack
__attribute__((always_inline))
void resize_c_stack(c_fn_frame_t **c_stack, int *c_stack_capacity) {
  int new_c_stack_capacity = 2 * (*c_stack_capacity);
  c_fn_frame_t *new_c_stack = (c_fn_frame_t*)malloc(sizeof(c_fn_frame_t)
                                                    * new_c_stack_capacity);
  for (int i = 0; i < *c_stack_capacity; ++i) {
    new_c_stack[i] = (*c_stack)[i];
  }

  free(*c_stack);
  *c_stack = new_c_stack;
  *c_stack_capacity = new_c_stack_capacity;
}

// Initializes C function frame *c_fn_frame
static inline
void cilkprof_c_fn_frame_init(c_fn_frame_t *c_fn_frame) {
  /* c_fn_frame->top_cs = false; */
  /* c_fn_frame->top_fn = false; */

  c_fn_frame->cs_index = 0;
  /* c_fn_frame->fn_index = 0; */

#ifndef NDEBUG
  c_fn_frame->rip = (uintptr_t)NULL;
  c_fn_frame->function = (uintptr_t)NULL;
#endif

  c_fn_frame->local_wrk = 0;
  /* c_fn_frame->local_contin = 0; */
  c_fn_frame->running_wrk = 0;
  c_fn_frame->running_spn = 0;

  /* c_fn_frame->parent = NULL; */
}

// Initializes the cilkprof stack frame *frame
static inline
void cilkprof_stack_frame_init(cilkprof_stack_frame_t *frame,
                               FunctionType_t func_type,
                               int c_head)
{
  frame->parent = NULL;

  frame->func_type = func_type;

  /* cilkprof_c_fn_frame_init(frame->c_fn_frame); */
  frame->c_head = c_head;

  /* frame->top_cs = false; */
  /* frame->top_fn = false; */

  /* frame->rip = 0;  // (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)); */
  /* /\* frame->depth = 0; *\/ */

  /* frame->local_wrk = 0; */
  frame->local_spn = 0;
  frame->local_contin = 0;

  /* frame->running_wrk = 0; */

  frame->prefix_spn = 0; 
  frame->lchild_spn = 0;
  /* frame->contin_spn = 0; */

  if (HELPER == func_type) {
    assert(cc_hashtable_is_empty(frame->prefix_table));
    /* clear_cc_hashtable(frame->prefix_table); */
    /* frame->prefix_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
    assert(strand_hashtable_is_empty(frame->strand_prefix_table));
    /* clear_strand_hashtable(frame->strand_prefix_table); */
    /* frame->strand_prefix_table = strand_hashtable_create(); */
#endif
  } else {
    assert(cc_hashtable_is_empty(frame->lchild_table));
    /* clear_cc_hashtable(frame->lchild_table); */
    /* frame->lchild_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
    assert(strand_hashtable_is_empty(frame->strand_lchild_table));
    /* clear_strand_hashtable(frame->strand_lchild_table); */
    /* frame->strand_lchild_table = strand_hashtable_create(); */
#endif
    assert(cc_hashtable_is_empty(frame->contin_table));
    /* clear_cc_hashtable(frame->contin_table); */
    /* frame->contin_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
    assert(strand_hashtable_is_empty(frame->strand_contin_table));
    /* clear_strand_hashtable(frame->strand_contin_table); */
    /* frame->strand_contin_table = strand_hashtable_create(); */
#endif
  }
}


// Push new frame of C function onto the C function stack starting at
// stack->bot->c_fn_frame.
c_fn_frame_t* cilkprof_c_fn_push(cilkprof_stack_t *stack)
{
  /* fprintf(stderr, "pushing C stack\n"); */
  assert(NULL != stack->bot);

  ++stack->c_tail;

  if (stack->c_tail >= stack->c_stack_capacity) {
    resize_c_stack(&(stack->c_stack), &(stack->c_stack_capacity));
  }

  cilkprof_c_fn_frame_init(&(stack->c_stack[stack->c_tail]));

  /* assert(NULL != stack->bot->c_fn_frame); */

  /* c_fn_frame_t *new_frame; */
  /* if (NULL != stack->c_fn_free_list) { */
  /*   new_frame = stack->c_fn_free_list; */
  /*   stack->c_fn_free_list = stack->c_fn_free_list->parent; */
  /* } else { */
  /*   new_frame = (c_fn_frame_t *)malloc(sizeof(c_fn_frame_t)); */
  /* } */

  /* cilkprof_c_fn_frame_init(new_frame); */
  /* new_frame->parent = stack->bot->c_fn_frame; */
  /* stack->bot->c_fn_frame = new_frame; */

  return &(stack->c_stack[stack->c_tail]);
}


// Push new frame of function type func_type onto the stack *stack
__attribute__((always_inline))
cilkprof_stack_frame_t*
cilkprof_stack_push(cilkprof_stack_t *stack, FunctionType_t func_type)
{
  cilkprof_stack_frame_t *new_frame;
  if (HELPER == func_type || NULL == stack->bot) {
    if (NULL != stack->helper_sf_free_list) {
      new_frame = stack->helper_sf_free_list;
      stack->helper_sf_free_list = stack->helper_sf_free_list->parent;
    } else {
      new_frame = (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));

      /* c_fn_frame_t *new_c_frame; */
      /* if (NULL != stack->c_fn_free_list) { */
      /*   new_c_frame = stack->c_fn_free_list; */
      /*   stack->c_fn_free_list = stack->c_fn_free_list->parent; */
      /* } else { */
      /*   new_c_frame = (c_fn_frame_t *)malloc(sizeof(c_fn_frame_t)); */
      /* } */
      /* new_frame->c_fn_frame = new_c_frame; */

      new_frame->prefix_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
      new_frame->strand_prefix_table = strand_hashtable_create();
#endif
      new_frame->lchild_table = NULL;
      /* new_frame->lchild_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
      new_frame->strand_lchild_table = NULL;
      /* new_frame->strand_lchild_table = strand_hashtable_create(); */
#endif
      new_frame->contin_table = NULL;
      /* new_frame->contin_table = cc_hashtable_create(); */
#if COMPUTE_STRAND_DATA
      new_frame->strand_contin_table = NULL;
      /* new_frame->strand_contin_table = strand_hashtable_create(); */
#endif
    }
  } else {
    if (NULL != stack->spawner_sf_free_list) {
      new_frame = stack->spawner_sf_free_list;
      stack->spawner_sf_free_list = stack->spawner_sf_free_list->parent;
    } else {
      new_frame = (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));

      /* c_fn_frame_t *new_c_frame; */
      /* if (NULL != stack->c_fn_free_list) { */
      /*   new_c_frame = stack->c_fn_free_list; */
      /*   stack->c_fn_free_list = stack->c_fn_free_list->parent; */
      /* } else { */
      /*   new_c_frame = (c_fn_frame_t *)malloc(sizeof(c_fn_frame_t)); */
      /* } */
      /* new_frame->c_fn_frame = new_c_frame; */

      new_frame->lchild_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
      new_frame->strand_lchild_table = strand_hashtable_create();
#endif
      new_frame->contin_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
      new_frame->strand_contin_table = strand_hashtable_create();
#endif
    }
    if (0 == stack->bot->lchild_spn) {
      new_frame->prefix_table = stack->bot->prefix_table;
#if COMPUTE_STRAND_DATA
      new_frame->strand_prefix_table = stack->bot->strand_prefix_table;
#endif
    } else {
      new_frame->prefix_table = stack->bot->contin_table;
#if COMPUTE_STRAND_DATA
      new_frame->strand_prefix_table = stack->bot->strand_contin_table;
#endif
    }
  }

  cilkprof_c_fn_push(stack);
  cilkprof_stack_frame_init(new_frame, func_type, stack->c_tail);
  new_frame->parent = stack->bot;
  stack->bot = new_frame;
  /* if (new_frame->parent) { */
  /*   new_frame->depth = new_frame->parent->depth + 1; */
  /* } */

  return new_frame;
}


// Initializes the cilkprof stack
void cilkprof_stack_init(cilkprof_stack_t *stack, FunctionType_t func_type)
{
  stack->in_user_code = false;
  stack->bot = NULL;
  stack->helper_sf_free_list = NULL;
  stack->spawner_sf_free_list = NULL;
  /* stack->c_fn_free_list = NULL; */

  stack->c_stack = (c_fn_frame_t*)malloc(sizeof(c_fn_frame_t) * START_C_STACK_SIZE);
  stack->c_stack_capacity = START_C_STACK_SIZE;
  stack->c_tail = 0;

  cilkprof_stack_frame_t *new_frame = (cilkprof_stack_frame_t *)malloc(sizeof(cilkprof_stack_frame_t));

  new_frame->prefix_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  new_frame->strand_prefix_table = strand_hashtable_create();
#endif
  new_frame->lchild_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  new_frame->strand_lchild_table = strand_hashtable_create();
#endif
  new_frame->contin_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  new_frame->strand_contin_table = strand_hashtable_create();
#endif

  cilkprof_stack_frame_init(new_frame, func_type, 0);
  cilkprof_c_fn_frame_init(&(stack->c_stack[0]));

  stack->bot = new_frame;

  stack->wrk_table = cc_hashtable_create();
#if COMPUTE_STRAND_DATA
  stack->strand_wrk_table = strand_hashtable_create();
#endif

  stack->cs_status_capacity = START_STATUS_VECTOR_SIZE;
  stack->fn_status_capacity = START_STATUS_VECTOR_SIZE;

  stack->cs_status = (cs_status_t*)malloc(sizeof(cs_status_t)
                                          * START_STATUS_VECTOR_SIZE);

  stack->fn_status = (fn_status_t*)malloc(sizeof(fn_status_t)
                                          * START_STATUS_VECTOR_SIZE);

  for (int i = 0; i < START_STATUS_VECTOR_SIZE; ++i) {
    /* stack->cs_status[i].count_on_stack = 0; */
    /* stack->cs_status[i].func_type = EMPTY; */
    stack->cs_status[i].c_tail = OFF_STACK;
    stack->cs_status[i].fn_index = UNINITIALIZED;
    stack->cs_status[i].flags = 0;
    stack->fn_status[i] = OFF_STACK;
  }

  init_strand_ruler(&(stack->strand_ruler));
}

// Doubles the capacity of a cs status vector
void resize_cs_status_vector(cs_status_t **old_status_vec,
                             int *old_vec_capacity) {
  int new_vec_capacity = *old_vec_capacity * 2;
  cs_status_t *new_status_vec = (cs_status_t*)malloc(sizeof(cs_status_t)
                                                     * new_vec_capacity);
  int i;
  for (i = 0; i < *old_vec_capacity; ++i) {
    new_status_vec[i] = (*old_status_vec)[i];
  }
  for ( ; i < new_vec_capacity; ++i) {
    /* new_status_vec[i].count_on_stack = 0; */
    new_status_vec[i].c_tail = OFF_STACK;
    new_status_vec[i].fn_index = UNINITIALIZED;
    new_status_vec[i].flags = 0;
  }

  free(*old_status_vec);
  *old_status_vec = new_status_vec;
  *old_vec_capacity = new_vec_capacity;
}

// Doubles the capacity of a fn status vector
void resize_fn_status_vector(fn_status_t **old_status_vec,
                             int *old_vec_capacity) {
  int new_vec_capacity = *old_vec_capacity * 2;
  fn_status_t *new_status_vec = (fn_status_t*)malloc(sizeof(fn_status_t)
                                                     * new_vec_capacity);
  int i;
  for (i = 0; i < *old_vec_capacity; ++i) {
    new_status_vec[i] = (*old_status_vec)[i];
  }
  for ( ; i < new_vec_capacity; ++i) {
    new_status_vec[i] = OFF_STACK;
  }

  free(*old_status_vec);
  *old_status_vec = new_status_vec;
  *old_vec_capacity = new_vec_capacity;
}

// Pops the bottommost C frame off of the stack
// stack->bot->c_fn_frame, and returns a pointer to it.
c_fn_frame_t* cilkprof_c_fn_pop(cilkprof_stack_t *stack)
{
  /* fprintf(stderr, "poping C stack\n"); */
  c_fn_frame_t *old_c_bot = &(stack->c_stack[stack->c_tail]);
  --stack->c_tail;
  assert(stack->c_tail >= stack->bot->c_head);
  /* stack->bot->c_fn_frame = stack->bot->c_fn_frame->parent; */
  /* assert(NULL != stack->bot->c_fn_frame); */
  return old_c_bot;
}


// Pops the bottommost frame off of the stack *stack, and returns a
// pointer to it.
cilkprof_stack_frame_t* cilkprof_stack_pop(cilkprof_stack_t *stack)
{
  cilkprof_stack_frame_t *old_bottom = stack->bot;
  stack->bot = stack->bot->parent;
  cilkprof_c_fn_pop(stack);
  // if (stack->bot && stack->bot->height < old_bottom->height + 1) {
  //  stack->bot->height = old_bottom->height + 1;
  // }

  return old_bottom;
}

#endif

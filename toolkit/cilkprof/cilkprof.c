#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* #define _POSIX_C_SOURCE = 200112L */
/* #define _POSIX_C_SOURCE = 200809L */
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include <float.h>
#include <unistd.h>
#include <sys/types.h>

#include "cilktool.h"

#include "cilkprof_stack.h"
#include "iaddrs.h"
#include "util.h"

#ifndef SERIAL_TOOL
#define SERIAL_TOOL 1
#endif

#ifndef OLD_PRINTOUT 
#define OLD_PRINTOUT 0
#endif

#ifndef COMPUTE_STRAND_DATA
#define COMPUTE_STRAND_DATA 0
#endif

#ifndef TRACE_CALLS
#define TRACE_CALLS 0
#endif

#ifndef BURDENING
#define BURDENING 0
#endif

#ifndef PRINT_RES
#define PRINT_RES 1
#endif

#if SERIAL_TOOL
#define GET_STACK(ex) ex
#else
#define GET_STACK(ex) REDUCER_VIEW(ex)
#include "cilkprof_stack_reducer.h"
#endif

#include <libgen.h>


// TB: Adjusted so I can terminate WHEN_TRACE_CALLS() with semicolons.
// Emacs gets confused about tabbing otherwise.
#if TRACE_CALLS
#define WHEN_TRACE_CALLS(ex) do { ex } while (0)
#else
#define WHEN_TRACE_CALLS(ex) do {} while (0)
#endif

/*************************************************************************/
/**
 * Data structures for tracking work and span.
 */


#if SERIAL_TOOL
static cilkprof_stack_t ctx_stack;
#else
cilkprof_wls_t *wls;
static CILK_C_DECLARE_REDUCER(cilkprof_stack_t) ctx_stack =
  CILK_C_INIT_REDUCER(cilkprof_stack_t,
		      reduce_cilkprof_stack,
		      identity_cilkprof_stack,
		      destroy_cilkprof_stack,
		      {NULL});
#endif

iaddr_table_t *call_site_table;
static iaddr_table_t *function_table;

static bool TOOL_INITIALIZED = false;
static bool TOOL_PRINTED = false;
static int TOOL_PRINT_NUM = 0;

extern int MIN_CAPACITY;
/* extern int MIN_LG_CAPACITY; */

/*************************************************************************/
/**
 * Helper methods.
 */

static inline void initialize_tool(cilkprof_stack_t *stack) {
#if SERIAL_TOOL
  // This is a serial tool
  ensure_serial_tool();
  call_site_table = iaddr_table_create();
  function_table = iaddr_table_create();
#else
  int P = __cilkrts_get_nworkers();
  wls = (cilkprof_wls_t*)malloc(sizeof(cilkprof_wls_t) * P);
  for (int p = 0; p < P; ++p) {
    cilkprof_wls_init(wls + p);
  }
  // probably need to register the reducer here as well.
  CILK_C_REGISTER_REDUCER(ctx_stack);
#endif
  cilkprof_stack_init(stack, MAIN);
  TOOL_INITIALIZED = true;
  TOOL_PRINTED = false;
}

__attribute__((always_inline))
void begin_strand(cilkprof_stack_t *stack) {
  start_strand(&(stack->strand_ruler));
}

__attribute__((always_inline))
uint64_t measure_and_add_strand_length(cilkprof_stack_t *stack) {
  // Measure strand length
  uint64_t strand_len = measure_strand_length(&(stack->strand_ruler));
  assert(NULL != stack->bot);

  // Accumulate strand length
  stack->c_stack[stack->c_tail].local_wrk += strand_len;
  /* stack->bot->c_fn_frame->local_wrk += strand_len; */
  /* stack->bot->c_fn_frame->local_contin += strand_len; */
  /* stack->bot->c_fn_frame->running_wrk += strand_len; */
  /* stack->bot->c_fn_frame->contin_spn += strand_len; */

#if COMPUTE_STRAND_DATA
  // Add strand length to strand_wrk and strand_contin tables
  /* fprintf(stderr, "start %lx, end %lx\n", stack->strand_start, stack->strand_end); */
  bool add_success = add_to_strand_hashtable(&(stack->strand_wrk_table),
                                             stack->strand_start,
                                             stack->strand_end,
                                             strand_len);
  assert(add_success);
  add_success = add_to_strand_hashtable(&(stack->bot->strand_contin_table),
                                        stack->strand_start,
                                        stack->strand_end,
                                        strand_len);
  assert(add_success);
#endif
  return strand_len;
}

/*************************************************************************/

void cilk_tool_init(void) {
  // Do the initialization only if it hasn't been done. 
  // It could have been done if we instrument C functions, and the user 
  // separately calls cilk_tool_init in the main function.
  if(!TOOL_INITIALIZED) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_init() [ret %p]\n",
                              __builtin_extract_return_addr(__builtin_return_address(0))); );

    initialize_tool(&GET_STACK(ctx_stack));

    GET_STACK(ctx_stack).in_user_code = true;

    begin_strand(&(GET_STACK(ctx_stack)));
  }
}

/* Cleaningup; note that these cleanup may not be performed if
 * the user did not include cilk_tool_destroy in its main function and the
 * program is not compiled with -fcilktool_instr_c.
 */
void cilk_tool_destroy(void) {
  // Do the destroy only if it hasn't been done. 
  // It could have been done if we instrument C functions, and the user 
  // separately calls cilk_tool_destroy in the main function.
  if(TOOL_INITIALIZED) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_destroy() [ret %p]\n",
                              __builtin_extract_return_addr(__builtin_return_address(0))); );

    cilkprof_stack_t *stack = &GET_STACK(ctx_stack);
    // Print the output, if we haven't done so already
    if (!TOOL_PRINTED)
      cilk_tool_print();

    /* cilkprof_stack_frame_t *old_bottom = cilkprof_stack_pop(stack); */
    cilkprof_stack_frame_t *old_bottom = stack->bot;
    stack->bot = NULL;

    assert(old_bottom && MAIN == old_bottom->func_type);

#if !SERIAL_TOOL
    CILK_C_UNREGISTER_REDUCER(ctx_stack);
#endif
    free_cc_hashtable(stack->wrk_table);
#if COMPUTE_STRAND_DATA
    free_strand_hashtable(stack->strand_wrk_table);
#endif
    old_bottom->parent = stack->helper_sf_free_list;
    stack->helper_sf_free_list = old_bottom;

    // Actually free the entries of the free lists
    /* c_fn_frame_t *c_fn_frame = stack->c_fn_free_list; */
    /* c_fn_frame_t *next_c_fn_frame; */
    /* while (NULL != c_fn_frame) { */
    /*   next_c_fn_frame = c_fn_frame->parent; */
    /*   free(c_fn_frame); */
    /*   c_fn_frame = next_c_fn_frame; */
    /* } */
    /* stack->c_fn_free_list = NULL; */

    cilkprof_stack_frame_t *free_frame = stack->helper_sf_free_list;
    cilkprof_stack_frame_t *next_free_frame;
    while (NULL != free_frame) {
      next_free_frame = free_frame->parent;
      /* c_fn_frame_t *c_fn_frame = free_frame->c_fn_frame; */
      /* assert(NULL == c_fn_frame->parent); */
      /* free(c_fn_frame); */
      free_cc_hashtable(free_frame->prefix_table);
      /* free_cc_hashtable(free_frame->lchild_table); */
      /* free_cc_hashtable(free_frame->contin_table); */
#if COMPUTE_STRAND_DATA
      free_strand_hashtable(free_frame->strand_prefix_table);
      /* free_strand_hashtable(free_frame->strand_lchild_table); */
      /* free_strand_hashtable(free_frame->strand_contin_table); */
#endif
      free(free_frame);
      free_frame = next_free_frame;
    }
    stack->helper_sf_free_list = NULL;

    free_frame = stack->spawner_sf_free_list;
    while (NULL != free_frame) {
      next_free_frame = free_frame->parent;
      /* c_fn_frame_t *c_fn_frame = free_frame->c_fn_frame; */
      /* assert(NULL == c_fn_frame->parent); */
      /* free(c_fn_frame); */
      free_cc_hashtable(free_frame->lchild_table);
      free_cc_hashtable(free_frame->contin_table);
#if COMPUTE_STRAND_DATA
      free_strand_hashtable(free_frame->strand_lchild_table);
      free_strand_hashtable(free_frame->strand_contin_table);
#endif
      free(free_frame);
      free_frame = next_free_frame;
    }
    stack->spawner_sf_free_list = NULL;

    free(stack->cs_status);
    free(stack->fn_status);
    free(stack->c_stack);

    cc_hashtable_list_el_t *free_list_el = ll_free_list;
    cc_hashtable_list_el_t *next_free_list_el;
    while (NULL != free_list_el) {
      next_free_list_el = free_list_el->next;
      free(free_list_el);
      free_list_el = next_free_list_el;
    }
    ll_free_list = NULL;

    // Free the tables of call sites and functions
    iaddr_table_free(call_site_table);
    call_site_table = NULL;
    iaddr_table_free(function_table);
    function_table = NULL;

    TOOL_INITIALIZED = false;
  }
}


void cilk_tool_print(void) {
  FILE *fout;
  char filename[64];

  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_tool_print()\n"); );

  assert(TOOL_INITIALIZED);

  cilkprof_stack_t *stack;
  stack = &GET_STACK(ctx_stack);

  assert(NULL != stack->bot);
  assert(MAIN == stack->bot->func_type);
  /* assert(NULL == stack->bot->c_fn_frame->parent); */
  assert(stack->bot->c_head == stack->c_tail);

  cilkprof_stack_frame_t *bottom = stack->bot;
  c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

  /* uint64_t span = stack->bot->prefix_spn + stack->bot->c_fn_frame->running_spn; */
  uint64_t span = bottom->prefix_spn + c_bottom->running_spn
      + bottom->local_spn + bottom->local_contin;

  add_cc_hashtables(&(bottom->prefix_table), &(bottom->contin_table));
  clear_cc_hashtable(bottom->contin_table);

  flush_cc_hashtable(&(bottom->prefix_table));

  cc_hashtable_t* span_table = bottom->prefix_table;
/* #if PRINT_RES */
/*   fprintf(stderr, */
/*           "span_table->list_size = %d, span_table->table_size = %d, span_table->lg_capacity = %d\n", */
/*   	  span_table->list_size, span_table->table_size, span_table->lg_capacity); */
/* #endif */

  /* uint64_t work = stack->bot->c_fn_frame->running_wrk; */
  uint64_t work = c_bottom->running_wrk + c_bottom->local_wrk;
  flush_cc_hashtable(&(stack->wrk_table));
  cc_hashtable_t* work_table = stack->wrk_table;
/* #if PRINT_RES */
/*   fprintf(stderr, */
/*           "work_table->list_size = %d, work_table->table_size = %d, work_table->lg_capacity = %d\n", */
/*   	  work_table->list_size, work_table->table_size, work_table->lg_capacity); */
/* #endif */

  // Read the proc maps list
  read_proc_maps();

  // Open call site CSV
  sprintf(filename, "cilkprof_cs_%d.csv", TOOL_PRINT_NUM);
  fout = fopen(filename, "w"); 

  // print the header for the csv file
  /* fprintf(fout, "file, line, call sites (rip), depth, "); */
  fprintf(fout, "file, line, call sites (rip), function type, ");
  fprintf(fout, "work on work, span on work, parallelism on work, count on work, ");
  fprintf(fout, "top work on work, top span on work, top parallelism on work, top count on work, ");
  fprintf(fout, "local work on work, local span on work, local parallelism on work, local count on work, ");
  fprintf(fout, "work on span, span on span, parallelism on span, count on span, ");
  fprintf(fout, "top work on span, top span on span, top parallelism on span, top count on span, ");
  fprintf(fout, "local work on span, local span on span, local parallelism on span, local count on span \n");

  // Parse tables
  int span_table_entries_read = 0;
  for (size_t i = 0; i < (1 << (call_site_table->lg_capacity)); ++i) {
    iaddr_record_t *record = call_site_table->records[i];
    /* fprintf(stderr, "\n"); */
    while (NULL != record /* && (uintptr_t)NULL != record->iaddr */) {
      /* fprintf(stderr, "rip %p (%d), ", record->iaddr, record->index); */

      assert(0 != record->iaddr);
      assert(0 <= record->index && record->index < (1 << work_table->lg_capacity));

      cc_hashtable_entry_t *entry = &(work_table->entries[ record->index ]);
      /* fprintf(stderr, "%p\n", entry->rip); */
      /* if (entry->rip != record->iaddr) { */
      if (empty_cc_entry_p(entry)) {
        record = record->next;
        continue;
      }

      assert(entry->rip == record->iaddr);

      uint64_t wrk_wrk = entry->wrk;
      uint64_t spn_wrk = entry->spn;
      double par_wrk = (double)wrk_wrk/(double)spn_wrk;
      uint64_t cnt_wrk = entry->count;

      uint64_t t_wrk_wrk = entry->top_wrk;
      uint64_t t_spn_wrk = entry->top_spn;
      double t_par_wrk = (double)t_wrk_wrk/(double)t_spn_wrk;
      uint64_t t_cnt_wrk = entry->top_count;

      uint64_t l_wrk_wrk = entry->local_wrk;
      uint64_t l_spn_wrk = entry->local_spn;
      double l_par_wrk = (double)l_wrk_wrk/(double)l_spn_wrk;
      uint64_t l_cnt_wrk = entry->local_count;

      uint64_t wrk_spn = 0;
      uint64_t spn_spn = 0;
      double par_spn = DBL_MAX;
      uint64_t cnt_spn = 0;

      uint64_t t_wrk_spn = 0;
      uint64_t t_spn_spn = 0;
      double t_par_spn = DBL_MAX;
      uint64_t t_cnt_spn = 0;

      uint64_t l_wrk_spn = 0;
      uint64_t l_spn_spn = 0;
      double l_par_spn = DBL_MAX;
      uint64_t l_cnt_spn = 0;

      if (record->index < (1 << span_table->lg_capacity)) {
        cc_hashtable_entry_t *st_entry = &(span_table->entries[ record->index ]);

        if (!empty_cc_entry_p(st_entry)) {
          assert(st_entry->rip == entry->rip);

          wrk_spn = st_entry->wrk;
          spn_spn = st_entry->spn;
          par_spn = (double)wrk_spn / (double)spn_spn;
          cnt_spn = st_entry->count;

          t_wrk_spn = st_entry->top_wrk;
          t_spn_spn = st_entry->top_spn;
          t_par_spn = (double)t_wrk_spn / (double)t_spn_spn;
          t_cnt_spn = st_entry->top_count;

          l_wrk_spn = st_entry->local_wrk;
          l_spn_spn = st_entry->local_spn;
          l_par_spn = (double)l_wrk_spn / (double)l_spn_spn;
          l_cnt_spn = st_entry->local_count;

          ++span_table_entries_read;
        }
      }

      int line = 0; 
      char *fstr = NULL;
      /* uint64_t addr = rip2cc(entry->rip); */
      uint64_t addr = rip2cc(record->iaddr);

      // get_info_on_inst_addr returns a char array from some system call that
      // needs to get freed by the user after we are done with the info
      char *line_to_free = get_info_on_inst_addr(addr, &line, &fstr);
      char *file = basename(fstr);
      /* fprintf(fout, "\"%s\", %d, 0x%lx, %d, ", file, line, addr, entry->depth); */
      fprintf(fout, "\"%s\", %d, 0x%lx, ", file, line, addr);
      /* if (entry->is_recursive) {  // recursive function */
      /* if (entry->func_type & IS_RECURSIVE) {  // recursive function */
      /* FunctionType_t func_type = (stack->cs_status[record->index].func_type & ~ON_STACK); */
      FunctionType_t func_type = record->func_type;
      if (stack->cs_status[record->index].flags & RECURSIVE) {  // recursive function
        fprintf(fout, "%s %s, ",
                FunctionType_str[func_type],
                FunctionType_str[IS_RECURSIVE]);
      } else {
        fprintf(fout, "%s, ", FunctionType_str[func_type]);
      }
      fprintf(fout, "%lu, %lu, %g, %lu, %lu, %lu, %g, %lu, %lu, %lu, %g, %lu, ", 
              wrk_wrk, spn_wrk, par_wrk, cnt_wrk,
              t_wrk_wrk, t_spn_wrk, t_par_wrk, t_cnt_wrk,
              l_wrk_wrk, l_spn_wrk, l_par_wrk, l_cnt_wrk);
      fprintf(fout, "%lu, %lu, %g, %lu, %lu, %lu, %g, %lu, %lu, %lu, %g, %lu\n", 
              wrk_spn, spn_spn, par_spn, cnt_spn,
              t_wrk_spn, t_spn_spn, t_par_spn, t_cnt_spn,
              l_wrk_spn, l_spn_spn, l_par_spn, l_cnt_spn);
      if(line_to_free) free(line_to_free);
      
      record = record->next;
    }
  }
  fclose(fout);

  /* if (span_table_entries_read != span_table->table_size) { */
  /*   fprintf(stderr, "read %d, table contains %d\n", */
  /*           span_table_entries_read, span_table->table_size); */
  /* } */
  assert(span_table_entries_read == span_table->table_size);

#if COMPUTE_STRAND_DATA
  // Strand tables
  add_strand_hashtables(&(stack->bot->strand_prefix_table), &(stack->bot->strand_contin_table));
  clear_strand_hashtable(stack->bot->strand_contin_table);

  flush_strand_hashtable(&(stack->bot->strand_prefix_table));

  strand_hashtable_t* strand_span_table = stack->bot->strand_prefix_table;
  fprintf(stderr, 
          "strand_span_table->list_size = %d, strand_span_table->table_size = %d, strand_span_table->lg_capacity = %d\n",
  	  strand_span_table->list_size, strand_span_table->table_size, strand_span_table->lg_capacity);


  flush_strand_hashtable(&(stack->strand_wrk_table));
  strand_hashtable_t* strand_work_table = stack->strand_wrk_table;
  fprintf(stderr, 
          "strand_work_table->list_size = %d, strand_work_table->table_size = %d, strand_work_table->lg_capacity = %d\n",
  	  strand_work_table->list_size, strand_work_table->table_size, strand_work_table->lg_capacity);

  // Open strand CSV
  sprintf(filename, "cilkprof_strand_%d.csv", TOOL_PRINT_NUM);
  fout = fopen(filename, "w"); 
  /* fout = fopen("cilkprof_strand.csv", "w");  */

  // print the header for the csv file
  fprintf(fout, "start file, start line, start rip, end file, end line, end rip, ");
  fprintf(fout, "work on work, count on work, ");
  fprintf(fout, "work on span, count on span \n");

  // Parse tables
  span_table_entries_read = 0;
  for (size_t i = 0; i < (1 << strand_work_table->lg_capacity); ++i) {
    strand_hashtable_entry_t *entry = &(strand_work_table->entries[i]);
    /* fprintf(stderr, "entry->start %lx, entry->end %lx\n", */
    /*         entry->start, entry->end); */
    if (!empty_strand_entry_p(entry)) {
      uint64_t wrk_wrk = entry->wrk;
      uint64_t on_wrk_cnt = entry->count;
      uint64_t wrk_spn = 0;
      uint64_t on_spn_cnt = 0;

      strand_hashtable_entry_t *st_entry = 
          get_strand_hashtable_entry_const(entry->start, entry->end, strand_span_table);
      if(st_entry && !empty_strand_entry_p(st_entry)) {
	  ++span_table_entries_read;
          wrk_spn = st_entry->wrk;
          on_spn_cnt = st_entry->count;
      }

#if OLD_PRINTOUT
      fprintf(stdout, "%lx:%lx ", rip2cc(entry->start), rip2cc(entry->end));
      fprintf(stdout, " %lu %lu\n",
	      wrk_wrk, wrk_spn);
#endif
      int line = 0; 
      char *fstr = NULL;
      uint64_t start_addr = rip2cc(entry->start);
      uint64_t end_addr = rip2cc(entry->end);

      // get_info_on_inst_addr returns a char array from some system call that
      // needs to get freed by the user after we are done with the info
      char *line_to_free = get_info_on_inst_addr(start_addr, &line, &fstr);
      char *file = basename(fstr);
      fprintf(fout, "\"%s\", %d, 0x%lx, ", file, line, start_addr);
      if(line_to_free) free(line_to_free);
      line_to_free = get_info_on_inst_addr(end_addr, &line, &fstr);
      file = basename(fstr);
      fprintf(fout, "\"%s\", %d, 0x%lx, ", file, line, end_addr);
      if(line_to_free) free(line_to_free);
      fprintf(fout, "%lu, %lu, %lu, %lu\n", 
              wrk_wrk, on_wrk_cnt, wrk_spn, on_spn_cnt);
    }
  }
  fclose(fout);

  assert(span_table_entries_read == strand_span_table->table_size);
#endif  // COMPUTE_STRAND_DATA

#if PRINT_RES
  print_work_span(work, span);
#endif

  /*
  fprintf(stderr, "%ld read, %d size\n", span_table_entries_read, span_table->table_size);
  fprintf(stderr, "Dumping span table:\n");
  for (size_t j = 0; j < (1 << span_table->lg_capacity); ++j) {
    cc_hashtable_entry_t *st_entry = &(span_table->entries[j]);
    if (empty_cc_entry_p(st_entry)) {
      continue;
    }
    fprintf(stderr, "entry %zu: rip %lx, depth %d\n", j, rip2cc(st_entry->rip), st_entry->depth);
  } */

  // Free the proc maps list
  mapping_list_el_t *map_lst_el = maps.head;
  mapping_list_el_t *next_map_lst_el;
  while (NULL != map_lst_el) {
    next_map_lst_el = map_lst_el->next;
    free(map_lst_el);
    map_lst_el = next_map_lst_el;
  }
  maps.head = NULL;
  maps.tail = NULL;

  TOOL_PRINTED = true;
  ++TOOL_PRINT_NUM;
}


/*************************************************************************/
/**
 * Hooks into runtime system.
 */

void cilk_enter_begin(uint32_t prop, __cilkrts_stack_frame *sf, void* this_fn, void* rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_begin(%u, %p, %p) [ret %p]\n", prop, sf, rip,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  /* fprintf(stderr, "worker %d entering %p\n", __cilkrts_get_worker_number(), sf); */
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if (!TOOL_INITIALIZED) {
    initialize_tool(&(ctx_stack));

  } else {
    stack = &(GET_STACK(ctx_stack));

#if COMPUTE_STRAND_DATA
    // Prologue disabled
    stack->strand_end
        = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
    uint64_t strand_len = measure_and_add_strand_length(stack);
    if (stack->bot->c_head == stack->c_tail) {
      stack->bot->local_contin += strand_len;
    }
    stack->in_user_code = false;
  }

  // Push new frame onto the stack
  cilkprof_stack_push(stack, SPAWNER);

  c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

  /* fprintf(stderr, "local_wrk %lu, running_wrk %lu, running_spn %lu\n", */
  /*         c_bottom->local_wrk, c_bottom->running_wrk, c_bottom->running_spn); */

  uintptr_t cs = (uintptr_t)__builtin_extract_return_addr(rip);
  uintptr_t fn = (uintptr_t)this_fn;

  int32_t cs_index = add_to_iaddr_table(&call_site_table, cs, SPAWNER);
  c_bottom->cs_index = cs_index;
  if (cs_index >= stack->cs_status_capacity) {
    resize_cs_status_vector(&(stack->cs_status), &(stack->cs_status_capacity));
  }
  int32_t cs_tail = stack->cs_status[cs_index].c_tail;
  if (OFF_STACK != cs_tail) {
    if (!(stack->cs_status[cs_index].flags & RECURSIVE)) {
      stack->cs_status[cs_index].flags |= RECURSIVE;
    }
  } else {
    int32_t fn_index;
    if (UNINITIALIZED == stack->cs_status[cs_index].fn_index) {

      assert(call_site_table->table_size == cs_index + 1);
      MIN_CAPACITY = cs_index + 1;

      fn_index = add_to_iaddr_table(&function_table, fn, SPAWNER);
      stack->cs_status[cs_index].fn_index = fn_index;
      if (fn_index >= stack->fn_status_capacity) {
        resize_fn_status_vector(&(stack->fn_status), &(stack->fn_status_capacity));
      }
    } else {
      fn_index = stack->cs_status[cs_index].fn_index;
    }
    stack->cs_status[cs_index].c_tail = stack->c_tail;
    if (OFF_STACK == stack->fn_status[fn_index]) {
      stack->fn_status[fn_index] = stack->c_tail;
    }
  }


#ifndef NDEBUG
  c_bottom->rip = (uintptr_t)__builtin_extract_return_addr(rip);
  c_bottom->function = (uintptr_t)this_fn;
#endif
}


void cilk_enter_helper_begin(__cilkrts_stack_frame *sf, void *this_fn, void *rip)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_helper_begin(%p, %p) [ret %p]\n", sf, rip,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  // We should have passed spawn_or_continue(0) to get here.
  assert(stack->in_user_code);
#if COMPUTE_STRAND_DATA
  // Prologue disabled
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  // A helper should not be invoked from a C function
  assert(stack->bot->c_head == stack->c_tail);

  uint64_t strand_len = measure_and_add_strand_length(stack);
  stack->bot->local_contin += strand_len;

  stack->in_user_code = false;

  // Push new frame onto the stack
  cilkprof_stack_push(stack, HELPER);

  c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

  uintptr_t cs = (uintptr_t)__builtin_extract_return_addr(rip);
  uintptr_t fn = (uintptr_t)this_fn;

  int32_t cs_index = add_to_iaddr_table(&call_site_table, cs, HELPER);
  c_bottom->cs_index = cs_index;
  if (cs_index >= stack->cs_status_capacity) {
    resize_cs_status_vector(&(stack->cs_status), &(stack->cs_status_capacity));
  }
  int32_t cs_tail = stack->cs_status[cs_index].c_tail;
  if (OFF_STACK != cs_tail) {
    if (!(stack->cs_status[cs_index].flags & RECURSIVE)) {
      stack->cs_status[cs_index].flags |= RECURSIVE;
    }
  } else {
    int32_t fn_index;
    if (UNINITIALIZED == stack->cs_status[cs_index].fn_index) {

      assert(call_site_table->table_size == cs_index + 1);
      MIN_CAPACITY = cs_index + 1;

      fn_index = add_to_iaddr_table(&function_table, fn, SPAWNER);
      stack->cs_status[cs_index].fn_index = fn_index;
      if (fn_index >= stack->fn_status_capacity) {
        resize_fn_status_vector(&(stack->fn_status), &(stack->fn_status_capacity));
      }
    } else {
      fn_index = stack->cs_status[cs_index].fn_index;
    }
    stack->cs_status[cs_index].c_tail = stack->c_tail;
    if (OFF_STACK == stack->fn_status[fn_index]) {
      stack->fn_status[fn_index] = stack->c_tail;
    }
  }

#ifndef NDEBUG
  c_bottom->rip = (uintptr_t)__builtin_extract_return_addr(rip);
  c_bottom->function = (uintptr_t)this_fn;
#endif

}

void cilk_enter_end(__cilkrts_stack_frame *sf, void *rsp)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  if (SPAWNER == stack->bot->func_type) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from SPAWNER [ret %p]\n", sf, rsp,
                              __builtin_extract_return_addr(__builtin_return_address(0))); );
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_enter_end(%p, %p) from HELPER [ret %p]\n", sf, rsp,
                              __builtin_extract_return_addr(__builtin_return_address(0))); );
  }
  assert(!(stack->in_user_code));

  stack->in_user_code = true;

#if COMPUTE_STRAND_DATA
  stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  begin_strand(stack);
}

void cilk_tool_c_function_enter(uint32_t prop, void *this_fn, void *rip)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_enter(%u, %p, %p) [ret %p]\n", prop, this_fn, rip,
     __builtin_extract_return_addr(__builtin_return_address(0))); );

  if(!TOOL_INITIALIZED) { // We are entering main.
    cilk_tool_init(); // this will push the frame for MAIN and do a gettime

    c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

    uintptr_t cs = (uintptr_t)__builtin_extract_return_addr(rip);
    uintptr_t fn = (uintptr_t)this_fn;

    int32_t cs_index = add_to_iaddr_table(&call_site_table, cs, MAIN);
    c_bottom->cs_index = cs_index;
    if (cs_index >= stack->cs_status_capacity) {
      resize_cs_status_vector(&(stack->cs_status), &(stack->cs_status_capacity));
    }
    stack->cs_status[cs_index].c_tail = stack->c_tail;
    assert(call_site_table->table_size == cs_index + 1);
    MIN_CAPACITY = cs_index + 1;

    int32_t fn_index = add_to_iaddr_table(&function_table, fn, MAIN);
    stack->cs_status[cs_index].fn_index = fn_index;
    /* c_bottom->fn_index = fn_index; */
    if (fn_index >= stack->fn_status_capacity) {
      resize_fn_status_vector(&(stack->fn_status), &(stack->fn_status_capacity));
    }
    assert(OFF_STACK == stack->fn_status[fn_index]);
    stack->fn_status[fn_index] = stack->c_tail;

#ifndef NDEBUG
    c_bottom->rip = (uintptr_t)__builtin_extract_return_addr(rip);
    c_bottom->function = (uintptr_t)this_fn;
#endif

#if COMPUTE_STRAND_DATA
    stack->strand_start
        = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  } else {
    if (!stack->in_user_code) {
      WHEN_TRACE_CALLS( fprintf(stderr, "c_function_enter(%p) [ret %p]\n", rip,
                                __builtin_extract_return_addr(__builtin_return_address(0))); );
    }
    assert(stack->in_user_code);
#if COMPUTE_STRAND_DATA
    stack->strand_end
        = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif

    uint64_t strand_len = measure_and_add_strand_length(stack);
    if (stack->bot->c_head == stack->c_tail) {
      stack->bot->local_contin += strand_len;
    }

    // Push new frame for this C function onto the stack
    /* cilkprof_stack_push(stack, C_FUNCTION); */
    c_fn_frame_t *c_bottom = cilkprof_c_fn_push(stack);

    uintptr_t cs = (uintptr_t)__builtin_extract_return_addr(rip);
    uintptr_t fn = (uintptr_t)this_fn;

    int32_t cs_index = add_to_iaddr_table(&call_site_table, cs, C_FUNCTION);
    c_bottom->cs_index = cs_index;
    if (cs_index >= stack->cs_status_capacity) {
      resize_cs_status_vector(&(stack->cs_status), &(stack->cs_status_capacity));
    }
    int32_t cs_tail = stack->cs_status[cs_index].c_tail;
    if (OFF_STACK != cs_tail) {
      if (!(stack->cs_status[cs_index].flags & RECURSIVE)) {
        stack->cs_status[cs_index].flags |= RECURSIVE;
      }
    } else {
      int32_t fn_index;
      if (UNINITIALIZED == stack->cs_status[cs_index].fn_index) {

        assert(call_site_table->table_size == cs_index + 1);
        MIN_CAPACITY = cs_index + 1;

        fn_index = add_to_iaddr_table(&function_table, fn, C_FUNCTION);
        stack->cs_status[cs_index].fn_index = fn_index;
        if (fn_index >= stack->fn_status_capacity) {
          resize_fn_status_vector(&(stack->fn_status), &(stack->fn_status_capacity));
        }
      } else {
        fn_index = stack->cs_status[cs_index].fn_index;
      }
      stack->cs_status[cs_index].c_tail = stack->c_tail;
      if (OFF_STACK == stack->fn_status[fn_index]) {
        stack->fn_status[fn_index] = stack->c_tail;
      }
    }

    /* fprintf(stderr, "cs_index %d\n", c_bottom->cs_index); */

#ifndef NDEBUG
    c_bottom->rip = (uintptr_t)__builtin_extract_return_addr(rip);
    c_bottom->function = (uintptr_t)this_fn;
#endif

#if COMPUTE_STRAND_DATA
    stack->strand_start
        = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
    /* the stop time is also the start time of this function */
    // stack->start = stack->stop; /* TB: Want to exclude the length
    // (e.g. time or instruction count) of this function */
    begin_strand(stack);
  }
}

void cilk_tool_c_function_leave(void *rip)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "c_function_leave(%p) [ret %p]\n", rip,
     __builtin_extract_return_addr(__builtin_return_address(0))); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  const c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);
  if (NULL != stack->bot &&
      MAIN == stack->bot->func_type &&
      stack->c_tail == stack->bot->c_head) {

    int32_t cs_index = c_bottom->cs_index;
    int32_t cs_tail = stack->cs_status[cs_index].c_tail;
    bool top_cs = (cs_tail == stack->c_tail);

    if (top_cs) {
      stack->cs_status[cs_index].c_tail = OFF_STACK;
      int32_t fn_index = stack->cs_status[cs_index].fn_index;
      if (stack->fn_status[fn_index] == stack->c_tail) {
        stack->fn_status[fn_index] = OFF_STACK;
      }
    }

    cilk_tool_destroy();
  }
  if (!TOOL_INITIALIZED) {
    // either user code already called cilk_tool_destroy, or we are leaving
    // main; in either case, nothing to do here;
    return;
  }

  bool add_success;
  /* cilkprof_stack_frame_t *old_bottom; */
  const c_fn_frame_t *old_bottom;

  assert(stack->in_user_code);
  // stop the timer and attribute the elapsed time to this returning
  // function
#if COMPUTE_STRAND_DATA
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  measure_and_add_strand_length(stack);

  assert(stack->c_tail > stack->bot->c_head);
  // Given this is a C function, everything should be accumulated in
  // contin_spn and contin_table, so let's just deposit that into the
  // parent.
  /* assert(0 == stack->bot->prefix_spn); */
  /* assert(0 == stack->bot->local_spn); */
  /* assert(0 == stack->bot->lchild_spn); */
/*   assert(cc_hashtable_is_empty(stack->bot->prefix_table)); */
/*   assert(cc_hashtable_is_empty(stack->bot->lchild_table)); */
/* #if COMPUTE_STRAND_DATA */
/*   assert(strand_hashtable_is_empty(stack->bot->strand_prefix_table)); */
/*   assert(strand_hashtable_is_empty(stack->bot->strand_lchild_table)); */
/* #endif */

  // Pop the stack
  old_bottom = cilkprof_c_fn_pop(stack);
  /* assert(old_bottom->local_wrk == old_bottom->local_contin); */
  uint64_t local_wrk = old_bottom->local_wrk;
  uint64_t running_wrk = old_bottom->running_wrk + local_wrk;
  uint64_t running_spn = old_bottom->running_spn + local_wrk;

  int32_t cs_index = old_bottom->cs_index;
  int32_t cs_tail = stack->cs_status[cs_index].c_tail;
  bool top_cs = (cs_tail == stack->c_tail + 1);

  /* fprintf(stderr, "cs_index = %d\n", cs_index); */
  if (top_cs) {  // top CS instance
    stack->cs_status[cs_index].c_tail = OFF_STACK;
    int32_t fn_index = stack->cs_status[cs_index].fn_index;
    if (stack->fn_status[fn_index] == stack->c_tail + 1) {
      stack->fn_status[fn_index] = OFF_STACK;
    }
  }

  c_fn_frame_t *new_bottom = &(stack->c_stack[stack->c_tail]);
  new_bottom->running_wrk += running_wrk;
  new_bottom->running_spn += running_spn;

  // TB: This assert can fail if the compiler does really aggressive
  // inlining.  See bfs compiled with -O3.
  /* assert(old_bottom->top_cs || !stack->bot->top_fn); */

  cc_hashtable_t **dst_spn_table;
  if (0 == stack->bot->lchild_spn) {
    dst_spn_table = &(stack->bot->prefix_table);
  } else {
    dst_spn_table = &(stack->bot->contin_table);
  }

  assert(NULL != dst_spn_table);

  // Update work table
  if (top_cs) {
    uint32_t fn_index = stack->cs_status[new_bottom->cs_index].fn_index;
    /* fprintf(stderr, "adding to wrk table\n"); */
    add_success = add_to_cc_hashtable(&(stack->wrk_table),
                                      stack->c_tail == stack->fn_status[fn_index],
                                      cs_index,
#ifndef NDEBUG
                                      old_bottom->rip,
#endif
                                      running_wrk,
                                      running_spn,
                                      local_wrk,
                                      local_wrk);
    assert(add_success);
    /* fprintf(stderr, "adding to prefix table\n"); */
    add_success = add_to_cc_hashtable(dst_spn_table/* &(stack->bot->contin_table) */,
                                      stack->c_tail == stack->fn_status[fn_index],
                                      cs_index,
#ifndef NDEBUG
                                      old_bottom->rip,
#endif
                                      running_wrk,
                                      running_spn,
                                      local_wrk,
                                      local_wrk);
    assert(add_success);
  } else {
    // Only record the local work and local span
    /* fprintf(stderr, "adding to wrk table\n"); */
    add_success = add_local_to_cc_hashtable(&(stack->wrk_table),
                                            cs_index,
#ifndef NDEBUG
                                            old_bottom->rip,
#endif
                                            local_wrk,
                                            local_wrk);
    assert(add_success);
    /* fprintf(stderr, "adding to contin table\n"); */
    add_success = add_local_to_cc_hashtable(dst_spn_table/* &(stack->bot->contin_table) */,
                                            cs_index,
#ifndef NDEBUG
                                            old_bottom->rip,
#endif
                                            local_wrk,
                                            local_wrk);
    assert(add_success);
  }

/*   // clean up */
/*   clear_cc_hashtable(old_bottom->prefix_table); */
/*   clear_cc_hashtable(old_bottom->contin_table); */
/*   clear_cc_hashtable(old_bottom->lchild_table); */
/* #if COMPUTE_STRAND_DATA */
/*   clear_strand_hashtable(old_bottom->strand_prefix_table); */
/*   clear_strand_hashtable(old_bottom->strand_contin_table); */
/*   clear_strand_hashtable(old_bottom->strand_lchild_table); */
/* #endif */
  /* free(old_bottom); */
  /* old_bottom->parent = stack->c_fn_free_list; */
  /* stack->c_fn_free_list = old_bottom; */

#if COMPUTE_STRAND_DATA
  stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  begin_strand(stack);
}

void cilk_spawn_prepare(__cilkrts_stack_frame *sf)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_spawn_prepare(%p) [ret %p]\n", sf,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  // Tool must have been initialized as this is only called in a SPAWNER, and 
  // we would have at least initialized the tool in the first cilk_enter_begin.
  assert(TOOL_INITIALIZED);

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

#if COMPUTE_STRAND_DATA
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  uint64_t strand_len = measure_and_add_strand_length(stack);
  stack->bot->local_contin += strand_len;

  assert(stack->c_tail == stack->bot->c_head);

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

// If in_continuation == 0, we just did setjmp and about to call the spawn helper.  
// If in_continuation == 1, we are resuming after setjmp (via longjmp) at the continuation 
// of a spawn statement; note that this is possible only if steals can occur.
void cilk_spawn_or_continue(int in_continuation)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  assert(stack->c_tail == stack->bot->c_head);

  assert(!(stack->in_user_code));
  if (in_continuation) {
    // In the continuation
    WHEN_TRACE_CALLS(
        fprintf(stderr, "cilk_spawn_or_continue(%d) from continuation [ret %p]\n", in_continuation,
                __builtin_extract_return_addr(__builtin_return_address(0))); );
    stack->in_user_code = true;

    stack->bot->local_contin += BURDENING;

#if COMPUTE_STRAND_DATA
    stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
    begin_strand(stack);
  } else {
    // In the spawned child
    WHEN_TRACE_CALLS(
        fprintf(stderr, "cilk_spawn_or_continue(%d) from spawn [ret %p]\n", in_continuation,
                __builtin_extract_return_addr(__builtin_return_address(0))); );
    // We need to re-enter user code, because function calls for
    // arguments might be called before enter_helper_begin occurs in
    // spawn helper.
    stack->in_user_code = true;

#if COMPUTE_STRAND_DATA
    stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
    begin_strand(stack);
  }
}

void cilk_detach_begin(__cilkrts_stack_frame *parent)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_begin(%p) [ret %p]\n", parent,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));
  assert(HELPER == stack->bot->func_type);

#if COMPUTE_STRAND_DATA
  // Prologue disabled
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  uint64_t strand_len = measure_and_add_strand_length(stack);
  stack->bot->local_contin += strand_len;

  assert(stack->bot->c_head == stack->c_tail);

  assert(stack->in_user_code);
  stack->in_user_code = false;

  return;
}

void cilk_detach_end(void)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_detach_end() [ret %p]\n",
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  assert(stack->bot->c_head == stack->c_tail);
  
  assert(!(stack->in_user_code));
  stack->in_user_code = true;

#if COMPUTE_STRAND_DATA
  // Prologue disabled
  stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  begin_strand(stack);

  return;
}

void cilk_sync_begin(__cilkrts_stack_frame *sf)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

#if COMPUTE_STRAND_DATA
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  uint64_t strand_len = measure_and_add_strand_length(stack);
  stack->bot->local_contin += strand_len;

  assert(stack->bot->c_head == stack->c_tail);

  if (SPAWNER == stack->bot->func_type) {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_begin(%p) from SPAWNER [ret %p]\n", sf,
                              __builtin_extract_return_addr(__builtin_return_address(0))); );
  } else {
    WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_begin(%p) from HELPER [ret %p]\n", sf,
                              __builtin_extract_return_addr(__builtin_return_address(0))); );
  }

  assert(stack->in_user_code);
  stack->in_user_code = false;
}

void cilk_sync_end(__cilkrts_stack_frame *sf)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  assert(stack->bot->c_head == stack->c_tail);

  c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

  /* c_bottom->running_spn += stack->bot->local_contin; */

  /* fprintf(stderr, "local_wrk %lu, running_wrk %lu, running_spn %lu\n", */
  /*         c_bottom->local_wrk, c_bottom->running_wrk, c_bottom->running_spn); */

  // can't be anything else; only SPAWNER have sync
  assert(SPAWNER == stack->bot->func_type); 

  /* if (stack->bot->lchild_spn > stack->bot->contin_spn) { */
  if (stack->bot->lchild_spn > c_bottom->running_spn + stack->bot->local_contin) {
    stack->bot->prefix_spn += stack->bot->lchild_spn;
    // local_spn does not increase, because critical path goes through
    // spawned child.
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->lchild_table));
#if COMPUTE_STRAND_DATA
    add_strand_hashtables(&(stack->bot->strand_prefix_table), &(stack->bot->strand_lchild_table));
#endif
  } else {
    /* stack->bot->prefix_spn += stack->bot->contin_spn; */
    stack->bot->prefix_spn += c_bottom->running_spn;
    // critical path goes through continuation, which is local.  add
    // local_contin to local_spn.
    stack->bot->local_spn += stack->bot->local_contin;
    add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));
#if COMPUTE_STRAND_DATA
    add_strand_hashtables(&(stack->bot->strand_prefix_table), &(stack->bot->strand_contin_table));
#endif
  }

  // reset lchild and contin span variables
  stack->bot->lchild_spn = 0;
  c_bottom->running_spn = 0;
  stack->bot->local_contin = 0;
  clear_cc_hashtable(stack->bot->lchild_table);
  clear_cc_hashtable(stack->bot->contin_table);
#if COMPUTE_STRAND_DATA
  clear_strand_hashtable(stack->bot->strand_lchild_table);
  clear_strand_hashtable(stack->bot->strand_contin_table);
#endif

  /* fprintf(stderr, "local_wrk %lu, running_wrk %lu, local_spn %lu, prefix_spn %lu\n", */
  /*         c_bottom->local_wrk, c_bottom->running_wrk, stack->bot->local_spn, stack->bot->prefix_spn); */

  assert(!(stack->in_user_code));
  stack->in_user_code = true;
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_sync_end(%p) from SPAWNER [ret %p]\n", sf,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );
#if COMPUTE_STRAND_DATA
  stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  begin_strand(stack);
}

void cilk_leave_begin(__cilkrts_stack_frame *sf)
{
  WHEN_TRACE_CALLS( fprintf(stderr, "cilk_leave_begin(%p) [ret %p]\n", sf,
                            __builtin_extract_return_addr(__builtin_return_address(0))); );

  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

  cilkprof_stack_frame_t *old_bottom;
  bool add_success;

#if COMPUTE_STRAND_DATA
  // Epilogues disabled
  stack->strand_end
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  uint64_t strand_len = measure_and_add_strand_length(stack);
  stack->bot->local_contin += strand_len;

  assert(stack->in_user_code);
  stack->in_user_code = false;

  // We are leaving this function, so it must have sync-ed, meaning
  // that, lchild should be 0 / empty.  prefix could contain value,
  // however, if this function is a Cilk function that spawned before.
  assert(0 == stack->bot->lchild_spn);
  if (SPAWNER == stack->bot->func_type) {
    assert(cc_hashtable_is_empty(stack->bot->lchild_table));
#if COMPUTE_STRAND_DATA
    assert(strand_hashtable_is_empty(stack->bot->strand_lchild_table));
#endif
  } else {
    assert(NULL == stack->bot->lchild_table);
#if COMPUTE_STRAND_DATA
    assert(NULL == stack->bot->strand_lchild_table);
#endif
  }
  assert(stack->bot->c_head == stack->c_tail);

  c_fn_frame_t *old_c_bottom = &(stack->c_stack[stack->c_tail]);

  stack->bot->prefix_spn += old_c_bottom->running_spn;
  stack->bot->local_spn += stack->bot->local_contin + BURDENING;
  old_c_bottom->running_wrk += old_c_bottom->local_wrk;
  stack->bot->prefix_spn += stack->bot->local_spn;

  if (SPAWNER == stack->bot->func_type) {
    assert(cc_hashtable_is_empty(stack->bot->contin_table));
  } else {
    assert(NULL == stack->bot->contin_table);
  }
/*   add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table)); */
/* #if COMPUTE_STRAND_DATA */
/*   add_strand_hashtables(&(stack->bot->strand_prefix_table), &(stack->bot->strand_contin_table)); */
/* #endif */

  /* fprintf(stderr, "local_wrk %lu, running_wrk %lu, local_spn %lu, prefix_spn %lu\n", */
  /*         old_c_bottom->local_wrk, old_c_bottom->running_wrk, stack->bot->local_spn, stack->bot->prefix_spn); */

  // Pop the stack
  old_bottom = cilkprof_stack_pop(stack);

  c_fn_frame_t *c_bottom = &(stack->c_stack[stack->c_tail]);

  int32_t cs_index = old_c_bottom->cs_index;
  int32_t cs_tail = stack->cs_status[cs_index].c_tail;
  bool top_cs = (cs_tail == stack->c_tail + 1);

  if (top_cs) {  // top CS instance
    stack->cs_status[cs_index].c_tail = OFF_STACK;
    int32_t fn_index = stack->cs_status[cs_index].fn_index;
    if (stack->fn_status[fn_index] == stack->c_tail + 1) {
      stack->fn_status[fn_index] = OFF_STACK;
    }
  }

  c_bottom->running_wrk += old_c_bottom->running_wrk;

  // Update work table
  if (top_cs) {
    /* assert(stack->cs_status[c_bottom->cs_index].fn_index == c_bottom->fn_index); */
    int32_t fn_index = stack->cs_status[c_bottom->cs_index].fn_index;
    /* fprintf(stderr, "adding to wrk table\n"); */
    add_success = add_to_cc_hashtable(&(stack->wrk_table),
                                      stack->c_tail == stack->fn_status[fn_index],
                                      cs_index,
#ifndef NDEBUG
                                      old_c_bottom->rip,
#endif
                                      old_c_bottom->running_wrk,
                                      old_bottom->prefix_spn,
                                      old_c_bottom->local_wrk,
                                      old_bottom->local_spn);
    assert(add_success);
    /* fprintf(stderr, "adding to prefix table\n"); */
    add_success = add_to_cc_hashtable(&(old_bottom->prefix_table),
                                      stack->c_tail == stack->fn_status[fn_index],
                                      cs_index,
#ifndef NDEBUG
                                      old_c_bottom->rip,
#endif
                                      old_c_bottom->running_wrk,
                                      old_bottom->prefix_spn,
                                      old_c_bottom->local_wrk,
                                      old_bottom->local_spn);
    assert(add_success);
  } else {
    // Only record the local work and local span
    /* fprintf(stderr, "adding to wrk table\n"); */
    add_success = add_local_to_cc_hashtable(&(stack->wrk_table),
                                            cs_index,
#ifndef NDEBUG
                                            old_c_bottom->rip,
#endif
                                            old_c_bottom->local_wrk,
                                            old_bottom->local_spn);
    assert(add_success);
    /* fprintf(stderr, "adding to prefix table\n"); */
    add_success = add_local_to_cc_hashtable(&(old_bottom->prefix_table),
                                            cs_index,
#ifndef NDEBUG
                                            old_c_bottom->rip,
#endif
                                            old_c_bottom->local_wrk,
                                            old_bottom->local_spn);
    assert(add_success);
  }

  if (SPAWNER == old_bottom->func_type) {
    // This is the case we are returning to a call, since a spawn
    // helper never calls a HELPER.

    assert(NULL != old_bottom->parent);

    // Update continuation variable
    c_bottom->running_spn += old_bottom->prefix_spn;
    // Don't increment local_spn for new stack->bot.
    /* fprintf(stderr, "adding tables\n"); */

    assert(cc_hashtable_is_empty(old_bottom->contin_table));
    assert(cc_hashtable_is_empty(old_bottom->lchild_table));
#if COMPUTE_STRAND_DATA
    assert(cc_hashtable_is_empty(old_bottom->contin_table));
    assert(cc_hashtable_is_empty(old_bottom->lchild_table));
#endif
    // Need to reassign pointers, in case of table resize.
    if (0 == stack->bot->lchild_spn) {
      stack->bot->prefix_table = old_bottom->prefix_table;
      /* assert(stack->bot->prefix_table == old_bottom->prefix_table); */
      // No outstanding spawned children
/*       add_cc_hashtables(&(stack->bot->prefix_table), &(old_bottom->prefix_table)); */
/* #if COMPUTE_STRAND_DATA */
/*       add_strand_hashtables(&(stack->bot->strand_prefix_table), &(old_bottom->strand_prefix_table)); */
/* #endif */
    } else {
      stack->bot->contin_table = old_bottom->prefix_table;
      /* assert(stack->bot->contin_table == old_bottom->prefix_table); */
/*       add_cc_hashtables(&(stack->bot->contin_table), &(old_bottom->prefix_table)); */
/* #if COMPUTE_STRAND_DATA */
/*       add_strand_hashtables(&(stack->bot->strand_contin_table), &(old_bottom->strand_prefix_table)); */
/* #endif */
    }

/*     /\* clear_cc_hashtable(old_bottom->prefix_table); *\/ */
/*     clear_cc_hashtable(old_bottom->lchild_table); */
/*     clear_cc_hashtable(old_bottom->contin_table); */
/* #if COMPUTE_STRAND_DATA */
/*     /\* clear_strand_hashtable(old_bottom->strand_prefix_table); *\/ */
/*     clear_strand_hashtable(old_bottom->strand_lchild_table); */
/*     clear_strand_hashtable(old_bottom->strand_contin_table); */
/* #endif */
  } else {
    // This is the case we are returning to a spawn, since a HELPER 
    // is always invoked due to a spawn statement.

    assert(HELPER != stack->bot->func_type);

    assert(NULL == old_bottom->contin_table);
    assert(NULL == old_bottom->lchild_table);
    /* assert(cc_hashtable_is_empty(old_bottom->contin_table)); */
    /* assert(cc_hashtable_is_empty(old_bottom->lchild_table)); */
#if COMPUTE_STRAND_DATA
    assert(NULL == old_bottom->contin_table);
    assert(NULL == old_bottom->lchild_table);
    /* assert(cc_hashtable_is_empty(old_bottom->contin_table)); */
    /* assert(cc_hashtable_is_empty(old_bottom->lchild_table)); */
#endif

    if (c_bottom->running_spn + stack->bot->local_contin + old_bottom->prefix_spn > stack->bot->lchild_spn) {
      // fprintf(stderr, "updating longest child\n");
      stack->bot->prefix_spn += c_bottom->running_spn;
      stack->bot->local_spn += stack->bot->local_contin;
      add_cc_hashtables(&(stack->bot->prefix_table), &(stack->bot->contin_table));
#if COMPUTE_STRAND_DATA
      add_strand_hashtables(&(stack->bot->strand_prefix_table), &(stack->bot->strand_contin_table));
#endif

      // Save old_bottom tables in new bottom's l_child variable.
      stack->bot->lchild_spn = old_bottom->prefix_spn;
      clear_cc_hashtable(stack->bot->lchild_table);
      /* free(stack->bot->lchild_table); */
      cc_hashtable_t* tmp_cc = stack->bot->lchild_table;
      stack->bot->lchild_table = old_bottom->prefix_table;
      old_bottom->prefix_table = tmp_cc;
#if COMPUTE_STRAND_DATA
      clear_strand_hashtable(stack->bot->strand_lchild_table);
      /* free(stack->bot->strand_lchild_table); */
      strand_hashtable_t* tmp_strand = stack->bot->strand_lchild_table;
      stack->bot->strand_lchild_table = old_bottom->strand_prefix_table;
      old_bottom->strand_prefix_table = tmp_strand;
#endif
/*       stack->bot->lchild_table = old_bottom->prefix_table; */
/* #if COMPUTE_STRAND_DATA */
/*       stack->bot->strand_lchild_table = old_bottom->strand_prefix_table; */
/* #endif */
      // Free old_bottom tables that are no longer in use
      assert(cc_hashtable_is_empty(old_bottom->prefix_table));
      /* clear_cc_hashtable(old_bottom->lchild_table); */
      /* clear_cc_hashtable(old_bottom->contin_table); */
#if COMPUTE_STRAND_DATA
      assert(cc_hashtable_is_empty(old_bottom->strand_prefix_table));
      /* clear_strand_hashtable(old_bottom->strand_lchild_table); */
      /* clear_strand_hashtable(old_bottom->strand_contin_table); */
#endif

      // Empy new bottom's continuation
      c_bottom->running_spn = 0;
      stack->bot->local_contin = 0;
      clear_cc_hashtable(stack->bot->contin_table);
#if COMPUTE_STRAND_DATA
      clear_strand_hashtable(stack->bot->strand_contin_table);
#endif
    } else {
      // Discared all tables from old_bottom
      clear_cc_hashtable(old_bottom->prefix_table);
      /* clear_cc_hashtable(old_bottom->lchild_table); */
      /* clear_cc_hashtable(old_bottom->contin_table); */
#if COMPUTE_STRAND_DATA
      clear_strand_hashtable(old_bottom->strand_prefix_table);
      /* clear_strand_hashtable(old_bottom->strand_lchild_table); */
      /* clear_strand_hashtable(old_bottom->strand_contin_table); */
#endif
    }
  }

  /* free(old_bottom); */
  if (SPAWNER == old_bottom->func_type) {
    old_bottom->parent = stack->spawner_sf_free_list;
    stack->spawner_sf_free_list = old_bottom;
  } else {
    old_bottom->parent = stack->helper_sf_free_list;
    stack->helper_sf_free_list = old_bottom;
  }
}

void cilk_leave_end(void)
{
  cilkprof_stack_t *stack = &(GET_STACK(ctx_stack));

#if TRACE_CALLS
  switch(stack->bot->func_type) {
    case HELPER:
      fprintf(stderr, "cilk_leave_end() from HELPER [ret %p]\n",
              __builtin_extract_return_addr(__builtin_return_address(0)));
      break;
    case SPAWNER:
      fprintf(stderr, "cilk_leave_end() from SPAWNER [ret %p]\n",
              __builtin_extract_return_addr(__builtin_return_address(0)));
      break;
    case MAIN:
      fprintf(stderr, "cilk_leave_end() from MAIN [ret %p]\n",
              __builtin_extract_return_addr(__builtin_return_address(0)));
      break;
    case C_FUNCTION:
      // We can have leave_end from C_FUNCTION because leave_begin
      // popped the stack already
      fprintf(stderr, "cilk_leave_end() from C_FUNCTION [ret %p]\n",
              __builtin_extract_return_addr(__builtin_return_address(0)));
      break;
    default:
      assert(false && "Should not reach this point.");
  }
#endif

  assert(!(stack->in_user_code));
  stack->in_user_code = true;
#if COMPUTE_STRAND_DATA
  // Epilogues disabled
  stack->strand_start
      = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
#endif
  begin_strand(stack);
}

#include "cc_hashtable.c"
#include "util.c"
#include "iaddrs.c"
#include "csi.c"

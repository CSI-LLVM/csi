#define _GNU_SOURCE // for syscall
#include "csi.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <pthread.h>

__thread GQueue *call_stack = NULL;
__thread GTree  *call_graph = NULL;
__thread int thread_initialized = 0;

pthread_mutex_t *cg_list_lock = NULL;
GList *cg_list = NULL;
pthread_mutex_t *cs_list_lock = NULL;
GList *cs_list = NULL;
pthread_mutex_t *cg_lock = NULL;
GTree *cg = NULL;

FILE *outfile = NULL;
int initialized = 0;

gint scmp(gpointer a, gpointer b) {
    const GString *sa = (const GString *)a;
    const GString *sb = (const GString *)b;
    assert(sa);
    assert(sb);
    return strcmp(sa->str, sb->str);
}

void threadinit() {
    if (thread_initialized) return;
    assert(!call_stack);
    assert(!call_graph);

    call_stack = g_queue_new();
    assert(call_stack);
    call_graph = g_tree_new((GCompareFunc)scmp);
    assert(call_graph);

    assert(cg_list_lock);
    pthread_mutex_lock(cg_list_lock);
    cg_list = g_list_prepend(cg_list, call_graph);
    pthread_mutex_unlock(cg_list_lock);

    assert(cs_list_lock);
    pthread_mutex_lock(cs_list_lock);
    cs_list = g_list_prepend(cs_list, call_stack);
    pthread_mutex_unlock(cs_list_lock);

    thread_initialized = 1;
}

void push_func(GString *name) {
    threadinit();
    assert(call_stack);
    g_queue_push_tail(call_stack, (void *)name);
}

void pop_func() {
    threadinit();
    assert(call_stack);
    g_queue_pop_tail(call_stack);
}

gboolean my_call_stack_empty() {
    gboolean result;
    assert(call_stack);
    result = g_queue_is_empty(call_stack);
    return result;
}

GString *my_call_stack_top() {
    GString *result = NULL;
    assert(call_stack);
    result = (GString *)g_queue_peek_tail(call_stack);
    return result;
}

void add_call_edge(GString *f, GString *g) {
    threadinit();
    assert(call_graph);
    assert(f);
    assert(g);
    //printf("Add edge %s (%lu) -> %s (%lu)\n", f, strlen(f), g, strlen(g));
    GSequence *value = g_tree_lookup(call_graph, f);
    //printf("Added edge %s -> %s\n", f, g);
    if (value == NULL) {
        value = g_sequence_new(NULL);
        g_sequence_insert_sorted(value, (void *)g, (GCompareDataFunc)scmp, NULL);
        g_tree_insert(call_graph, (void *)f, value);
    }
    if (g_sequence_lookup(value, (void *)g, (GCompareDataFunc)scmp, NULL) == NULL) {
        g_sequence_insert_sorted(value, (void *)g, (GCompareDataFunc)scmp, NULL);
    }
}

void print_func(gpointer value, gpointer data) {
    assert(value);
    assert(outfile);
    fprintf(outfile, "    %s\n", ((const GString *)value)->str);
}

gboolean print_cg(gpointer key, gpointer value, gpointer data) {
    assert(outfile);
    fprintf(outfile, "Function %s calls:\n", ((const GString *)key)->str);
    GSequence *strings = (GSequence *)value;
    assert(strings);
    g_sequence_foreach(strings, print_func, NULL);
    return FALSE;
}

void init_cg() {
    assert(initialized == 0);

    cg_list_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(cg_list_lock, NULL);

    cs_list_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(cs_list_lock, NULL);

    cg_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(cg_lock, NULL);

    initialized = 1;
}

gboolean all_stacks_empty() {
    gboolean result = TRUE;
    pthread_mutex_lock(cs_list_lock);
    assert(cs_list);

    GList *iter = g_list_first(cs_list);
    while (iter) {
        GQueue *stack = (GQueue*)iter->data;
        assert(stack);
        if (!g_queue_is_empty(stack)) {
            result = FALSE;
            break;
        }
        iter = iter->next;
    }

    pthread_mutex_unlock(cs_list_lock);
    return result;
}

gboolean merge_cg(gpointer key, gpointer value, gpointer data) {
    assert(cg);
    GSequence *calls = g_tree_lookup(cg, key);
    GSequence *newcalls = (GSequence*)value;
    if (calls == NULL) {
        g_tree_insert(cg, key, newcalls);
    } else {
        GSequenceIter *iter = g_sequence_get_begin_iter(newcalls);
        while (!g_sequence_iter_is_end(iter)) {
            GString *f = (GString*)g_sequence_get(iter);
            if (g_sequence_lookup(calls, f, (GCompareDataFunc)scmp, NULL) == NULL) {
                g_sequence_insert_sorted(calls, f, (GCompareDataFunc)scmp, NULL);
            }
            iter = g_sequence_iter_next(iter);
        }
    }
    return FALSE;
}

// CallGraph in cg_list -> cg
void merge_call_graphs() {
    assert(cg);
    assert(cg_list);
    pthread_mutex_lock(cg_list_lock);
    GList *iter = g_list_first(cg_list);
    while (iter) {
        GTree *callgraph = (GTree*)iter->data;
        assert(callgraph);
        g_tree_foreach(callgraph, merge_cg, NULL);
        iter = iter->next;
    }

    pthread_mutex_unlock(cg_list_lock);
}

void destroy() {
    assert(call_graph);

    int fd = open("/tmp/callgraph.csi", O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        printf("Failed to open file: %s\n", strerror(errno));
        fflush(stdout);
        assert(0);
    }
    outfile = fdopen(fd, "w");
    if (!outfile) {
        printf("Failed to open file: %s\n", strerror(errno));
        fflush(stdout);
    }
    assert(outfile);

    assert(!cg);
    cg = g_tree_new((GCompareFunc)scmp);
    assert(cg);

    pthread_mutex_lock(cg_lock);
    merge_call_graphs();
    g_tree_foreach(cg, print_cg, NULL);
    pthread_mutex_unlock(cg_lock);
    fclose(outfile);
    close(fd);
}

/* void __csi_init(csi_info_t info) { */
void __csi_init(uint32_t num_modules) {
    if (initialized == 0) {
        atexit(destroy);
        init_cg();
    }
}

void __csi_func_entry(void *function, void *parentReturnAddr, char *funcName) {
    threadinit();
    GString *gs = g_string_new(funcName);
    if (!my_call_stack_empty()) {
        GString *f = my_call_stack_top();
        add_call_edge(f, gs);
    }
    push_func(gs);
}

void __csi_func_exit() {
    pop_func();
}

// Unused functions
/* WEAK void __csi_module_init(uint32_t module_id, uint64_t num_basic_blocks) {} */
/* WEAK void __csi_before_load(void *addr, int num_bytes, int attr) {} */
/* WEAK void __csi_after_load(void *addr, int num_bytes, int attr) {} */
/* WEAK void __csi_before_store(void *addr, int num_bytes, int attr) {} */
/* WEAK void __csi_after_store(void *addr, int num_bytes, int attr) {} */
/* WEAK void __csi_bb_entry(uint32_t module_id, uint64_t id) {} */
/* WEAK void __csi_bb_exit() {} */

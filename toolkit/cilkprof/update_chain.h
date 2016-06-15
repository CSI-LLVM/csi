#ifndef INCLUDED_UPDATE_CHAIN_H
#define INCLUDED_UPDATE_CHAIN_H

#include <inttypes.h>

typedef struct update_t {
  uint64_t measurement;
  struct update_t *next;
} update_t;

#endif

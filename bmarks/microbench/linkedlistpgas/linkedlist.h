/*
 *  linkedlist.h
 *  
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "tm.h"

/*______________________________________________________________________________
 * SETTINGS
 * _____________________________________________________________________________
 */
#define SEQUENTIAL_
#define LOCKS_
#define STM
#define EARLY_RELEASE_
#define READ_VALIDATION_

#ifdef READ_VALIDATION
#ifdef EARLY_RELEASE
#undef EARLY_RELEASE
#endif
#endif

#define DEFAULT_DURATION                10
#define DEFAULT_INITIAL                 256
#define DEFAULT_RANGE                   VAL_MAX
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY		4
#define DEFAULT_ALTERNATE		0
#define DEFAULT_EFFECTIVE		1
#define DEFAULT_LOCKS                   0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define TRANSACTIONAL                   d->unit_tx

typedef unsigned short int val_t;
typedef unsigned short int nxt_t;

#define VAL_MIN                         0
#define VAL_MAX                         USHRT_MAX

typedef union {
    int toint;
    struct {
        val_t val;
        nxt_t next;
    };
} node_t;

typedef struct intset {
    nxt_t head;

} intset_t;

#define N2O(set, node)                  (nxt_t) ((nxt_t) (node) - (nxt_t) (set))
#define O2N(set, offset)                ((void *) (offset) == NULL ? NULL : (node_t *) ((nxt_t) (set) + (offset)))
#define SET                             set
#define ND(offs)                        O2N(SET, (offs))
#define OF(node)                        N2O(SET, (node))

void *shmem_init(size_t offset);

pgas_addr_t new_node(val_t val, nxt_t next, int transactional);
intset_t *set_new();
void set_delete(intset_t *set);
int set_size(intset_t *set);

/*intset.c*/
int set_contains(intset_t *set, val_t val, int transactional);
int set_add(intset_t *set, val_t val, int transactional);
int set_remove(intset_t *set, val_t val, int transactional);

void set_print(intset_t *set);

inline void global_lock() {
    PRINTD("askking for global lock");
    RCCE_acquire_lock(0);
    PRINTD("got global lock");
}

inline void global_lock_release() {
    RCCE_release_lock(0);
    PRINTD("released global lock");
}
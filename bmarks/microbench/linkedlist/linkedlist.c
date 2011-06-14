/*
 *  linkedlist.c
 *  
 *  Linked list data structure
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include "linkedlist.h"


#define STM

//#define SEQUENTIAL

node_t *new_node(val_t val, nxt_t next, int transactional) {
    node_t *node;

    if (transactional) {
        node = (node_t *) TX_SHMALLOC(sizeof (node_t));
    }
    else {
        node = (node_t *) RCCE_shmalloc(sizeof (node_t));
    }
    if (node == NULL) {
        perror("malloc");
        EXIT(1);
    }

    node->val = val;
    node->next = next;

    return node;
}

intset_t *set_new() {
    intset_t *set;
    node_t *min, *max;

    if ((set = (intset_t *) RCCE_shmalloc(sizeof (intset_t))) == NULL) {
        perror("malloc");
        EXIT(1);
    }
    max = new_node(VAL_MAX, NULL, 0);
    min = new_node(VAL_MIN, N2O(set, max), 0);
    set->head = min;

    return set;
}

void set_delete(intset_t *set) {
    node_t *node, *next;

    node = set->head;
    while (node != NULL) {
        next = O2N(set, node->next);
        RCCE_shfree((t_vcharp) node);
        node = next;
    }
    RCCE_shfree((t_vcharp) set);
}

int set_size(intset_t *set) {
    int size = 0;
    node_t *node;

    /* We have at least 2 elements */
    node = O2N(set, set->head->next);
    while (node->nextp != NULL) {
        size++;
        node = O2N(set, node->next);
    }

    return size;
}


/*
 *  intset.c
 *  
 *  Integer set operations (contain, insert, delete) 
 *  that call stm-based / lock-free counterparts.
 *
 *  Created by Vincent Gramoli on 1/12/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

extern stm_tx_t *stm_tx = NULL;
extern stm_tx_node_t *stm_tx_node = NULL;

#define SET             set
#define ND(offs)        O2N(SET, (offs))
#define OF(node)        N2O(SET, (node))

int set_contains(intset_t *set, val_t val, int transactional) {
    int result;

#ifdef DEBUG
    printf("++> set_contains(%d)\n", (int) val);
    FLUSH;
#endif

#ifdef SEQUENTIAL
    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val == val);

#elif defined STM
    PRINT("contains STM");

    node_t *prev, *next;
    val_t v = 0;

    TX_START
    prev = set->head;
    next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    while (1) {
        v = *(val_t *) TX_LOAD(&next->val);
        if (v >= val)
            break;
        prev = next;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    }
    TX_COMMIT
    result = (v == val);

#elif defined LOCKFREE			
    result = harris_find(set, val);
#endif	

    return result;
}

inline int set_seq_add(intset_t *set, val_t val) {
    int result;
    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val != val);
    if (result) {
        prev->next = OF(new_node(val, OF(next), 0));
    }
    return result;
}

int set_add(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_add(%d)\n", (int) val);
    FLUSH;
#endif

    if (!transactional) {

        result = set_seq_add(set, val);

    }
    else {

#ifdef SEQUENTIAL /* Unprotected */

        result = set_seq_add(set, val);

#elif defined STM
        PRINT("add STM");

        node_t *prev, *next;
        val_t v;
        TX_START
        prev = set->head;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
        while (1) {
            v = *(val_t *) TX_LOAD(&next->val);
            if (v >= val)
                break;
            prev = next;
            next = ND(*(nxt_t *) TX_LOAD(&prev->next));
        }
        result = (v != val);
        if (result) {
            nxt_t nxt = OF(new_node(val, OF(next), transactional));
            TX_STORE(&prev->next, &nxt, TYPE_UINT);
        }
        TX_COMMIT

#elif defined LOCKFREE
        result = harris_insert(set, val);
#endif

    }

    return result;
}

int set_remove(intset_t *set, val_t val, int transactional) {
    int result = 0;

#ifdef DEBUG
    printf("++> set_remove(%d)\n", (int) val);
    FLUSH;
#endif

#ifdef SEQUENTIAL /* Unprotected */

    node_t *prev, *next;

    prev = set->head;
    next = ND(prev->next);
    while (next->val < val) {
        prev = next;
        next = ND(prev->next);
    }
    result = (next->val == val);
    if (result) {
        node_t * t = ND(prev->next);
        t = ND(next->next);
        RCCE_shfree((t_vcharp) next);
    }

#elif defined STM
    PRINT("remove STM");

    node_t *prev, *next;
    val_t v;
    node_t *n;
    TX_START
    prev = set->head;
    next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    while (1) {
        v = *(val_t *) TX_LOAD(&next->val);
        if (v >= val)
            break;
        prev = next;
        next = ND(*(nxt_t *) TX_LOAD(&prev->next));
    }
    result = (v == val);
    if (result) {
        n = ND(*(nxt_t *) TX_LOAD(&next->next));
        TX_STORE(&prev->next, OF(n), TYPE_UINT);
        TX_SHFREE(next);
    }
    TX_COMMIT

#elif defined LOCKFREE
    result = harris_delete(set, val);
#endif

    return result;
}

void set_print(intset_t* set) {
    node_t *node = ND(set->head->next);

    if (node == NULL) {
        goto null;
    }
    while (node->nextp != NULL) {
        printf("%d -> ", node->val);
        node = ND(node->next);
    }

null:
    PRINTSF("NULL\n");

}
/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses of the linked list
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Microbench
 * 
 * Microbench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "linkedlist.h"
#include <unistd.h>

#ifdef SEQUENTIAL
#ifdef BARRIER
#undef BARRIER
#define BARRIER BARRIERW
#endif
#endif

/* ################################################################### *
 * RANDOM
 * ################################################################### */

/* Re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
    int m = RAND_MAX;
    long d, v = 0;

    do {
        d = (m > r ? r : m);
        v += 1 + (long) (d * ((double) rand_r(seed) / ((double) (m) + 1.0)));
        r -= m;
    } while (r > 0);
    return v;
}

typedef struct thread_data {
    val_t first;
    long range;
    int update;
    int unit_tx;
    int alternate;
    int effective;
    unsigned long nb_add;
    unsigned long nb_added;
    unsigned long nb_remove;
    unsigned long nb_removed;
    unsigned long nb_contains;
    unsigned long nb_found;
    unsigned int seed;
    intset_t *set;
    unsigned long failures_because_contention;
} thread_data_t;

void *test(void *data, double duration) {
    int unext, last = -1;
    val_t val = 0;

    thread_data_t *d = (thread_data_t *) data;

    srand_core();

    /* Create transaction */

    /* Is the first op an update? */
    unext = (rand_range(100) - 1 < d->update);

    FOR(duration) {
        if (unext) { // update

            if (last < 0) { // add

                val = rand_range_re(&d->seed, d->range);
                if (set_add(d->set, val, TRANSACTIONAL)) {
                    d->nb_added++;
                    last = val;
                }
                d->nb_add++;

            }
            else { // remove

                if (d->alternate) { // alternate mode (default)
                    if (set_remove(d->set, last, TRANSACTIONAL)) {
                        d->nb_removed++;
                    }
                    last = -1;
                }
                else {
                    /* Random computation only in non-alternated cases */
                    val = rand_range_re(&d->seed, d->range);
                    /* Remove one random value */
                    if (set_remove(d->set, val, TRANSACTIONAL)) {
                        d->nb_removed++;
                        /* Repeat until successful, to avoid size variations */
                        last = -1;
                    }
                }
                d->nb_remove++;
            }
        }
        else { // read

            if (d->alternate) {
                if (d->update == 0) {
                    if (last < 0) {
                        val = d->first;
                        last = val;
                    }
                    else { // last >= 0
                        val = rand_range_re(&d->seed, d->range);
                        last = -1;
                    }
                }
                else { // update != 0
                    if (last < 0) {
                        val = rand_range_re(&d->seed, d->range);
                        //last = val;
                    }
                    else {
                        val = last;
                    }
                }
            }
            else val = rand_range_re(&d->seed, d->range);

            if (set_contains(d->set, val, TRANSACTIONAL))
                d->nb_found++;
            d->nb_contains++;

        }

        /* Is the next op an update? */
        if (d->effective) { // a failed remove/add is a read-only tx
            unext = ((100 * (d->nb_added + d->nb_removed))
                    < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
        }
        else { // remove/add (even failed) is considered as an update
            unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
        }
    }

    return NULL;
}

TASKMAIN(int argc, char **argv) {
    dup2(STDOUT_FILENO, STDERR_FILENO);
#ifndef SEQUENTIAL
    TM_INIT
#else
    RCCE_init(&argc, &argv);
    iRCCE_init();
#endif

    struct option long_options[] = {
        // These options don't set a flag
        {"help", no_argument, NULL, 'h'},
        {"duration", required_argument, NULL, 'd'},
        {"initial-size", required_argument, NULL, 'i'},
        {"range", required_argument, NULL, 'r'},
        {"update-rate", required_argument, NULL, 'u'},
        {"elasticity", required_argument, NULL, 'x'},
        {"effective", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

    intset_t *set;
    int i, c, size;
    val_t last = 0;
    val_t val = 0;
    thread_data_t *data;
    double duration = DEFAULT_DURATION;
    int initial = DEFAULT_INITIAL;
    int nb_app_cores = NUM_APP_NODES;
    long range = DEFAULT_RANGE;
    int update = DEFAULT_UPDATE;
    int unit_tx = DEFAULT_ELASTICITY;
    int alternate = DEFAULT_ALTERNATE;
    int effective = DEFAULT_EFFECTIVE;
    unsigned int seed = 0;

    while (1) {
        i = 0;
        c = getopt_long(argc, argv, "hAf:d:i:r:u:x:", long_options, &i);

        if (c == -1)
            break;

        if (c == 0 && long_options[i].flag == 0)
            c = long_options[i].val;

        switch (c) {
            case 0:
                /* Flag is automatically set */
                break;
            case 'h':
                ONCE
            {
                printf("intset -- STM stress test "
                        "(linked list)\n"
                        "\n"
                        "Usage:\n"
                        "  intset [options...]\n"
                        "\n"
                        "Options:\n"
                        "  -h, --help\n"
                        "        Print this message\n"
                        "  -A, --alternate (default="XSTR(DEFAULT_ALTERNATE)")\n"
                        "        Consecutive insert/remove target the same value\n"
                        "  -f, --effective <int>\n"
                        "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
                        "  -d, --duration secs<double>\n"
                        "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
                        "  -i, --initial-size <int>\n"
                        "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
                        "  -r, --range <int>\n"
                        "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
                        "  -u, --update-rate <int>\n"
                        "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
                        "  -x, --elasticity (default=4)\n"
                        "        Use elastic transactions\n"
                        "        0 = non-protected,\n"
                        "        1 = normal transaction,\n"
                        "        2 = read elastic-tx,\n"
                        "        3 = read/add elastic-tx,\n"
                        "        4 = read/add/rem elastic-tx,\n"
                        "        5 = all recursive elastic-tx,\n"
                        "        6 = harris lock-free\n"
                        );
            }
                exit(0);
            case 'A':
                alternate = 1;
                break;
            case 'f':
                effective = atoi(optarg);
                break;
            case 'd':
                duration = atof(optarg);
                break;
            case 'i':
                initial = atoi(optarg);
                break;
            case 'r':
                range = atol(optarg);
                break;
            case 'u':
                update = atoi(optarg);
                break;
            case 'x':
                unit_tx = atoi(optarg);
                break;
            case '?':
                ONCE
            {
                printf("Use -h or --help for help\n");
            }
                exit(0);
            default:
                exit(1);
        }
    }

    if (seed == 0) {
        srand_core();
        seed = rand_range((ID + 17) * 123);
        srand(seed);
    }
    else
        srand(seed);

    assert(duration >= 0);
    assert(initial >= 0);
    assert(nb_app_cores > 0);
    assert(range > 0 && range >= initial);
    assert(update >= 0 && update <= 100);

    ONCE
    {
        printf("Bench type   : linked list PGAS\n");
#ifdef SEQUENTIAL
        printf("                sequential\n");
#elif defined(EARLY_RELEASE )
        printf("                using early-release\n");
#elif defined(READ_VALIDATION)
        printf("                using read-validation\n");
#endif
#ifdef LOCKS
        printf("                  with locks\n");
#endif
        printf("Duration     : %f\n", duration);
        printf("Initial size : %d\n", initial);
        printf("Nb cores     : %d\n", nb_app_cores);
        printf("Value range  : %ld\n", range);
        printf("Update rate  : %d\n", update);
        printf("Elasticity   : %d\n", unit_tx);
        printf("Alternate    : %d\n", alternate);
        printf("Effective    : %d\n", effective);
        FLUSH
    }

    if ((data = (thread_data_t *) malloc(sizeof (thread_data_t))) == NULL) {
        perror("malloc");
        exit(1);
    }

    shmem_init(8);      //allow NULL to be 0
    set = set_new();

    BARRIER;

    ONCE
    {
        /* Populate set */
        printf("Adding %d entries to set\n", initial);
        i = 0;
        while (i < initial) {
            val = rand_range(range);
            if (set_add(set, val, 0)) {
                last = val;
                i++;
            }
        }
        size = set_size(set);
        printf("Set size     : %d\n", size);

/*
        set_print(set);
*/

        assert(size == initial);
        FLUSH
    }
    
    
#if defined(PLATFORM_iRCCE)
    int off, id2use;
    if (ID < 6) {
        off = 0;
        id2use = ID;
    }
    else if (ID < 12) {
        off = 1;
        id2use = ID - 6;
    }
    else if (ID < 18) {
        off = 0;
        id2use = ID - 6;
    }
    else if (ID < 24) {
        off = 1;
        id2use = ID - 12;
    }
    else if (ID < 30) {
        off = 2;
        id2use = ID - 24;
    }
    else if (ID < 36) {
        off = 3;
        id2use = ID - 30;
    }
    else if (ID < 42) {
        off = 2;
        id2use = ID - 30;
    }
    else if (ID < 48) {
        off = 3;
        id2use = ID - 36;
    }

    shmem_init(((off * 16) * 1024 * 1024) + ((id2use / 2) * 1024 * 1024));
    PRINT("shmem from %d MB", (off * 16) + id2use / 2);

#else
    shmem_init(1024 * 100 * ID * sizeof (node_t) + ((initial + 2) * sizeof (node_t)));

#endif

    /* Access set from all threads */
    data->first = last;
    data->range = range;
    data->update = update;
    data->unit_tx = unit_tx;
    data->alternate = alternate;
    data->effective = effective;
    data->nb_add = 0;
    data->nb_added = 0;
    data->nb_remove = 0;
    data->nb_removed = 0;
    data->nb_contains = 0;
    data->nb_found = 0;
    data->set = set;
    data->seed = seed;

    BARRIER
    /* Start */
            
    test(data, duration);

    printf("-- Core %d\n", NODE_ID());
    printf("  #add        : %lu\n", data->nb_add);
    printf("    #added    : %lu\n", data->nb_added);
    printf("  #remove     : %lu\n", data->nb_remove);
    printf("    #removed  : %lu\n", data->nb_removed);
    printf("  #contains   : %lu\n", data->nb_contains);
    printf("    #found    : %lu\n", data->nb_found);
    printf("---------------------------------------------------");
    FLUSH;
    /* Delete set */


    BARRIER

#ifdef SEQUENTIAL
    int total_ops = data->nb_add + data->nb_contains + data->nb_remove;
    printf("#Ops          : %d\n", total_ops);
    printf("#Ops/s        : %d\n", (int) (total_ops / duration__));
    printf("#Latency      : %f\n", duration__ / total_ops);
    FLUSH
#endif

    //set_delete(set);

    /* Cleanup STM */

    free(data);

    BARRIER

#ifdef STM
      TM_END
#endif

    EXIT(0);
}

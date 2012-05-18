/*
 * Testing the message passing latencies
 */

#include <assert.h>

#ifndef PGAS
#error "Need PGAS defined for the mp latencies benchmark"
#endif

#include "tm.h"

#ifdef PLATFORM_TILERA
#include <arch/cycle.h>
#define getticks get_cycle_count
#endif

#define REPS 1000000
typedef long long int ticks;

MAIN(int argc, char **argv) {

  long long int steps = REPS;

    TM_INIT

    if (argc > 1) {
        steps = atoll(argv[1]);
    }



    int *sm = (int *) sys_shmalloc(steps * sizeof (int));

    ONCE {
      PRINT("## sending %lld messages", steps);
    }
    BARRIER

      long long int rounds, sum = 0;

    double _start = wtime();
    ticks _start_ticks = getticks();


    for (rounds = 0; rounds < steps; rounds++) {
      sum += (int) NONTX_LOAD(sm + rounds);
    }
     

    BARRIER;

    ticks _end_ticks = getticks();
    double _end = wtime();

    double _time = _end - _start;
    ticks _ticks = _end_ticks - _start_ticks;
    ticks _ticksm =(ticks) ((double)_ticks / steps);
    double lat = (double) (1000*1000*1000*_time) / steps;
    printf("sent %lld msgs\n\t"
	   "in %f secs\n\t"
	   "%.2f msgs/us\n\t"
	   "%f ns latency\n"
	   "in ticks:\n\t"
	   "in %lld ticks\n\t"
	   "%lld ticks/msg\n", steps, _time, ((double)steps/(1000*1000*_time)), lat,
	   _ticks, _ticksm);


    PRINT("sum -- %lld", sum);

    TM_END
    EXIT(0);
}
#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <assert.h>
#include <ssmp.h>


#include "common.h"
#include "pubSubTM.h"
#include "dslock.h"
#include "pgas.h"
#include "mcore_malloc.h"

PS_REPLY* ps_remote_msg; // holds the received msg

INLINED void sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response);
/*
 * For cluster conf, we need a different kind of command line parameters.
 * Since there is no way for a node to know it's identity, we need to pass it,
 * along with the total number of nodes.
 * To make sure we don't rely on any particular order, params should be passed
 * as: -id=ID -total=TOTAL_NODES
 */
nodeid_t MY_NODE_ID;
nodeid_t MY_TOTAL_NODES;

void
sys_init_system(int* argc, char** argv[])
{


	if (*argc < 2) {
		fprintf(stderr, "Not enough parameters (%d)\n", *argc);
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", (*argv)[0]);
		EXIT(1);
	}

	int p = 1;
	int found = 0;
	while (p < *argc) {
		if (strncmp("-total=", (*argv)[p], strlen("-total=")) == 0) {

			char *cf = (*argv)[p] + strlen("-total=");
			MY_TOTAL_NODES = atoi(cf);
			(*argv)[p] = NULL;
			found = 1;
		}
		p++;
	}
	if (!found) {
		fprintf(stderr, "Did not pass all parameters\n");
		fprintf(stderr, "Call this program as:\n");
		fprintf(stderr, "\t%s -total=TOTAL_NODES ...\n", (*argv)[0]);
		EXIT(1);
	}
	p = 1;
	int cur = 1;
	while (p < *argc) {
		if ((*argv)[p] == NULL) {
			p++;
			continue;
		}
		(*argv)[cur] = (*argv)[p];
		cur++;
		p++;
	}
	*argc = *argc - (p-cur);

	MY_NODE_ID = 0;

	ssmp_init(MY_TOTAL_NODES);

	nodeid_t rank;
	for (rank = 1; rank < MY_TOTAL_NODES; rank++) {
		PRINTD("Forking child %u", rank);
		pid_t child = fork();
		if (child < 0) {
			PRINT("Failure in fork():\n%s", strerror(errno));
		} else if (child == 0) {
			goto fork_done;
		}
	}
	rank = 0;

fork_done:
	PRINTD("Initializing child %u", rank);
	MY_NODE_ID = rank;
	ssmp_mem_init(MY_NODE_ID, MY_TOTAL_NODES);

	// Now, pin the process to the right core (NODE_ID == core id)
	int place;
	if (rank%2 != 0) {
		place = MY_TOTAL_NODES/2+rank/2;
	} else {
		place = rank/2;
	}
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(place, &mask);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
		PRINT("Problem with setting processor affinity: %s\n",
			  strerror(errno));
		EXIT(3);
	}
}

void
term_system()
{
  ssmp_term();
}

sys_t_vcharp
sys_shmalloc(size_t size)
{
#ifdef PGAS
	return fakemem_malloc(size);
#else
	return MCORE_shmalloc(size);
#endif
}

void
sys_shfree(sys_t_vcharp ptr)
{
	MCORE_shfree(ptr);
}

void
sys_tm_init()
{
}

void
sys_ps_init_(void)
{
	BARRIERW
	
	MCORE_shmalloc_init(1024*1024*1024); //1GB

	ps_remote_msg = NULL;
	PRINTD("sys_ps_init: done");
}

void
sys_dsl_init(void)
{
	BARRIERW

}

void
sys_dsl_term(void)
{

}

void
sys_ps_term(void)
{

}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
INLINED void 
sys_ps_command_reply(nodeid_t sender,
                    PS_REPLY_TYPE command,
                    tm_addr_t address,
                    uint32_t* value,
                    CONFLICT_TYPE response)
{
#ifndef PGAS
  ssmp_send1(sender, response);
#else //PGAS
  if (command == PS_SUBSCRIBE_RESPONSE) {
    ssmp_send2(sender, response, *(int *) value);
  }
  else {
    ssmp_send1(sender, response);
  }
#endif
}


void
dsl_communication()
{
  int sender;
  ssmp_msg_t msg;
  PS_COMMAND_TYPE command;
  unsigned int address;

  while (1) {

    ssmp_recv6(&msg);
    sender = msg.sender;
    //    PRINT("recved msg from %d", sender);

    command = msg.w0;

    switch (command) {
    case PS_SUBSCRIBE:
      address = msg.w1;
#ifdef DEBUG_UTILIZATION
      read_reqs_num++;
#endif

#ifdef PGAS
      /*
	PRINT("RL addr: %3d, val: %d", address, PGAS_read(address));
      */
      sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE,
			  address, 
			  PGAS_read(address),
			  try_subscribe(sender, address));
#else
      sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, 
			  address, 
			  NULL,
			  try_subscribe(sender, address));
      //sys_ps_command_reply(sender, PS_SUBSCRIBE_RESPONSE, address, NO_CONFLICT);
#endif
      break;
    case PS_PUBLISH:
      {
	address = msg.w1;

#ifdef DEBUG_UTILIZATION
	write_reqs_num++;
#endif

	CONFLICT_TYPE conflict = try_publish(sender, address);
#ifdef PGAS
	int write_value = msg.w2;
	if (conflict == NO_CONFLICT) {
	  /*
	    union {
	    int i;
	    unsigned short s[2];
	    } convert;
	    convert.i = write_value;
	    PRINT("\t\t\tWriting (val:%d|nxt:%d) to address %d", convert.s[0], convert.s[1], address);
	  */
	  write_set_pgas_insert(PGAS_write_sets[sender],
				write_value, 
				address);
	}
#endif
	sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE, 
			    address,
			    NULL,
			    conflict);
	break;
      }
#ifdef PGAS
    case PS_WRITE_INC:
      {
	address = msg.w1;
	int write_value = msg.w2;

#ifdef DEBUG_UTILIZATION
	write_reqs_num++;
#endif
	CONFLICT_TYPE conflict = try_publish(sender, address);
	if (conflict == NO_CONFLICT) {
	  //		      PRINT("wval for %d is %d", address, write_value);
	  /*
	    PRINT("PS_WRITE_INC from %2d for %3d, old: %3d, new: %d", sender, address, PGAS_read(address),
	    PGAS_read(address) + write_value);
	  */
	  write_set_pgas_insert(PGAS_write_sets[sender], 
				*(int *) PGAS_read(address) + write_value,
                                address);
	}
	sys_ps_command_reply(sender, PS_PUBLISH_RESPONSE,
			    address,
			    NULL,
			    conflict);
	break;
      }
    case PS_LOAD_NONTX:
      {
	address = msg.w1;
	//		PRINT("((non-tx ld: from %d, addr %d (val: %d)))", sender, address, (*PGAS_read(address)));
	ssmp_send2(sender, NO_CONFLICT, *(int *) PGAS_read(address));
	break;
      }
    case PS_STORE_NONTX:
      {
	address = msg.w1;
	int write_value = msg.w2;
	//		PRINT("((non-tx st: from %d, addr %d (val: %d)))", sender, address, (write_value));
	PGAS_write(address, (int) write_value);
	break;
      }
#endif
    case PS_REMOVE_NODE:
#ifdef PGAS	
      int response = msg.w1;
      if (response == NO_CONFLICT) {
	write_set_pgas_persist(PGAS_write_sets[sender]);
      }
      PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif
      ps_hashtable_delete_node(ps_hashtable, sender);
      break;
    case PS_UNSUBSCRIBE:
      address = msg.w1;
      ps_hashtable_delete(ps_hashtable, sender, address, READ);
      break;
    case PS_PUBLISH_FINISH:
      address = msg.w1;
      ps_hashtable_delete(ps_hashtable, sender, address, WRITE);
      break;
    case PS_STATS:
      {
	union {
	  int from[2];
	  double to;
	} convert;
	convert.from[0] = msg.w1;
	convert.from[1] = msg.w2;
	double duration = convert.to;

	if (duration) {
	  if (!ID) { PRINT("stats 1 from %d", sender); }
	  unsigned int aborts = msg.w3;
	  stats_aborts += aborts;
	  unsigned int commits = msg.w4;
	  stats_commits += commits;
	  stats_duration += duration;
	  unsigned int max_retries = msg.w5;
	  stats_max_retries = stats_max_retries < max_retries ? max_retries
	    : stats_max_retries;
	  stats_total += commits + aborts;
	}
	else {
	  stats_aborts_raw += msg.w3;
	  stats_aborts_war += msg.w4;
	  stats_aborts_waw += msg.w5;
	}

	if (++stats_received >= 2*NUM_APP_NODES) {
	  if (ID == 0) {
	    print_global_stats();

	    print_hashtable_usage();

	  }

#ifdef DEBUG_UTILIZATION
	  PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
#endif

	  EXIT(0);
	}
      }
    default:
      PRINTD("REMOTE MSG: ??");
    }
  }
}



/*
 * Seeding the rand()
 */
void
srand_core()
{
	double timed_ = wtime();
	unsigned int timeprfx_ = (unsigned int) timed_;
	unsigned int time_ = (unsigned int) ((timed_ - timeprfx_) * 1000000);
	srand(time_ + (13 * (ID + 1)));
}

void 
udelay(unsigned int micros)
{
   double __ts_end = wtime() + ((double) micros / 1000000);
   while (wtime() < __ts_end);
   //usleep(micros);
}

static nodeid_t num_tm_nodes;
static nodeid_t num_dsl_nodes;

volatile nodeid_t* aux;
volatile nodeid_t* apps;
volatile nodeid_t* apps2;
volatile nodeid_t* nodes;
volatile nodeid_t* nodes2;
volatile nodeid_t* auxNodes;

void
init_barrier()
{

}

void
app_barrier()
{

}

void
global_barrier()
{

}


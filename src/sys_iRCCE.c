#include "common.h"
#include "iRCCE.h"
#include "pubSubTM.h"
#include "dslock.h"

void 
init_system(int* argc, char** argv[])
{
	RCCE_init(argc, argv);
	iRCCE_init();
}

void
term_system()
{
	// noop
}

sys_t_vcharp
sys_shmalloc(size_t size)
{
	return RCCE_shmalloc(size);
}

void
sys_shfree(sys_t_vcharp ptr)
{
	RCCE_shfree(ptr);
}

static int color(int id, void *aux) {
    return !(id % DSLNDPERNODES);
}

RCCE_COMM RCCE_COMM_APP;

static iRCCE_RECV_REQUEST *recv_requests;
static iRCCE_RECV_REQUEST *recv_current;
static iRCCE_SEND_REQUEST *send_current;

static iRCCE_WAIT_LIST waitlist; //the send-recv buffer

#ifdef SENDLIST
static iRCCE_WAIT_LIST sendlist;
#endif

static char *buf;
static CONFLICT_TYPE ps_response; //TODO: make it more sophisticated
static PS_COMMAND *ps_command, *ps_remote, *psc;

void
sys_tm_init(unsigned int ID)
{
    RCCE_comm_split(color, NULL, &RCCE_COMM_APP);
}

void
sys_ps_init_(void)
{
    iRCCE_init_wait_list(&waitlist);

    RCCE_barrier(&RCCE_COMM_WORLD);
}

void
sys_dsl_init(void)
{
    iRCCE_init_wait_list(&waitlist);
    iRCCE_init_wait_list(&sendlist);

    recv_requests = (iRCCE_RECV_REQUEST*) calloc(NUM_UES, sizeof (iRCCE_RECV_REQUEST));
    if (recv_requests == NULL) {
        fprintf(stderr, "alloc");
        PRINTD("not able to alloc the recv_requests..");
        EXIT(-1);
    }

    ps_command = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    ps_remote = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null
    psc = (PS_COMMAND *) malloc(sizeof (PS_COMMAND)); //TODO: free at finalize + check for null

    if (ps_command == NULL || ps_remote == NULL || psc == NULL) {
        PRINTD("malloc ps_command == NULL || ps_remote == NULL || psc == NULL");
    }

    buf = (char *) malloc(NUM_UES * PS_BUFFER_SIZE); //TODO: free at finalize + check for null
    if (buf == NULL) {
        PRINTD("malloc || buff == NULL");
    }

    // Create recv request for each possible (other) core.
    unsigned int i;
    for (i = 0; i < NUM_UES; i++) {
        if (i % DSLNDPERNODES) { /*only for non DSL cores*/
            iRCCE_irecv(buf + i * PS_BUFFER_SIZE, PS_BUFFER_SIZE, i, &recv_requests[i]);
            iRCCE_add_recv_to_wait_list(&waitlist, &recv_requests[i]);
        }
    }

    RCCE_barrier(&RCCE_COMM_WORLD);
}

int
sys_sendcmd(void* data, size_t len, nodeid_t to)
{
	char buf[PS_BUFFER_SIZE];
	memcpy(buf, data, len);
	return (iRCCE_isend(buf, PS_BUFFER_SIZE, to, NULL) == iRCCE_SUCCESS);
}

int
sys_recvcmd(void* data, size_t len, nodeid_t from)
{
	int res = iRCCE_irecv(data, PS_BUFFER_SIZE, from, NULL);
	return (res == RCCE_SUCCESS);
}

// If value == NULL, we just return the address.
// Otherwise, we return the value.
static inline void 
sys_ps_command_send(unsigned short int target,
                    PS_COMMAND_TYPE command, 
                    tm_addr_t address, 
                    uint32_t* value, 
                    CONFLICT_TYPE response) 
{

    iRCCE_SEND_REQUEST *s = (iRCCE_SEND_REQUEST *) malloc(sizeof (iRCCE_SEND_REQUEST));
    if (s == NULL) {
        PRINTD("Could not allocate space for iRCCE_SEND_REQUEST");
        EXIT(-1);
    }
    psc->type = command;
#ifdef PGAS
    if (value != NULL) {
        psc->value = value;
    } else {
        psc->address = (uintptr_t)address;
    }
#else
    psc->address = (uintptr_t)address;
#endif
    psc->response = response;

    char *data = (char *) malloc(PS_BUFFER_SIZE * sizeof (char));
    if (data == NULL) {
        PRINTD("Could not allocate space for data");
        EXIT(-1);
    }

    memcpy(data, psc, sizeof (PS_COMMAND));
    if (iRCCE_isend(data, PS_BUFFER_SIZE, target, s) != iRCCE_SUCCESS) {
        iRCCE_add_send_to_wait_list(&sendlist, s);
        //iRCCE_add_to_wait_list(&sendlist, s, NULL);
    }
    else {
        free(s);
        free(data);
    }
}


void dsl_communication() {
    int sender;
    char *base;

    while (1) {

        iRCCE_test_any(&sendlist, &send_current, NULL);
        if (send_current != NULL) {
            free(send_current->privbuf);
            free(send_current);
            continue;
        }
        //test if any send or recv completed
        iRCCE_test_any(&waitlist, NULL, &recv_current);
        if (recv_current != NULL) {


            //the sender of the message
            sender = recv_current->source;
            base = buf + sender * PS_BUFFER_SIZE;
            ps_remote = (PS_COMMAND *) base;

            switch (ps_remote->type) {
                case PS_SUBSCRIBE:

#ifdef DEBUG_UTILIZATION
                    read_reqs_num++;
#endif

#ifdef PGAS
/*
                    PRINT("RL addr: %3d, val: %d", ps_remote->address, PGAS_read(ps_remote->address));
*/
                    sys_ps_command_send(sender, PS_SUBSCRIBE_RESPONSE,
                    		(tm_addr_t)ps_remote->address, 
                    		PGAS_read(ps_remote->address),
							try_subscribe(sender, (tm_addr_t)ps_remote->address));
#else
                    sys_ps_command_send(sender, PS_SUBSCRIBE_RESPONSE, 
                    		(tm_addr_t)ps_remote->address, 
                    		NULL,
                    		try_subscribe(sender, (tm_addr_t)ps_remote->address));
                    //sys_ps_command_send(sender, PS_SUBSCRIBE_RESPONSE, ps_remote->address, NO_CONFLICT);
#endif
                    break;
                case PS_PUBLISH:
                {

#ifdef DEBUG_UTILIZATION
                    write_reqs_num++;
#endif

                    CONFLICT_TYPE conflict = try_publish(sender, (tm_addr_t)ps_remote->address);
#ifdef PGAS
                    if (conflict == NO_CONFLICT) {
                        /*
                                                union {
                                                    int i;
                                                    unsigned short s[2];
                                                } convert;
                                                convert.i = ps_remote->write_value;
                                                PRINT("\t\t\tWriting (val:%d|nxt:%d) to address %d", convert.s[0], convert.s[1], ps_remote->address);
                         */
                        write_set_pgas_insert(PGAS_write_sets[sender], ps_remote->write_value, (tm_addr_t)(ps_remote->address));
                    }
#endif
                    sys_ps_command_send(sender, PS_PUBLISH_RESPONSE, 
							(tm_addr_t)(ps_remote->address),
							NULL,
							conflict);
                    break;
                }
#ifdef PGAS
                case PS_WRITE_INC:
                {

#ifdef DEBUG_UTILIZATION
                    write_reqs_num++;
#endif
                    CONFLICT_TYPE conflict = try_publish(sender, (tm_addr_t)ps_remote->address);
                    if (conflict == NO_CONFLICT) {
                        /*
                                                PRINT("PS_WRITE_INC from %2d for %3d, old: %3d, new: %d", sender, ps_remote->address, PGAS_read(ps_remote->address),
                                                        PGAS_read(ps_remote->address) + ps_remote->write_value);
                         */
                        write_set_pgas_insert(PGAS_write_sets[sender], 
                        		*PGAS_read(ps_remote->address) + ps_remote->write_value,
                                (tm_addr_t)ps_remote->address);
                    }
                    sys_ps_command_send(sender, PS_PUBLISH_RESPONSE,
                    		(tm_addr_t)ps_remote->address,
                    		NULL,
                    		conflict);
                    break;
                }
#endif
                case PS_REMOVE_NODE:
#ifdef PGAS
                    if (ps_remote->response == NO_CONFLICT) {
                        write_set_pgas_persist(PGAS_write_sets[sender]);
                    }
                    PGAS_write_sets[sender] = write_set_pgas_empty(PGAS_write_sets[sender]);
#endif
                    ps_hashtable_delete_node(ps_hashtable, sender);
                    break;
                case PS_UNSUBSCRIBE:
                    ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, READ);
                    break;
                case PS_PUBLISH_FINISH:
                    ps_hashtable_delete(ps_hashtable, sender, ps_remote->address, WRITE);
                    break;
                case PS_STATS:
                    stats_aborts += ps_remote->aborts;
                    stats_aborts_raw += ps_remote->aborts_raw;
                    stats_aborts_war += ps_remote->aborts_war;
                    stats_aborts_waw += ps_remote->aborts_waw;
                    stats_commits += ps_remote->commits;
                    stats_duration += ps_remote->tx_duration;
                    stats_max_retries = stats_max_retries < ps_remote->max_retries ? ps_remote->max_retries : stats_max_retries;
                    stats_total += ps_remote->commits + ps_remote->aborts;

                    if (++stats_received >= NUM_UES_APP) {
                        if (RCCE_ue() == 0) {
                            print_global_stats();

                            print_hashtable_usage();

                        }

#ifdef DEBUG_UTILIZATION
                        PRINT("*** Completed requests: %d", read_reqs_num + write_reqs_num);
#endif

                                EXIT(0);
                    }
                default:
                    PRINTD("REMOTE MSG: ??");
            }

            // Create request for new message from this core, add to waitlist
            iRCCE_irecv(base, PS_BUFFER_SIZE, sender, &recv_requests[sender]);
            iRCCE_add_recv_to_wait_list(&waitlist, &recv_requests[sender]);

        }
    }
}


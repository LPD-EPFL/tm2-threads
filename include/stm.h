/* 
 * File:   stm.h
 * Author: trigonak
 *
 * Created on April 11, 2011, 6:05 PM
 * 
 * Data structures and operations related to the TM metadata
 */

#ifndef STM_H
#define	STM_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <setjmp.h>
#include "log.h"
#include "mem.h"

  typedef enum {
    IDLE,
    RUNNING,
    ABORTED,
    COMMITED
  } TX_STATE;

  typedef struct ALIGNED(64) stm_tx /* Transaction descriptor */
  { 
#if defined(GREEDY) /* placed in diff place than for FAIRCM, according to access seq */
    ticks start_ts;
#eif defined(FAIRCM) 
    ticks start_ts;
#endif
    uint32_t aborts;	 /* Total number of aborts (cumulative) */
    uint32_t aborts_raw; /* Aborts due to read after write (cumulative) */
    uint32_t aborts_war; /* Aborts due to write after read (cumulative) */
    uint32_t aborts_waw; /* Aborts due to write after write (cumulative) */
    uint32_t max_retries; /* Maximum number of consecutive aborts (retries) */
    uint32_t retries;	  /* Number of consecutive aborts (retries) */
    mem_info_t* mem_info; /* Transactional mem alloc lists*/
#if !defined(PGAS)		/* in PGAS only the DSLs hold a write_set */
    write_set_t *write_set;	/* Write set */
#endif
    sigjmp_buf env;		/* Environment for setjmp/longjmp */
  } stm_tx_t;

  typedef struct stm_tx_node 
  {
    uint32_t tx_starts;
    uint32_t tx_commited;
    uint32_t tx_aborted;
    uint32_t max_retries;
    uint32_t aborts_war;
    uint32_t aborts_raw;
    uint32_t aborts_waw;
#ifdef FAIRCM
    ticks tx_duration;
#else
    uint8_t padding[36];
#endif
  } stm_tx_node_t;

  INLINED void tx_metadata_node_print(stm_tx_node_t * stm_tx_node) {
    printf("TXs Statistics for node --------------------------------------\n");
    printf("Starts      \t: %llu\n", stm_tx_node->tx_starts);
    printf("Commits     \t: %llu\n", stm_tx_node->tx_commited);
    printf("Aborts      \t: %llu\n", stm_tx_node->tx_aborted);
    printf("Max Retries \t: %llu\n", stm_tx_node->max_retries);
    printf("Aborts WAR  \t: %llu\n", stm_tx_node->aborts_war);
    printf("Aborts RAW  \t: %llu\n", stm_tx_node->aborts_raw);
    printf("Aborts WAW  \t: %llu\n", stm_tx_node->aborts_waw);
    printf("--------------------------------------------------------------\n");
    fflush(stdout);
  }

  INLINED void tx_metadata_print(stm_tx_t * stm_tx) {
    printf("TX Statistics ------------------------------------------------\n");
    printf("Retries     \t: %llu\n", stm_tx->retries);
    printf("Aborts      \t: %llu\n", stm_tx->aborts);
    printf("Max Retries \t: %llu\n", stm_tx->max_retries);
    printf("Aborts WAR  \t: %llu\n", stm_tx->aborts_war);
    printf("Aborts RAW  \t: %llu\n", stm_tx->aborts_raw);
    printf("Aborts WAW  \t: %llu\n", stm_tx->aborts_waw);
    printf("--------------------------------------------------------------\n");
    fflush(stdout);
  }

  INLINED stm_tx_node_t * tx_metadata_node_new() {
    stm_tx_node_t *stm_tx_node_temp = (stm_tx_node_t *) malloc(sizeof (stm_tx_node_t));
    if (stm_tx_node_temp == NULL) {
      printf("malloc stm_tx_node @ tx_metadata_node_new");
      return NULL;
    }

    stm_tx_node_temp->tx_starts = 0;
    stm_tx_node_temp->tx_commited = 0;
    stm_tx_node_temp->tx_aborted = 0;
    stm_tx_node_temp->max_retries = 0;
    stm_tx_node_temp->aborts_war = 0;
    stm_tx_node_temp->aborts_raw = 0;
    stm_tx_node_temp->aborts_waw = 0;

#if defined(FAIRCM)
    stm_tx_node_temp->tx_duration = 1;
#endif

    return stm_tx_node_temp;
  }

  INLINED stm_tx_t* 
  tx_metadata_new() 
  {
    stm_tx_t *stm_tx_temp = (stm_tx_t *) malloc(sizeof(stm_tx_t));
    if (stm_tx_temp == NULL) 
      {
	printf("malloc stm_tx @ tx_metadata_new");
	return NULL;
      }

#if !defined(PGAS) 
    stm_tx_temp->write_set = write_set_new();
#endif
    stm_tx_temp->mem_info = mem_info_new();

    stm_tx_temp->retries = 0;
    stm_tx_temp->aborts = 0;
    stm_tx_temp->aborts_war = 0;
    stm_tx_temp->aborts_raw = 0;
    stm_tx_temp->aborts_waw = 0;
    stm_tx_temp->max_retries = 0;

#if defined(FAIRCM) || defined(GREEDY)
    stm_tx_temp->start_ts = 0;
#endif

    return stm_tx_temp;
  }

  INLINED stm_tx_t * tx_metadata_empty(stm_tx_t *stm_tx_temp) {

#if !defined(PGAS)
    stm_tx_temp->write_set = write_set_empty(stm_tx_temp->write_set);
#endif
    //stm_tx_temp->mem_info = mem_info_new();
    //TODO: what about the env?
    stm_tx_temp->retries = 0;
    stm_tx_temp->aborts = 0;
    stm_tx_temp->aborts_war = 0;
    stm_tx_temp->aborts_raw = 0;
    stm_tx_temp->aborts_waw = 0;
    stm_tx_temp->max_retries = 0;

    return stm_tx_temp;
  }

  INLINED void tx_metadata_free(stm_tx_t **stm_tx) {
    //TODO: "clear" insted of freeing the stm_tx

#if !defined(PGAS)
    write_set_free((*stm_tx)->write_set);
#endif
    mem_info_free((*stm_tx)->mem_info);
    free((*stm_tx));
    *stm_tx = NULL;
  }

#ifdef	__cplusplus
}
#endif

#endif	/* STM_H */


#ifndef PKTSINK_CORE_WRITE_H
#define PKTSINK_CORE_WRITE_H

#include <stdbool.h>
#include <stdint.h>

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_atomic.h>


#define PKTSINK_OUTPUT_FILE_LENGTH 100
#define PKTSINK_WRITE_BURST_SIZE 256

struct rx_core_stats {
    uint64_t packets;
    uint64_t bytes;
    uint64_t drop;
    uint64_t rx_empty;
};

struct rx_core_config {
    bool volatile *stop_condition;
    struct rte_mempool *pool;
    uint64_t pause;
    uint64_t sleep;
    rte_atomic16_t *core_counter;
    uint16_t burst_size;
    int core_id;
    uint16_t port;
    uint16_t queue_min;
    uint16_t queue_num;
    struct rx_core_stats stats;
};

int rx_core(struct rx_core_config *config);

#endif

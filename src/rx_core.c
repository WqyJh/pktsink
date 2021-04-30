#include <errno.h>
#include <generic/rte_atomic.h>
#include <generic/rte_cycles.h>
#include <rte_malloc.h>
#include <rte_mbuf_core.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_branch_prediction.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_ring.h>
#include <rte_version.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>

#include <pcap.h>
#include <rx_core.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define RTE_LOGTYPE_RX RTE_LOGTYPE_USER1

int rx_core(struct rx_core_config *config)
{
    struct rx_core_stats *stats = &config->stats;
    int qid = config->queue_min;
    int qmax = config->queue_min + config->queue_num - 1;

    RTE_LOG(INFO, RX, "Rx core %u is running for port %u queue %u-%u\n",
        rte_lcore_id(), config->port, qid, qmax);

    struct rte_mbuf **mbufs = rte_malloc(NULL, sizeof(struct rte_mbuf *) * config->burst_size, 0);

    for (;;) {
        if (unlikely(*(config->stop_condition))) {
            break;
        }

        int nb_rx = rte_eth_rx_burst(config->port, qid, mbufs, config->burst_size);
        if (++qid > qmax) qid = config->queue_min;

        for (int j = 0; j < nb_rx; j++) {
            struct rte_mbuf *mbuf = mbufs[j];
            stats->bytes += rte_pktmbuf_pkt_len(mbuf);
            rte_pktmbuf_free(mbuf);
        }
        rte_pktmbuf_free_bulk(mbufs, nb_rx);
        stats->packets += nb_rx;
    }

    rte_atomic16_dec(config->core_counter);
    RTE_LOG(INFO, RX, "Rx core %u stopped\n", rte_lcore_id());
    return 0;
}

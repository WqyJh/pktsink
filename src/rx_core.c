#include <errno.h>

#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_branch_prediction.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_ring.h>
#include <rte_version.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>

#include <common.h>
#include <pcap.h>
#include <rx_core.h>
#include <scheduler.h>
#include <unistd.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define RTE_LOGTYPE_RX RTE_LOGTYPE_USER1


int set_scheduler(uint64_t runtime, uint64_t deadline, uint64_t period) {
    struct sched_attr attr = {
        .sched_policy = SCHED_DEADLINE,
        .sched_runtime = runtime,
        .sched_deadline = deadline,
        .sched_period = period,
        .size = sizeof(struct sched_attr),
        .sched_flags = 0,
        .sched_nice = 0,
        .sched_priority = 0,
    };
    int ret;
    ret = sched_setattr(0, &attr, 0);
    if (ret < 0) {
        perror("set_scheduler error");
    }
    return ret;
}

int rx_core(struct rx_core_config *config)
{
    struct rx_core_stats *stats = &config->stats;
    int qid = config->queue_min;
    int qmax = config->queue_min + config->queue_num - 1;

    if (config->runtime && config->deadline && config->period) {
        int ret = set_scheduler(config->runtime, config->deadline, config->period);
        if (ret < 0) {
            goto exit;
        }
        config->should_yield_ = true;
        RTE_LOG(INFO, RX, "Rx core %u SCHED_DEADLINE runtime:%lu deadline:%lu period:%lu\n",
            rte_lcore_id(), config->runtime, config->deadline, config->period);
    }

    RTE_LOG(INFO, RX, "Rx core %u is running for port %u queue %u-%u\n",
        rte_lcore_id(), config->port, qid, qmax);

    struct rte_mbuf **mbufs = rte_malloc(NULL, sizeof(struct rte_mbuf *) * config->burst_size, 0);

    for (;;) {
        if (unlikely(*(config->stop_condition))) {
            break;
        }

        int nb_rx = rte_eth_rx_burst(config->port, qid, mbufs, config->burst_size);
        if (unlikely(nb_rx == 0)) {
            stats->rx_empty++;
            if (config->should_yield_) {
                sched_yield();
            } else if (config->sleep) {
                switch (config->sleepfunc) {
                case SF_FLAG_rte_delay_us_sleep:
                    rte_delay_us_sleep(config->sleep);
                    break;
                case SF_FLAG_rte_delay_us_block:
                    rte_delay_us_block(config->sleep);
                    break;
                case SF_FLAG_usleep:
                default:
                    usleep(config->sleep);
                }
            }
        }
        if (++qid > qmax) qid = config->queue_min;

        stats->packets += nb_rx;
        for (int j = 0; j < nb_rx; j++) {
            struct rte_mbuf *mbuf = mbufs[j];
            stats->bytes += rte_pktmbuf_data_len(mbuf);
            // struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
            // if (ntohs(eth_hdr->ether_type) != RTE_ETHER_TYPE_IPV4) {
            //     fprintf(stderr, "ether_type: 0x%4x ipv6:%d\n", eth_hdr->ether_type,
            //     ntohs(eth_hdr->ether_type) == RTE_ETHER_TYPE_IPV6
            //     );
            //     uint8_t *buf = rte_pktmbuf_mtod(mbuf, uint8_t*);
            //     for (int i = 0; i < rte_pktmbuf_data_len(mbuf); i++) {
            //         fprintf(stderr, "%02x ", buf[i]);
            //     }
            //     fprintf(stderr, "\n");
            // }
        }
        rte_pktmbuf_free_bulk(mbufs, nb_rx);
    }

exit:
    rte_atomic16_dec(config->core_counter);
    RTE_LOG(INFO, RX, "Rx core %u stopped\n", rte_lcore_id());
    return 0;
}

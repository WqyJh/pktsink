#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_ring.h>

#include <rx_core.h>
#include <statistics.h>

#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE_ID CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE_ID CLOCK_MONOTONIC
#endif

#define RTE_LOGTYPE_DPDKCAP RTE_LOGTYPE_USER1

#define STATS_PERIOD_MS 1000
#define ROTATING_CHAR "-\\|/"
static unsigned int nb_stat_update = 0;

const char *bytes_unit[] = {"B", "KB", "MB", "GB", "TB"};
const char *units[] = {"", "K", "M", "G", "T"};
char result[50];

char *bytes_format(uint64_t bytes) {
    int i;
    double converted_bytes = bytes;
    for (i = 0; i < 5 && bytes >= 1024; i++, bytes /= 1024) {
        converted_bytes = bytes / 1024.0;
    }

    sprintf(result, "%.2f %s", converted_bytes, bytes_unit[i]);
    return result;
}

char *kilo_format(double n, char *output, int len) {
    int i = 0;
    double d = n;
    for (; i < sizeof(units) && n >= 1000; i++, n /= 1000) {
        d = n / 1000.0;
    }

    snprintf(output, len, "%.3f %s", d, units[i]);
    return output;
}

double timespec_diff_to_double(const struct timespec start,
                               const struct timespec end) {
    struct timespec diff;
    double duration;

    diff.tv_sec = end.tv_sec - start.tv_sec;
    if (end.tv_nsec > start.tv_nsec)
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    else {
        diff.tv_nsec = end.tv_nsec - start.tv_nsec + 1000000000;
        diff.tv_sec--;
    }
    duration = diff.tv_sec + ((double)diff.tv_nsec / 1000000000);
    return (duration);
}

static int print_stats(struct stats_config *config, uint16_t port, int idx) {
    printf("-- PORT %u --\n", port);
    struct rte_eth_stats eth_stats;
    rte_eth_stats_get(port, &eth_stats);

    struct rx_core_stats rx_stats;
    memset(&rx_stats, 0, sizeof(struct rx_core_stats));
    for (int i = 0; i < config->nb_rx_cores; i++) {
        // Copy stats
        struct rx_core_config *rx_config = &config->rx_core_config_list[i];
        if (rx_config->port != port)
            continue;

        struct rx_core_stats stats = rx_config->stats;
        // Accumulate stats
        rx_stats.packets += stats.packets;
        rx_stats.bytes += stats.bytes;
        rx_stats.drop += stats.drop;
        // Print stats
        printf("Rx core %u port %u\n", rx_config->core_id, rx_config->port);
        printf("\tpackets=%lu\tbytes=%lu\tdrop=%lu\n", stats.packets,
               stats.bytes, stats.drop);
        printf("\tQueue %u-%u RX: %lu pkts %lu bytes %lu missed pkts\n",
               rx_config->queue_min,
               rx_config->queue_min + rx_config->queue_num - 1,
               eth_stats.q_ipackets[i], eth_stats.q_ibytes[i],
               eth_stats.q_errors[i]);
    }
    // Print accumulated stats
    printf("Rx core summary\tpackets=%lu\tbytes=%lu\tdrop=%lu\n",
           rx_stats.packets, rx_stats.bytes, rx_stats.drop);

    struct port_stats_ *stats_ = &config->stats_[idx];

    double avg_bytes = 0;
    double line_rate = 0;

    int ret = clock_gettime(CLOCK_TYPE_ID, &stats_->end);
    if (unlikely(ret)) {
        fprintf(stderr, "clock_gettime failed on start: %s\n", strerror(errno));
    } else {
        struct rte_eth_link eth_link;
        rte_eth_link_get_nowait(port, &eth_link);
        uint64_t rx_packets = eth_stats.ipackets - stats_->packets;
        uint64_t rx_bytes = eth_stats.ibytes - stats_->bytes;
        uint64_t drop = eth_stats.imissed + eth_stats.ierrors;
        if (rx_packets > 0) {
            avg_bytes =
                (double)rx_bytes / rx_packets +
                4; // add 4 bytes FCS
            line_rate = eth_link.link_speed * 1000 * 1000.0 /
                        (8 * (avg_bytes + 8 + 12));
        }
        double seconds = timespec_diff_to_double(stats_->start, stats_->end);
        double pps = rx_packets / seconds;
        double drop_pps = (drop - stats_->drop) / seconds;
        double bps = rx_bytes / seconds;
        stats_->packets = eth_stats.ipackets;
        stats_->drop = drop;
        stats_->bytes = eth_stats.ibytes;
        stats_->start = stats_->end;
#define BUF_LEN 16
        char pps_buf[BUF_LEN];
        char drop_pps_buf[BUF_LEN];
        char line_rate_buf[BUF_LEN];
        printf("\tspeed\t%spps\t%sbps\tdrop=%spps\tline_rate=%spps\n",
               kilo_format(pps, pps_buf, BUF_LEN), bytes_format(bps),
               kilo_format(drop_pps, drop_pps_buf, BUF_LEN),
               kilo_format(line_rate, line_rate_buf, BUF_LEN));
    }

    printf("\tBuilt-in counters:\n"
           "\tRX Successful packets: %lu\n"
           "\tRX Successful bytes: %s (avg: %.2lf bytes/pkt)\n"
           "\tRX Missed packets: %lu\n"
           "\tRX Unsuccessful packets: %lu\n",
           eth_stats.ipackets, bytes_format(eth_stats.ibytes), avg_bytes,
           eth_stats.imissed, eth_stats.ierrors);
    return 0;
}

void start_stats_display(struct stats_config *config) {
    config->stats_ =
        rte_zmalloc(NULL, sizeof(struct port_stats_) * config->nb_ports, 0);
    struct port_stats_ *stats = config->stats_;
    int ret = clock_gettime(CLOCK_MONOTONIC, &stats[0].start);
    if (unlikely(ret)) {
        fprintf(stderr, "clock_gettime failed on start: %s\n", strerror(errno));
    }
    for (int i = 1; i < config->nb_ports; i++) {
        stats[i].start = stats[0].start;
    }
    for (;;) {
        if (unlikely(*(config->stop_condition) ||
                     rte_atomic16_read(config->core_counter) == 0)) {
            break;
        }
        printf("\e[1;1H\e[2J");
        printf("=== Packet capture statistics %c ===\n",
               ROTATING_CHAR[nb_stat_update++ % 4]);
        uint16_t port;
        int idx = 0;
        RTE_ETH_FOREACH_DEV(port) {
            if ((1ULL << port) & config->portmask) {
                print_stats(config, port, idx++);
            }
        }
        printf("===================================\n\n");
        rte_delay_ms(config->interval);
    }
    rte_free(config->stats_);
}

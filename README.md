# pktsink

A DPDK-based program just sinking packets.

## Dependencies

Install dpdk first.

See [Compiling and Installing DPDK System-wide](https://doc.dpdk.org/guides/linux_gsg/build_dpdk.html#compiling-and-installing-dpdk-system-wide).

## Build & Install

```bash
mkdir build/
cd build/
cmake ..
make
make install
```

## Run

Receive packets from port 1.

```bash
sudo pktsink -l 0-1 -- --portmask 0x1
```

You should see statistics like this. It's close to 10Gbit/s line rate with 1 core and 1 rx queue.

> 14.202 Mpps is the physical line rate in my NIC, although which claim to have 10Gbit/s speed (14.88 Mpps).
> Just like the RAM or DISK capacity, the actual amount you can use is always less than it claims.

```bash
=== Packet capture statistics - ===
-- PORT 0 --
	Built-in counters:
	RX Successful packets: 58259940
	RX Successful bytes: 3.47 GB (avg: 64.00 bytes/pkt)
	RX Unsuccessful packets: 0
Rx core 1 port 0
	packets=58261868	bytes=3728759552	drop=0
	Queue 0-0 RX: 58259940 pkts 3728637952 bytes 0 missed pkts
Rx core summary
	packets=58261868	bytes=3728759552	drop=0
	speed	14.202 Mpps	908.945 Mbps	line_rate=14.881 Mpps
===================================
```

You can adjust the arguments like the following.

- 2 rx cores
- 4 rx queues each core (16 rx queues in total)
- 4096 rx_descs for each rx queue
- burst 1024 packets at a time
- 11904771 mbufs in mbuf pool
- print statistics every 500 ms

```bash
sudo pktsink -l 0-3 -- --portmask 0x1 --cores 2 --rxq 4 --rxd 4096 --burst 1024 --num_mbufs 11904771 --stats 500
```

You should see the following statistics. There're stats for each rx core and summary for all rx cores.

> Note that the performance are worse than single core with single queue, because the NIC do RSS hash
> when multiple queue was enabled, which takes some time.
> Therefore, just one rx core is enough for packet sinking, only use multiple cores when single core cannot handle.

```bash
=== Packet capture statistics \ ===
-- PORT 0 --
	Built-in counters:
	RX Successful packets: 107540556
	RX Successful bytes: 6.41 GB (avg: 64.00 bytes/pkt)
	RX Unsuccessful packets: 0
Rx core 1 port 0
	packets=53769938	bytes=3441276032	drop=0
	Queue 0-3 RX: 53770772 pkts 3441330368 bytes 0 missed pkts
Rx core 2 port 0
	packets=53768955	bytes=3441213248	drop=0
	Queue 4-7 RX: 53769784 pkts 3441266944 bytes 0 missed pkts
Rx core summary
	packets=107538893	bytes=6882489280	drop=0
	speed	12.920 Mpps	826.876 Mbps	line_rate=14.881 Mpps
```

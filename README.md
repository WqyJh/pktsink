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

```bash
=== Packet capture statistics / ===
-- PORT 0 --
	Built-in counters:
	RX Successful packets: 661487651
	RX Successful bytes: 49.90 GB (avg: 81 bytes/pkt)
	RX Unsuccessful packets: 0
Rx core 1 port 0
	packets=661489238	bytes=53580628895	drop=0
	Queue 0-3 RX: 661487651 pkts 53580502259 bytes 0 missed pkts
Rx core summary
	packets=661489238	bytes=53580628895	drop=0
	speed	11.35 Mpps	919.58 Mbps
```

You can adjust the arguments like the following.

- 4 rx cores
- 4 rx queues each core (16 rx queues in total)
- 4096 rx_descs for each rx queue
- burst 1024 packets at a time
- 11904771 mbufs in mbuf pool
- print statistics every 500 ms

```bash
sudo pktsink -l 0-4 -- --portmask 0x1 --cores 4 --rxq 4 --rxd 4096 --burst 1024 --num_mbufs 11904771 --stats 500
```

You should see the following statistics. There're stats for each rx core and summary for all rx cores.

```bash
=== Packet capture statistics \ ===
-- PORT 0 --
	Built-in counters:
	RX Successful packets: 116695284
	RX Successful bytes: 8.80 GB (avg: 81 bytes/pkt)
	RX Unsuccessful packets: 0
Rx core 4 port 0
	packets=29291967	bytes=2372888455	drop=0
	Queue 0-3 RX: 29293287 pkts 2372993977 bytes 0 missed pkts
Rx core 6 port 0
	packets=29037569	bytes=2351919945	drop=0
	Queue 4-7 RX: 29036777 pkts 2351856289 bytes 0 missed pkts
Rx core 8 port 0
	packets=29025529	bytes=2353563441	drop=0
	Queue 8-11 RX: 29026655 pkts 2353656928 bytes 0 missed pkts
Rx core 10 port 0
	packets=29339488	bytes=2375726608	drop=0
	Queue 12-15 RX: 29338565 pkts 2375651837 bytes 0 missed pkts
Rx core summary
	packets=116694553	bytes=9454098449	drop=0
	speed	10.60 Mpps	858.49 Mbps
===================================
```


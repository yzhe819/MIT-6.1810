# Lab 6: Networking

[English](./lab6-xv6-network-driver.md) · [中文](./lab6-xv6-network-driver-zh.md)

In this lab you will write an xv6 device driver for a network interface card (NIC), and then write the receive half of an ethernet/IP/UDP protocol processing stack.

Fetch the xv6 source and switch to the `net` branch:

```bash
$ git fetch
$ git checkout net
$ make clean
```

---

## Background

Before coding, it is useful to review Chapter 6 (Interrupts and device drivers) in the [xv6 book](xv6/book-riscv-rev5.pdf).

You will use an E1000 network device. To xv6 and your driver, it appears to be real hardware on a real Ethernet LAN. In fact, QEMU emulates both the E1000 and the LAN. On this emulated LAN:

- xv6 (guest) IP is `10.0.2.15`
- host (running QEMU) appears as `10.0.2.2`

When xv6 sends a packet to `10.0.2.2` through E1000, QEMU delivers it to the corresponding host-side application.

This lab uses QEMU user-mode networking stack. See QEMU docs: [User Networking (SLIRP)](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29).

The Makefile is configured to enable both user-mode networking and E1000 emulation, and records all ingress/egress packets to `packets.pcap` in the lab directory.

You can inspect captured packets with:

```bash
tcpdump -XXnr packets.pcap
```

Additional lab files include:

- `kernel/e1000.c`: E1000 init code + empty TX/RX driver functions to complete
- `kernel/e1000_dev.h`: E1000 register/flag definitions
- `kernel/net.c`, `kernel/net.h`: minimal IP/UDP/ARP stack
- `kernel/pci.c`: PCI probe code that finds E1000 at boot

---

## Part One: NIC

Your job is to complete `e1000_transmit()` and `e1000_recv()` in `kernel/e1000.c` so the driver can send/receive packets.

You are done with this part when `make grade` passes tests **txone** and **rxone**.

When writing code, refer to Intel E1000 [Software Developer's Manual](readings/8254x_GBe_SDM.pdf), especially:

- Section 3.2 for receive (skip 3.2.8 and 3.2.9)
- Sections 3.3.1/3.3.2/3.3.3 and 3.4 for transmit
- Section 13 for register reference

QEMU emulates **82540EM**. You only need a small subset of E1000 features for this lab.

`e1000_init()` (provided) sets up DMA-based TX/RX rings in RAM:

- RX ring: descriptors (`struct rx_desc`) pointing to buffers where E1000 writes received packets
- TX ring: descriptors (`struct tx_desc`) where driver submits packets to transmit

Ring sizes are `RX_RING_SIZE` and `TX_RING_SIZE`.

### TX path summary

`net.c` calls `e1000_transmit()` with a packet buffer (allocated by `kalloc()`).

Your driver should:

1. Put packet buffer pointer into a TX descriptor.
2. Later `kfree()` the buffer only after hardware sets `E1000_TXD_STAT_DD`.
3. Advance `E1000_TDT` correctly.

### RX path summary

For each received packet, E1000 DMAs into the buffer pointed to by next RX descriptor and triggers interrupt (via PLIC) when appropriate.

Your `e1000_recv()` should:

1. Scan RX ring for newly completed descriptors.
2. Deliver each packet to `net_rx()`.
3. Allocate replacement buffer and re-arm descriptor.
4. Update `E1000_RDT`.

### Registers

Use memory-mapped register array `regs` (base pointer to E1000 registers).

You will particularly use:

- `E1000_TDT`
- `E1000_RDT`

### Testing Part One

`txone` test:

1. Host window: `python3 nettest.py txone`
2. xv6 window: `make qemu`, then run `nettest txone`

If successful, host side prints `txone: OK`.

`rxone` test:

1. xv6 window: `make qemu`
2. Host window: `python3 nettest.py rxone`

Expected xv6 logs include:

- `arp_rx: received an ARP packet`
- `ip_rx: received an IP packet`

If both TX/RX paths work, `make grade` should pass first two tests.

### e1000 hints

#### For `e1000_transmit`

- Add temporary prints in `e1000_transmit()` and `e1000_recv()`, then run `nettest txone`.
- Use legacy TX descriptor format in `e1000_dev.h` (manual section 3.3.3).
- Read current TX ring index via `E1000_TDT`.
- Check overflow: if descriptor at `E1000_TDT` lacks `E1000_TXD_STAT_DD`, return error.
- `kfree()` previously transmitted buffer for that descriptor (if present).
- Fill descriptor and set required cmd flags.
- Advance `E1000_TDT` modulo `TX_RING_SIZE`.

#### For `e1000_recv`

- Compute next RX descriptor index from `E1000_RDT + 1` modulo `RX_RING_SIZE`.
- Check `E1000_RXD_STAT_DD`; if not set, stop.
- Call `net_rx()` with packet buffer.
- Allocate replacement buffer via `kalloc()`; clear descriptor status bits.
- Update `E1000_RDT` to last processed descriptor.
- Review `e1000_init()` RX-ring initialization for reference.
- Ensure correctness after ring wrap-around (more than 16 total packets).
- One interrupt can correspond to multiple packets; handle looped receive.

You will need locking for concurrent use (multiple processes and interrupt context).

---

## Part Two: UDP Receive

UDP allows user processes on different hosts to exchange datagrams over IP.

In this part, extend `kernel/net.c` to receive UDP packets, queue them, and allow user-space reads. `net.c` already has outbound UDP logic (besides `e1000_transmit()`, which you implement).

Fill in:

- `ip_rx()`
- `sys_recv()`
- `sys_bind()`

You are done when `make grade` passes all tests.

Run grading tests manually:

1. Host window: `python3 nettest.py grade`
2. xv6 window: `nettest grade`

Expected host output starts with:

```bash
$ python3 nettest.py grade
txone: OK
rxone: sending one UDP packet
```

Expected xv6 output includes:

```bash
$ nettest grade
txone: sending one packet
arp_rx: received an ARP packet
ip_rx: received an IP packet
ping0: starting
ping0: OK
ping1: starting
ping1: OK
ping2: starting
ping2: OK
ping3: starting
ping3: OK
dns: starting
DNS arecord for pdos.csail.mit.edu. is 128.52.129.126
dns: OK
free: OK
```

### UDP syscall API

- `send(short sport, int dst, short dport, char *buf, int len)`
  - send UDP payload to `dst:dport` with source port `sport`
  - returns `0` on success, `-1` on failure
- `recv(short dport, int *src, short *sport, char *buf, int maxlen)`
  - receive next queued packet for destination port `dport`
  - if queue empty, block until one arrives
  - copy source IP to `*src`, source port to `*sport`, payload (up to `maxlen`) to `buf`
  - return payload bytes copied, or `-1` on error
- `bind(short port)`
  - must be called before `recv(port, ...)`
  - packets to unbound destination ports are dropped
- `unbind(short port)`
  - optional (not required by tests)

All syscall addresses/ports are in host byte order.

`user/nettest.c` uses this API. You provide kernel implementations except `send()`.

### Receive-queue behavior in `ip_rx()`

For each incoming IP packet, `ip_rx()` should:

1. check whether packet is UDP
2. check whether destination port is bound
3. if yes, queue for `recv()`

Queue limit rule:

- at most 16 queued packets per destination port
- if one port already has 16 queued packets, drop further packets for that port
- drops for one port must not affect other ports

Packet layout in buffer:

- 14-byte Ethernet header
- 20-byte IP header
- 8-byte UDP header
- UDP payload

Struct definitions are in `kernel/net.h`.

Need-to-parse fields:

- IP: `ip_p`, `ip_src`
- UDP: `dport`, `sport`, `ulen`

### Byte order note

Ethernet/IP/UDP multi-byte fields are network byte order (big-endian), while RISC-V is little-endian in memory.

Use:

- `ntohs()` for 2-byte fields
- `ntohl()` for 4-byte fields

See `net_rx()` for an example on Ethernet type parsing.

Also note: bugs in E1000 code may surface only in ping tests (e.g., ring index wrap-around).

### Additional hints

- Create a struct to track bound ports and queued packets.
- Use `sleep(void *chan, struct spinlock *lk)` and `wakeup(void *chan)` in `kernel/proc.c` for blocking/wakeup in `recv()`.
- `sys_recv()` destination pointers are user virtual addresses; copy from kernel to current user process properly.
- Free packets after copying to user space or dropping.

---

## Submit the Lab

### Time Spent

Create `time.txt` with a single integer: total hours spent. `git add` and `git commit` it.

### Answers

If this lab has questions, write answers in `answers-*.txt`, then `git add` and `git commit`.

### Submit

Submission is via Gradescope. You need an MIT Gradescope account. See Piazza for class entry code. Use [this link](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code) if needed.

Run `make zipball` to generate `lab.zip`, then upload it.

If there are uncommitted/untracked files, output may look like:

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

Ensure required files are tracked (not shown as `??`). Use `git add {filename}`.

- Run `make grade` before submission.
- Commit modified source before `make zipball`.
- Gradescope lab grade is the final lab grade.

---

## Optional Challenges

- This lab uses interrupts for ingress processing but not for egress. A more advanced design queues egress packets in software, provides only a limited set to NIC at once, and uses TX interrupts to refill TX ring. This enables egress traffic prioritization.
- Implement a full [ARP cache](https://tools.ietf.org/html/rfc826).
- E1000 supports multiple RX/TX rings. Configure one ring pair per core and modify stack accordingly to increase throughput and reduce lock contention (hard to test/measure under QEMU).
- Detect [ICMP](https://tools.ietf.org/html/rfc792) failure notifications and propagate them as user-process errors.
- Use E1000 stateless offloads (checksum, RSC, GRO, etc.) to improve throughput (hard to test/measure).
- Fix receive livelock susceptibility in this stack using lecture/reading material (hard to test).
- Implement a minimal TCP stack and download a web page.

Some challenge optimizations may not be clearly measurable in QEMU.

If you work on any challenge (networking-related or not), please let the course staff know.

---

*Questions or comments regarding 6.1810? Send e-mail to the course staff at 61810-staff@lists.csail.mit.edu.*

# Lab 6 Report (AI)

**Summary:** Implement the E1000 transmit and receive driver paths, and complete the xv6 network stack UDP receive path (`ip_rx`, `sys_recv`, `sys_bind`).

**Difficulty:** `Part One: NIC` · `Part Two: UDP Receive`

The networking chapter is relatively self-contained and feels quite different from the earlier OS labs. There are many conventions and implementation details specific to networking, and it has little to do with memory management or page tables. Prior experience from earlier labs won't be as directly applicable here, but the lab is still well worth working through.

---

## Part One: NIC

This part requires implementing two functions in `kernel/e1000.c`: `e1000_transmit()` and `e1000_recv()`. When `net.c` calls `e1000_transmit()`, it passes in a buffer already allocated via `kalloc()` containing the data to be sent. `e1000_recv()`, on the other hand, is responsible for writing received packets directly into memory buffers, managed through a receive ring (RX ring).

### Implementing `e1000_transmit()`

Start by understanding two key fields: `TDT` and `DD`.

```c
regs[E1000_TDLEN] = sizeof(tx_ring);
regs[E1000_TDH] = regs[E1000_TDT] = 0;
```

Looking at `e1000_dev.h`, `E1000_TDT` (Transmit Descriptor Tail) points to the next available slot in the transmit ring. At initialization, `TDT` is set to 0, meaning the ring is completely empty and index 0 is ready for use.

```c
for (i = 0; i < TX_RING_SIZE; i++) {
  tx_ring[i].status = E1000_TXD_STAT_DD;
  tx_ring[i].addr = 0;
}
```

The `E1000_TXD_STAT_DD` bit in the `status` field is initialized to 1, indicating the slot is free and available for writing. A value of 0 means the slot is currently in use.

We also need a separate array to track the buffer pointer associated with each slot, so we can free the memory once the previous transmission completes:

```c
static char *tx_bufs[TX_RING_SIZE];
```

Here's the overall logic before writing the code: given a packet to send, use `TDT` to find the next available slot; check `status` to confirm it's writable; if the ring is full, return an error; otherwise, fill in the packet, set `status` to 0 (marking the slot as occupied), update `TDT`, and advance the ring.

One important note: the `cmd` field needs to be set with two flags — `E1000_TXD_CMD_EOP` marks this descriptor as the last one for the current packet, and `E1000_TXD_CMD_RS` tells the NIC to write the status back to the descriptor (i.e., set the DD bit again) once transmission completes.

The complete `e1000_transmit` implementation:

```c
int
e1000_transmit(char *buf, int len)
{
  //
  // Your code here.
  //
  // buf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after send completes.
  //
  // return 0 on success.
  // return -1 on failure (e.g., there is no descriptor available)
  // so that the caller knows to free buf.
  //
  acquire(&e1000_lock);
  uint32 tail = regs[E1000_TDT];
  if(!(tx_ring[tail].status & E1000_TXD_STAT_DD)){
    release(&e1000_lock);
    return -1;
  }

  if(tx_bufs[tail] != 0){
    // release the old transmit data (from last round)
    kfree(tx_bufs[tail]);
  }

  tx_ring[tail].status = 0;
  tx_ring[tail].addr   = (uint64) buf;
  tx_ring[tail].length = len;
  tx_ring[tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  
  tx_bufs[tail] = buf;

  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;
  
  release(&e1000_lock);
  return 0;
}
```

With `e1000_transmit()` done, you can test it by opening two terminal windows: run `python3 nettest.py txone` in one, and `nettest txone` inside xv6 (via `make qemu`) in the other to send a packet.

Then run `tcpdump -XXnr packets.pcap` in a third window to inspect the captured traffic. A successful result looks like this:

![2026-07-12 16.42.41](./2026-07-12%2016.42.41.png)

### Implementing `e1000_recv()`

```c
regs[E1000_RDT] = RX_RING_SIZE - 1;
```

Notice that during initialization, `RDT` is set to `RX_RING_SIZE - 1` rather than 0. This means the first available receive slot is at index `(RDT + 1) % RX_RING_SIZE`. The lab hint calls this out explicitly:

> First ask the E1000 for the ring index at which the next waiting received packet (if any) is located, by fetching the E1000_RDT register and adding one modulo RX_RING_SIZE.

Similarly, the `status` field is initialized to 0. A value of `status = 0` means the NIC has not yet written data to this buffer (not ready to read); once the DD bit is set (`E1000_RXD_STAT_DD`), the buffer contains a received packet and is ready to be processed.

> The e1000 can deliver more than one packet per interrupt.

Since a single interrupt may deliver multiple packets, the hint recommends using a `while` loop to keep processing until no more packets are available.

The overall logic: start from `tail + 1` (the first slot where `status` is not yet set to DD, found by bitwise AND with `E1000_RXD_STAT_DD`). In the `while` loop, call `net_rx` for each received packet, allocate new memory with `kalloc` for the next packet, reset `status` to 0, update `RDT` and `tail` to advance one step around the ring.

```c
static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver a buf for each packet (using net_rx()).
  //
  acquire(&e1000_lock);
  uint32 tail = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  int has_packet = rx_ring[tail].status & E1000_RXD_STAT_DD;
  release(&e1000_lock);

  while(has_packet){
    // the net_rx will do the kfree, we donot need to do it manually
    net_rx((char*)rx_ring[tail].addr, rx_ring[tail].length);

    acquire(&e1000_lock);
    rx_ring[tail].addr = (uint64) kalloc();
    rx_ring[tail].length = 0;
    rx_ring[tail].csum = 0;
    // clear the DD status
    rx_ring[tail].status = 0;
    // also update the RDT
    regs[E1000_RDT] = tail;
    // move to handle the next on the ring
    tail = (tail + 1) % RX_RING_SIZE;
    has_packet = rx_ring[tail].status & E1000_RXD_STAT_DD;
    release(&e1000_lock);
  }
}
```

To test the receive path, open two windows: run `make qemu` to start xv6 in one, and `python3 nettest.py rxone` in the other to send a packet to xv6. The system will print a message when the packet is received successfully:

![2026-07-12 16.42.27](./2026-07-12%2016.42.27.png)

![2026-07-12 16.42.48](./2026-07-12%2016.42.48.png)

You can also confirm the captured traffic with `tcpdump -XXnr packets.pcap`:

![2026-07-12 16.42.41](./2026-07-12%2016.42.41.png)

---

## Part Two: UDP Receive

This part adds UDP support to xv6's network stack.

Three functions are involved: `ip_rx()`, `sys_recv()`, and `sys_bind()`.
- `sys_bind` handles initialization and port binding.
- `ip_rx` checks whether an incoming packet is UDP, verifies that its destination port matches a bound port, and enqueues the packet's payload.
- `sys_recv` runs in user-process context, dequeues a packet placed there by `ip_rx`, and copies its data to user memory.

### Data Structures

Following convention, start with the simplest part — initialization. We need an array to track which ports are bound and their current state, along with a queue to hold received packets and a count of how many are pending.

```c
struct udp_pkt {
  uint32 sip;
  uint16 sport; // source port
  uint16 dport; // destination port
  uint16 ulen;  // length, including udp header, not including IP header
  char payload[UDP_MAXPAYLOAD]; 
  uint16 sum;   // checksum
};

// system bind constant

#define MAX_PORTS 32
#define MAX_QUEUE 16

struct binding {
  int in_use;
  int port;
  struct udp_pkt queue[MAX_QUEUE];
  int head;
  int count;
};
```

The `udp_pkt` struct is derived from the original UDP header struct, with two additional fields: `sip` (source IP address) and `payload` (the UDP payload — just the data portion, not the eth/ip/udp headers). Since the maximum payload size is bounded by a system constant, we use a fixed-size array. When processing a packet, we allocate new memory with `kalloc`, copy the payload in, and then free the original buffer.

### `sys_bind`

Add the `bindings` array to `net.c` and implement `sys_bind`:

```c

// the port list for binding
struct binding bindings[MAX_PORTS];

static struct spinlock netlock;

void
netinit(void)
{
  initlock(&netlock, "netlock");
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  int port;
  argint(0, &port);

  acquire(&netlock);
  for(int i = 0; i < MAX_PORTS; i++){
    if(bindings[i].in_use == 0){
      bindings[i].port = port;
      bindings[i].head = 0;
      bindings[i].count = 0;
      bindings[i].in_use = 1;
      release(&netlock);
      return 0;
    }
  }

  release(&netlock);
  return -1;
}
```

### `ip_rx`

The overall logic: parse the raw packet into its headers, check whether the protocol is UDP (we only handle UDP), look up the destination port in the `bindings` array, find an available slot in that port's circular queue, copy the packet fields and payload in, update `head` and `count`, and call `wakeup` to wake any sleeping process waiting on that port. (The `wakeup` target will be explained in `sys_recv` below.)

```c
void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  // get the header information
  struct ip  *ip  = (struct ip *) (buf + sizeof(struct eth));
  struct udp *udp = (struct udp *) ((char *)ip + sizeof(struct ip));

  // only handle udp request
  if(ip->ip_p == IPPROTO_UDP){
    uint16 dport = ntohs(udp->dport);
    uint16 sport = ntohs(udp->sport);
    uint32 srcip = ntohl(ip->ip_src);

    acquire(&netlock);
    for(int i = 0; i < MAX_PORTS; i++){
      // find the correct port
      if(dport == bindings[i].port && bindings[i].in_use == 1){
        // get the payload and len
        char *payload = (char *)udp + sizeof(struct udp);
        int payload_len = ntohs(udp->ulen) - sizeof(struct udp);

        if(bindings[i].count < MAX_QUEUE){
          // create new udp package
          // add it to the bindings queue
          struct udp_pkt *p = &bindings[i].queue[bindings[i].head];
          p->sip = srcip;
          p->sport = sport;
          p->ulen = payload_len;
          memmove(p->payload, payload, payload_len);

          // update the states
          bindings[i].head = (bindings[i].head + 1) % MAX_QUEUE;
          bindings[i].count++;

          wakeup(&bindings[i]);
        }
        break;
      }
    }
    release(&netlock);
  }

  // free the buf space
  kfree(buf);
  return;
}
```

### `sys_recv`

`sys_recv` needs to copy the source IP, source port, and payload (up to `maxlen` bytes) to user space, and return the number of bytes copied. If no packet is available yet, it blocks and waits. The system call signature is:

```
recv(short dport, int *src, short *sport, char *buf, int maxlen)
```

> recv(short dport, int *src, short *sport, char *buf, int maxlen): This system call returns the payload of a UDP packet that arrives with destination port dport.

A key point: `sys_recv` only needs to receive **one** UDP packet per call, so the only loop needed is the one for waiting — there's no iteration over multiple packets.

```c
uint64
sys_recv(void)
{
  int dport;
  uint64 srcaddr;   // user address for int *src
  uint64 sportaddr; // user address for short *sport
  uint64 bufaddr;   // user address for char *buf
  int maxlen;

  argint(0, &dport);
  argaddr(1, &srcaddr);
  argaddr(2, &sportaddr);
  argaddr(3, &bufaddr);
  argint(4, &maxlen);

  acquire(&netlock);
  for(int i=0; i<MAX_PORTS; i++){
    if(bindings[i].port == dport && bindings[i].in_use == 1){
      while(bindings[i].count == 0){
        // waiting for the data comes in
        sleep(&bindings[i], &netlock);
      }

      // already have some data
      int head = bindings[i].head;
      int count = bindings[i].count;
      int index = (head - count + MAX_QUEUE) % MAX_QUEUE;

      struct udp_pkt *p = &bindings[i].queue[index];
      char* payload = p->payload;
      int len = (int)p->ulen;
      len = maxlen >= len ? len : maxlen;

      bindings[i].count--;
      release(&netlock);

      // use copyout to copy the entire payload data
      // copyout

      struct proc *pr = myproc();

      copyout(pr->pagetable, srcaddr, (char *)&p->sip, sizeof(p->sip));
      copyout(pr->pagetable, sportaddr, (char *)&p->sport, sizeof(p->sport));
      copyout(pr->pagetable, bufaddr, payload, len);
      
      return len;
    }
  }
  release(&netlock);
  return -1;
}
```

We use `argint` and `argaddr` to retrieve the arguments passed from user space, then scan the `bindings` array for a matching port. If no data has arrived yet, we call `sleep(&bindings[i], &netlock)` to block — this is the exact channel that `ip_rx` targets with its `wakeup` call. Once woken, we compute the correct index from `head` and `count`, extract the payload, decrement the count, and use `copyout` to copy all the data to user space.

Also note `maxlen`: always cap the actual copy length to the smaller of the available payload length and the caller's `maxlen` before performing the copy.

Finally, don't forget to implement `sys_unbind`. While the lab notes it's not graded, a complete solution should include it:

```c
uint64
sys_unbind(void)
{
  int port;
  argint(0, &port);

  acquire(&netlock);
  for (int i = 0; i < MAX_PORTS; i++) {
    if (bindings[i].port == port && bindings[i].in_use == 1) {
      bindings[i].in_use = 0;
      break;
    }
  }
  release(&netlock);

  return 0;
}
```

To test Part Two, run `python3 nettest.py grade` in one window and `nettest grade` inside xv6 in another. If everything is working correctly, the Python window should show:

![2026-07-12 19.28.07](./2026-07-12%2019.28.07.png)

And the xv6 window should show:

![2026-07-12 19.28.03](./2026-07-12%2019.28.03.png)

---

## Full Lab 6 Test

Once all exercises are complete, run the following command to validate the entire lab:

```sh
./grade-lab-net
```

![2026-07-12 19.49.15](./2026-07-12%2019.49.15.png)

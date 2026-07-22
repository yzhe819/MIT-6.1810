# Lab 6 实验报告

**目标简述：** 实现 E1000 收发驱动路径，并补全 xv6 网络栈 UDP 接收路径（`ip_rx`、`sys_recv`、`sys_bind`）。

**实验难度：** `part one: nic` · `part two: udp receive`

这一章相对独立，与前面的 OS lab 风格不同：network 部分有许多约定俗成的实现细节，且与内存管理、`pagetable` 关系不大。因此此前积累的经验和直觉在本章中帮助有限，不过仍值得逐一梳理。

## Part one: NIC

本部分需要在 `kernel/e1000.c` 中实现两个函数：`e1000_transmit()` 和 `e1000_recv()`。当 `net.c` 调用 `e1000_transmit()` 时，会传入一个已经通过 `kalloc()` 分配好、装有待发送数据的 buffer；`e1000_recv()` 则负责将收到的包直接写入内存中的 buffer，由一个接收环形队列（RX ring）管理。

```c
regs[E1000_TDLEN] = sizeof(tx_ring);
regs[E1000_TDH] = regs[E1000_TDT] = 0;
```

先实现 `e1000_transmit()`。查看 `e1000_dev.h` 可以看到 `E1000_TDT`，它表示可以向 `tdt` 这个发送环中写入数据（初始化时 `tdt` 为 0，代表可以直接从索引 0 开始填充——因为一开始环中完全没有数据，标记为 0 即表示 `tdt` 是最先可用的位置）。

```c
for (i = 0; i < TX_RING_SIZE; i++) {
  tx_ring[i].status = E1000_TXD_STAT_DD;
  tx_ring[i].addr = 0;
}
```

`status` 字段中的 `E1000_TXD_STAT_DD`，初始化时会被设为 1，说明该值为 1 时代表这些位置是空闲可用的，可以直接填入数据。

此外还需要单独维护一个数组，记录每个位置对应挂载的指针，用于释放内存——例如上一次发送完成的包，可以通过这个数组找到对应指针并释放。数组大小与 `tx_ring` 保持一致即可。

```c
static struct mbuf *tx_mbufs[TX_RING_SIZE];
```

在编写代码之前，先梳理整体思路：拿到数据包后，根据 `tdt` 得到环中的位置，检查 `dd` 位判断该位置是否已处理完成；若可写入则填入数据包，若 ring 已满则返回错误；填入时需将 `status` 置为 0，表示该位置已有数据待处理，同时更新 `tdt`。

提示：还需要设置 `cmd` 字段，这里用到两个标志位：`E1000_TXD_CMD_EOP` 标记"该描述符是这个包的最后一个描述符"，`E1000_TXD_CMD_RS` 告诉网卡"发送完成后请把状态写回描述符（即重新将 DD 位置 1）"。

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

至此 `e1000_transmit()` 部分已经完成。按照提示，需要开启两个命令行窗口：一个运行 `python3 nettest.py txone`，另一个通过 `make qemu` 进入 xv6 系统后运行 `nettest txone` 发送一个数据包。

随后在另一个命令行中运行 `tcpdump -XXnr packets.pcap`，结果类似下图所示，说明发送成功：

![2026-07-12 16.42.41](./2026-07-12%2016.42.41.png)


接下来看 `e1000_recv()`。观察初始化过程可以发现，`rdt` 被额外减了 1，因此使用时需要手动加 1 才能得到正确的索引。hint 中也提到了这一点：

```
regs[E1000_RDT] = RX_RING_SIZE - 1;
```

> First ask the E1000 for the ring index at which the next waiting received packet (if any) is located, by fetching the E1000_RDT register and adding one modulo RX_RING_SIZE.

同样地，`dd` 标记初始化时默认值为 0：`dd=0` 表示网卡尚未向该 buffer 写入数据，因此不可读；`dd=1` 表示 buffer 中已写入数据，可以开始读取。

由于一次中断可能收到多个包，建议使用 `while` 循环处理这部分逻辑，持续向后索引，直到没有新包为止。

> The e1000 can deliver more than one packet per interrupt

整理一下思路：该函数被调用时，从 `tail+1` 开始处理，这是最后一个 `dd` 为 0 的位置，说明可以填入新数据；在 `while` 循环中，每取到一个数据包就调用 `net_rx`，处理完成后清空该地址或用 `kalloc` 分配新空间，最后将 `status` 重置为默认值 0，并更新 `tail` 和 `rdt`，在环中向前推进一步。

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

同样根据 lab 提示，对新实现的功能进行测试：开启两个命令行窗口，一个运行 `make qemu` 进入操作系统，另一个运行 `nettest.py rxone` 向 xv6 发送数据包，收到数据后系统会自动显示接收成功。

![2026-07-12 16.42.27](./2026-07-12%2016.42.27.png)

![2026-07-12 16.42.48](./2026-07-12%2016.42.48.png)

随后在另一个命令行中运行 `tcpdump -XXnr packets.pcap`，结果如下图所示，说明接收同样成功：

![2026-07-12 16.42.41](./2026-07-12%2016.42.41.png)

## Part Two: UDP Receive

本部分为添加对 UDP 请求的支持。

涉及三个函数：`ip_rx()`、`sys_recv()`、`sys_bind()`。`sys_bind` 用于初始化，完成端口绑定；`ip_rx` 负责判断收到的包是否为 UDP，以及端口是否匹配已绑定的端口；`sys_recv` 则运行在用户进程侧，从队列中取出 `ip_rx` 存入的数据，并拷贝给用户（拷贝到当前 `proc` 下）。

按照惯例，先完成最基础的初始化部分：首先需要维护一个数组，记录哪些端口已被绑定及其使用状态；此外还需要一个 `queue` 存放收到的包，并记录已收到的数量。

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

此处新增了一个 `udp_pkt` 结构，参照原本的 UDP 结构改写而成，额外增加了两个字段：一是 `sip`（数据来源 IP），二是 `payload`（包内容数据，即 UDP 数据部分，不包括 eth/ip/udp 头）。由于 payload 大小存在上限，故使用一个定长数组来解决存储问题；后续会通过 `kalloc` 新分配一块内存，将包数据拷贝进去，再清空原来的 buffer。

该数组直接添加在 `net.c` 中，并通过 `sys_bind` 完成绑定。

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

接下来实现 `ip_rx`：先用结构体将包中的数据解析出来，判断是否为 UDP 类型的请求，再从数组中查找匹配绑定的端口，然后在该端口对应的环形队列中找到可存放数据包的位置，写入相关字段，并使用 `memmove` 迁移数据；最后更新 `head` 和 `count` 状态，确保 binding 中的数据准确无误。完成后调用 `wakeup` 唤醒等待进程。（关于这里 `wakeup` 对应的唤醒对象，会在下文的 `sys_recv` 中体现。）

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

最后一步是实现 `sys_recv`。它需要拷贝 src IP、`sport`、payload（最多 `maxlen` 字节），并返回拷贝的字节数；若没有数据包则进入等待。

> recv(short dport, int *src, short *sport, char *buf, int maxlen): This system call returns the payload of a UDP packet that arrives with destination port dport.
```c
recv(short dport, int *src, short *sport, char *buf, int maxlen)
```

直接来看实现结果。需要注意的是，`sys_recv` 只需收到一个 UDP 包即可返回，因此除了用于等待的 `while` 之外，不再需要其他循环。

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

首先通过 `argint` 和 `argaddr` 获取用户调用传入的参数，随后扫描整个 binding 数组，找到对应的端口。如果尚未收到数据，则调用 `sleep(&bindings[i], &netlock)` 挂起等待，只有在有数据到来时才会被唤醒——这正是前文 `ip_rx` 中调用 `wakeup` 唤醒的目标。之后依旧通过 `head` 计算索引，从环形队列中取出 payload，更新状态后，即可通过 `copyout` 将数据整体拷贝到用户进程中。

此外还需注意 `maxlen`：需要先根据用户传入的这个值计算出实际应拷贝的长度，再执行拷贝操作。

还有不要忘记添加 unbind 的逻辑，尽管 lab 里面提到，这里不会对 unbind 的逻辑打分，没有也不影响成绩，但为了解答完整，还是需要补全这部分

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


在完成 Part Two 的实现后，可以通过以下方式进行测试：在一个窗口中运行 `python3 nettest.py grade`，另一个窗口中运行 xv6 内的 `nettest grade`。若一切顺利，第一个窗口（即 xv6 外部、运行 python 的那个）应显示以下内容：

![2026-07-12 19.28.07](./2026-07-12%2019.28.07.png)

在 xv6 窗口中则可以看到以下内容：

![2026-07-12 19.28.03](./2026-07-12%2019.28.03.png)

以上结果说明整体正确。


## Lab 6 整体测试

完成所有练习后，执行以下命令对 Lab 6 进行完整测试，验证各功能实现的正确性：

```sh
./grade-lab-net
```

![2026-07-12 19.49.15](./2026-07-12%2019.49.15.png)


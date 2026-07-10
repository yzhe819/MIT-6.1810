# Lab 6：网络（Networking）

[English](./lab6-xv6-network-driver.md) · [中文](./lab6-xv6-network-driver-zh.md)

本实验你将为 xv6 编写一个网卡（NIC）设备驱动，并实现以太网/IP/UDP 协议栈中的“接收半部分”。

获取实验源码并切换到 `net` 分支：

```bash
$ git fetch
$ git checkout net
$ make clean
```

---

## 背景

开始编码前，建议先复习 [xv6 book](xv6/book-riscv-rev5.pdf) 第 6 章（中断与设备驱动）。

本实验使用 E1000 网络设备。对 xv6（和你编写的驱动）来说，它看起来像是连在真实以太网 LAN 上的真实硬件。实际上，E1000 与 LAN 都由 QEMU 仿真：

- xv6（guest）IP：`10.0.2.15`
- host（运行 QEMU 的机器）IP：`10.0.2.2`

当 xv6 通过 E1000 向 `10.0.2.2` 发包时，QEMU 会把该包递送到 host 上对应应用。

本实验使用 QEMU 的 user-mode networking stack，文档见：[User Networking (SLIRP)](https://wiki.qemu.org/Documentation/Networking#User_Networking_.28SLIRP.29)。

Makefile 已启用 user-mode 网络与 E1000 仿真，并将全部收发包记录到实验目录中的 `packets.pcap`。

可用以下命令查看抓包：

```bash
tcpdump -XXnr packets.pcap
```

实验新增的关键文件：

- `kernel/e1000.c`：E1000 初始化代码与待补全的收发函数
- `kernel/e1000_dev.h`：E1000 寄存器与标志位定义
- `kernel/net.c`、`kernel/net.h`：简化 IP/UDP/ARP 协议栈
- `kernel/pci.c`：xv6 启动时在 PCI 总线上查找 E1000

---

## Part One：NIC

你需要补全 `kernel/e1000.c` 中的 `e1000_transmit()` 与 `e1000_recv()`，使驱动可以正常收发包。

当 `make grade` 通过 **txone** 与 **rxone** 测试时，此部分完成。

实现过程中建议查阅 Intel E1000 [Software Developer's Manual](readings/8254x_GBe_SDM.pdf)，重点：

- 3.2 节：接收流程（跳过 3.2.8 与 3.2.9）
- 3.3.1/3.3.2/3.3.3 与 3.4 节：发送流程
- 13 节：寄存器参考

QEMU 仿真的是 **82540EM**，只需用到 E1000 基础功能即可完成本实验。

`e1000_init()`（已提供）会用 DMA 在内存里初始化 TX/RX ring：

- RX ring：`struct rx_desc` 描述符数组，指向可写入接收包的缓冲区
- TX ring：`struct tx_desc` 描述符数组，驱动把待发送包挂到这里

ring 大小分别是 `RX_RING_SIZE` 与 `TX_RING_SIZE`。

### 发送路径（TX）概要

`net.c` 发送包时会调用 `e1000_transmit()`，参数是由 `kalloc()` 分配的包缓冲区。

你的发送逻辑要做到：

1. 把包缓冲区指针写入 TX 描述符；
2. 仅在硬件将该描述符置 `E1000_TXD_STAT_DD` 后才 `kfree()` 对应缓冲区；
3. 正确推进 `E1000_TDT`。

### 接收路径（RX）概要

E1000 每收到一个包，就 DMA 写入 RX ring 的下一个描述符指向的缓冲区，并在适当时机触发中断（通过 PLIC）。

你的 `e1000_recv()` 要：

1. 扫描 RX ring 找到新完成描述符；
2. 将包交给 `net_rx()`；
3. 重新分配新缓冲区并补回描述符；
4. 更新 `E1000_RDT`。

### 寄存器访问

驱动通过内存映射寄存器数组 `regs` 与 E1000 交互，重点寄存器：

- `E1000_TDT`
- `E1000_RDT`

### Part One 测试方式

`txone`：

1. host 窗口运行 `python3 nettest.py txone`
2. xv6 窗口运行 `make qemu` 后执行 `nettest txone`

成功时 host 侧会显示 `txone: OK`。

`rxone`：

1. xv6 窗口运行 `make qemu`
2. host 窗口运行 `python3 nettest.py rxone`

预期 xv6 日志包含：

- `arp_rx: received an ARP packet`
- `ip_rx: received an IP packet`

若 TX/RX 都正确，`make grade` 前两项应通过。

### e1000 相关提示

#### `e1000_transmit`

- 可先在 `e1000_transmit()`/`e1000_recv()` 中加调试打印，运行 `nettest txone` 观察调用。
- `e1000_dev.h` 使用的是 legacy TX descriptor 格式（手册 3.3.3）。
- 先读 `E1000_TDT` 获取“下一包应放入”的 ring 下标。
- 检查 ring 是否满：若该描述符未置 `E1000_TXD_STAT_DD`，说明上一轮发送未完成，应返回错误。
- 如该描述符上有旧缓冲区，先 `kfree()`。
- 填写描述符并设置所需 cmd 标志位。
- `E1000_TDT` 以 `TX_RING_SIZE` 为模推进。

#### `e1000_recv`

- 从 `E1000_RDT + 1`（模 `RX_RING_SIZE`）定位下一可能已到包描述符。
- 检查 `E1000_RXD_STAT_DD`，若未置位则停止。
- 调用 `net_rx()` 交付包。
- 用 `kalloc()` 分配新缓冲区补回该描述符，清零 status 位。
- 将 `E1000_RDT` 更新为“最后处理到的描述符”。
- 可参考 `e1000_init()` 初始化 RX ring 的写法。
- 要处理总包数超过 ring 大小（16）后的回绕。
- 一次中断可能对应多个包，`e1000_recv()` 要循环处理。

需要加锁来处理并发场景（多进程使用 E1000，或中断上下文并发访问）。

---

## Part Two：UDP Receive

UDP（User Datagram Protocol）允许不同主机上的用户进程通过 IP 交换独立数据报。

这一部分需要你在 `kernel/net.c` 中补全 UDP 接收、排队与用户态读取逻辑。`net.c` 已有 UDP 发送路径（除 `e1000_transmit()` 由你实现）。

需要补全：

- `ip_rx()`
- `sys_recv()`
- `sys_bind()`

当 `make grade` 全部通过即完成。

可手动执行同样测试：

1. host 窗口：`python3 nettest.py grade`
2. xv6 窗口：`nettest grade`

host 侧预期开头：

```bash
$ python3 nettest.py grade
txone: OK
rxone: sending one UDP packet
```

xv6 侧预期包含：

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

### UDP 系统调用 API

- `send(short sport, int dst, short dport, char *buf, int len)`
  - 发送 UDP 负载到 `dst:dport`，源端口为 `sport`
  - 成功返回 `0`，失败 `-1`
- `recv(short dport, int *src, short *sport, char *buf, int maxlen)`
  - 读取目标端口 `dport` 的下一条包
  - 若队列为空则阻塞等待
  - 将源 IP 写入 `*src`，源端口写入 `*sport`，负载（最多 `maxlen` 字节）拷贝到 `buf`
  - 返回实际拷贝负载长度，失败返回 `-1`
- `bind(short port)`
  - 调用 `recv(port, ...)` 前应先绑定
  - 未绑定端口收到的包应丢弃
- `unbind(short port)`
  - 非必做（测试不依赖）

上述系统调用参数与返回中的地址/端口均采用 host byte order。

`user/nettest.c` 使用此 API。你只需补齐内核侧（`send()` 例外）。

### `ip_rx()` 队列行为

每个入站 IP 包，`ip_rx()` 应：

1. 判断是否 UDP
2. 判断目的端口是否已 bind
3. 满足则入队供 `recv()` 消费

队列上限规则：

- 每个目的端口最多缓存 16 个包
- 某端口已满时，该端口新包丢弃
- 一个端口丢包不应影响其他端口

缓冲区包格式：

- 14 字节 Ethernet 头
- 20 字节 IP 头
- 8 字节 UDP 头
- UDP payload

对应结构定义在 `kernel/net.h`。

必须关注字段：

- IP：`ip_p`、`ip_src`
- UDP：`dport`、`sport`、`ulen`

### 字节序注意

网络协议头多字节字段是 network byte order（大端），RISC-V 内存布局是小端。

使用：

- `ntohs()`：2 字节
- `ntohl()`：4 字节

`net_rx()` 中解析以太网 type 字段有示例。

此外，E1000 代码中的遗漏可能只会在 ping 测试阶段暴露（例如 ring 索引回绕）。

### 额外提示

- 设计一个结构维护已绑定端口与其包队列。
- `recv()` 的阻塞/唤醒可参考 `kernel/proc.c` 中 `sleep(void *chan, struct spinlock *lk)` 与 `wakeup(void *chan)`。
- `sys_recv()` 写入的是用户虚拟地址，需正确从内核拷贝到当前用户进程地址空间。
- 被复制到用户态或被丢弃的包都要及时释放。

---

## 提交实验

### 记录耗时

创建 `time.txt`，写入一个整数（实验耗时小时数），然后 `git add` + `git commit`。

### 回答问题

若实验有问答题，将答案写入 `answers-*.txt`，并 `git add` + `git commit`。

### 提交方式

通过 Gradescope 提交。需要 MIT 的 Gradescope 账号；课程加入码见 Piazza。需要帮助可参考[此链接](https://help.gradescope.com/article/gi7gm49peg-student-add-course#joining_a_course_using_a_course_code)。

运行 `make zipball` 生成 `lab.zip` 后上传。

若存在未提交/未跟踪文件，可能看到：

```text
 M hello.c
?? bar.c
?? foo.pyc
Untracked files will not be handed in.  Continue? [y/N]
```

确保解题必需文件都已被跟踪（不在 `??` 行）。可用 `git add {filename}`。

- 提交前运行 `make grade`。
- 运行 `make zipball` 前先提交源码修改。
- Gradescope 实验分数即最终实验成绩。

---

## Optional Challenges（可选挑战）

- 本实验在入站处理用中断，但出站未采用。可实现更高级策略：软件队列缓存出站包，仅向 NIC 提供有限数量描述符，并依赖 TX 中断回填 TX ring，从而支持不同出站流量优先级。
- 实现完整 [ARP cache](https://tools.ietf.org/html/rfc826)。
- E1000 支持多 RX/TX rings。可配置“每核一组 ring”，并改造协议栈以提升吞吐、降低锁竞争（QEMU 下较难测试/量化）。
- 检测 [ICMP](https://tools.ietf.org/html/rfc792) 失败通知并向用户进程传播错误。
- 使用 E1000 的无状态硬件 offload（如 checksum、RSC、GRO）提升吞吐（QEMU 下较难测试/量化）。
- 当前栈易受 receive livelock 影响。可结合课程资料设计并实现修复方案（较难测试）。
- 实现最小 TCP 协议栈并下载网页。

其中一些性能优化在 QEMU 下可能不明显或难以测量。

如果你做了任何挑战题（不局限于网络方向），请告知课程组。

---

*如有关于 6.1810 的问题或建议，请邮件联系课程组：61810-staff@lists.csail.mit.edu。*

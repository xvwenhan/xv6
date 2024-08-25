#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; //将 E1000_IMS 寄存器清零，以禁用中断
  regs[E1000_CTL] |= E1000_CTL_RST;//将 E1000_CTL 寄存器的 E1000_CTL_RST 位设置为 1，以复位网卡
  regs[E1000_IMS] = 0; //再次将 E1000_IMS 寄存器清零，确保中断被禁用
  __sync_synchronize();//同步，确保对寄存器的修改立即生效

  // [E1000 14.5] Transmit initialization
  //初始化传输环形缓冲区
  memset(tx_ring, 0, sizeof(tx_ring));//将 tx_ring 清零
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;//将每个传输描述符的状态设为 E1000_TXD_STAT_DD（描述符已完成）
    tx_mbufs[i] = 0;//将 tx_mbufs 数组清零
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;//将传输描述符基址设置为 tx_ring 的地址
  if(sizeof(tx_ring) % 128 != 0)//检查tx_ring 是否按 128 字节对齐
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);//设置传输描述符的长度
  regs[E1000_TDH] = regs[E1000_TDT] = 0;//头指针和尾指针设为 0，表示初始状态为空
  
  // [E1000 14.4] Receive initialization
  //初始化接收环形缓冲区
  memset(rx_ring, 0, sizeof(rx_ring));//将 rx_ring 清零
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);//为每个接收描述符分配内存缓冲区，并将其地址存储在 rx_mbufs 中
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;//将每个接收描述符的地址设置为对应的内存缓冲区地址
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;//将接收描述符基址设置为 rx_ring 的地址
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;//头指针设为0，表示初始状态为空
  regs[E1000_RDT] = RX_RING_SIZE - 1;//尾指针设置为RX_RING_SIZE - 1，表示初始状态为空
  regs[E1000_RDLEN] = sizeof(rx_ring);//设置接收描述符的长度

  //配置MAC地址过滤
  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;//设置网卡的MAC地址为 52:54:00:12:34:56
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;//将多播地址表清零

  // transmitter control bits.
  //配置传输控制
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  //配置接收控制
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  //请求接收中断
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
acquire(&e1000_lock); 

  uint32 ind = regs[E1000_TDT]; //从E1000_TDT寄存器中获取传输描述符环形缓冲区中下一个可用描述符的下标 ind
  struct tx_desc *desc = &tx_ring[ind]; //获取传输描述符指针desc。tx_ring 是传输描述符环形缓冲区的数组
  if(!(desc->status & E1000_TXD_STAT_DD)) {//检查描述符中上次传输的数据包是否已经发送完毕
    release(&e1000_lock);
    return -1;//如果未设置该位，说明 NIC 还在使用该描述符，因此无法覆盖此描述符中的数据
  }
  //释放原来的缓冲区
  if(tx_mbufs[ind]!= 0) {//tx_mbufs[ind] 中已经存储了一个 mbuf
    mbuffree(tx_mbufs[ind]);//释放旧的 mbuf
    tx_mbufs[ind] = 0;//tx_mbufs[ind] 置为 0
  }
  //使用描述符指针desc更改tx_ring[ind]的内容（tx_ring[ind]是一个描述符）
  desc->addr = (uint64)m->head;
  desc->length = m->len;
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;//设置描述符的 cmd 字段，这里设置了两个标志：
  //E1000_TXD_CMD_EOP (End of Packet): 表示这个描述符是数据包的结束。
  //E1000_TXD_CMD_RS (Report Status): 表示在数据包传输完成后需要更新描述符的状态。
  tx_mbufs[ind] = m;//tx_mbufs储存的是数据包buf的地址，即tx_mbufs是一个存放指针的数组。
  // 更新下一个槽位下标
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;// % TX_RING_SIZE 操作确保下标在环形缓冲区范围内循环

  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  while(1) { //持续检查接收缓冲区中是否有新的数据包到达
  	//regs[E1000_RDT] 是接收描述符环形缓冲区的尾指针（RDT）。它指示了硬件最后处理的接收描述符的索引。
    uint32 ind = (regs[E1000_RDT] + 1) % RX_RING_SIZE;// 计算出下一个可供软件读取的描述符的索引
    //获取描述符指针
    struct rx_desc *desc = &rx_ring[ind];
    if(!(desc->status & E1000_RXD_STAT_DD)) {//E1000_RXD_STAT_DD 位被设置，表示该数据包已被接收完成
      return;
    }
    rx_mbufs[ind]->len = desc->length;//更新 mbuf 的长度，让上层知道数据包的实际长度
    net_rx(rx_mbufs[ind]); //将接收到的数据包交给操作系统的网络栈进行进一步处理。

    //因为当前的 mbuf 已经传递给了上层处理，接收描述符对应的缓冲区需要准备好接收下一个数据包，
    //因此需要分配一个新的 mbuf
    rx_mbufs[ind] = mbufalloc(0);
    desc->addr = (uint64)rx_mbufs[ind]->head;//将新的 mbuf 的地址存储到描述符的 addr 字段中，以便硬件知道将下一个接收的数据包存储到新的 mbuf 中
    desc->status = 0;// 将描述符的 status 字段清零，以准备好接收下一个数据包

    regs[E1000_RDT] = ind;//更新尾指针: 更新尾指针 regs[E1000_RDT]，将其指向当前处理的描述符索引 ind
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}

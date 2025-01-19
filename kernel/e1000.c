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
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
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

  /* 需要锁，防止多个线程的以下可能竞态：
  1、多个线程可能同时读取并更新E1000_TDT寄存器 
  2、mbuf多次释放冲突 
  3、一个线程正在检查tx_ring[i].status是否为完成状态（E1000_TXD_STAT_DD）时，另一个线程可能已经改变了这个状态，但第一个线程仍然依据旧的状态做出决策。
  4、多个线程可能同时尝试写入相同的描述符位置。这会导致一个线程的数据被另一个线程的数据覆盖*/
  acquire(&e1000_lock);

  uint32 i = regs[E1000_TDT];//TDT存放网卡要发送的下一个Descripotr的索引

  // check whether the E1000 has finished transmitting the packet or not
  if((tx_ring[i].status & E1000_TXD_STAT_DD) == 0){
    release(&e1000_lock);//the E1000 has not finished transmitting , then return
    return -1;
  }

  // tx_mbufs[i]缓存的是上一次调用e1000_transmit的参数struct mbuf *m，现在已经确认成功发送，故free释放
  if(tx_mbufs[i])
    mbuffree(tx_mbufs[i]);

  // fill in control info and content
  tx_ring[i].addr = (uint64)m->head;
  tx_ring[i].length = (uint16)m->len;
  tx_ring[i].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  /*E1000_TXD_CMD_EOP (End of Packet) 表示这是当前要发送的数据包的最后一段。这对于分段的数据包非常重要，因为网络适配器需要知道什么时候结束一个数据包的发送。 
    E1000_TXD_CMD_RS (Report Status) 指示网络适配器在完成该描述符所指示的数据包发送后，应该更新描述符的状态，并且可以在必要时触发中断通知主机。*/

  // 缓存这次发送的mbuf结构体(m)，以便在下一次调用e1000_transmit函数，并且确认数据包已经被成功发送之后能够释放它
  tx_mbufs[i] = m;

  // 更新 E1000_TDT ，供下次调用e1000_transmit
  i = (i + 1) % TX_RING_SIZE;
  regs[E1000_TDT] = i;

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
  while (1) {
    uint32 i = regs[E1000_RDT];// RDT指示最近被软件更新的描述符的位置
    i = (i + 1) % RX_RING_SIZE;
    // 检查当前描述符是否已经接收数据  E1000_RXD_STAT_DD 标志位表示数据已被接收。
    if((rx_ring[i].status & E1000_RXD_STAT_DD) == 0)
    {
      return;// 如果没有设置，则说明还没有新的数据包到达，函数直接返回
    }
    rx_mbufs[i]->len = rx_ring[i].length;// 由网卡硬件设置，表示接收到的数据包的实际大小。

    // 交付网络栈处理网卡收到的rx_mbufs[i]
    net_rx(rx_mbufs[i]);

    //分配新的rx_mbufs
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000 mbufalloc");

    // 更新接收描述符，把新分配的rx_mbufs加入rx_ring
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
    rx_ring[i].status = 0;

    // 更新RDT寄存器，指向下一个描述符
    regs[E1000_RDT] = i;
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

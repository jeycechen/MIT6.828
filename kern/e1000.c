#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

#define E1000REG(offset) (void *)(e1000_reg + offset)

struct e1000_tdh* tdh_reg;
struct e1000_tdt* tdt_reg;
volatile uint32_t *e1000_reg;
struct tx_desc tx_desc_list[TXDESCS]; 
char tx_buffer[TXDESCS][TX_PKT_SIZE] __attribute__((aligned(4096))); //不是要连续的物理内存吗？这貌似不连续？TODO 无法使用DMA

struct rd_desc rd_desc_list[RDDESCS];

// LAB 6: Your driver code here
int E1000_attachfn(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000_reg = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]); // 分配一个MMIOBASE区域的虚拟地址 映射到 这个物理地址
    
    uint32_t *status_reg = (uint32_t *) E1000REG(E1000_STATUS / 4);
    cprintf("e1000_status_reg %x \n", *status_reg);
    assert(*status_reg == 0x80080783);

    transmit_init();
    return 0;
}

void transmit_init(){
    // 1. 分配一块内存区域 用作传输描述符列表。
    for(int i = 0; i < TXDESCS; ++i){
        tx_desc_list[i].addr = PADDR(tx_buffer[i]); // 实际操作的是物理地址
        tx_desc_list[i].cmd = 0;
        tx_desc_list[i].status |= E1000_TXD_STAT_DD; // 初始所有的描述符都已准备好
    }
    
    // 2. 设置Trasnmit Descriptor Lenght TDLEN 寄存器为描述符列表的长度. 128-b 对齐
    struct e1000_tdlen* tdlen_reg = (struct e1000_tdlen*) E1000REG(E1000_TDLEN / 4);
    tdlen_reg->len = TXDESCS * sizeof(struct tx_desc); 

    // extra. set Transmit Des Base Addr reg (TDBAL/TDBAH) 
    // struct e1000_tdbal* tdbal_reg = (struct e1000_tdbal*) E1000REG(E1000_TDBAL / 4);
    // struct e1000_tdbah* tdbah_reg = (struct e1000_tdbah*) E1000REG(E1000_TDBAH / 4);

    // tdbal_reg->tdbal = PADDR(tx_desc_list); // 低地址 // 这里可能会发生截断 所以显示不符合预期
    // tdbah_reg->tdbah = 0; // 高地址

    uint32_t *tdbal = (uint32_t *)E1000REG(E1000_TDBAL / 4);
    *tdbal = PADDR(tx_desc_list);

			 //TDBAH regsiter
    uint32_t *tdbah = (uint32_t *)E1000REG(E1000_TDBAH / 4);
    *tdbah = 0;
    
    // 3. TDH/TDT寄存器 在上电或者软件初始化以太网控制器重启的时候 将会被硬件初始化为0b， 软件在这些寄存器需要写入0b从而进一步保证0b
    tdh_reg = (struct e1000_tdh*) E1000REG(E1000_TDH / 4);
    tdh_reg->tdh = 0;
    tdt_reg = (struct e1000_tdt*) E1000REG(E1000_TDT / 4);
    tdt_reg->tdt = 0;

    // 4. 初始化传输控制寄存器TCTL 
    // uint32_t* tctl_reg = (uint32_t *) E1000REG(E1000_TCTL / 4);
    // *tctl_reg = (E1000_TCTL_EN) | (E1000_TCTL_PSP) | (0x10 << E1000_TCTL_CT) | (0x40 << E1000_TCTL_COLD);
    volatile struct e1000_tctl* tctl_reg = (struct e1000_tctl*) E1000REG(E1000_TCTL / 4);
    
    // 4.1 设置TCTL.EN 1b
    tctl_reg->en = 1;

    // 4.2 设置 Pad Short Packets（TCTL.PSP）1b
    tctl_reg->psp = 1;

    // 4.3 配置 Collision Threshold（TCTL.CT) 为合适的值，以太网为10h，This setting only has meaning in half duplex mode.
    tctl_reg->ct = 0x10;

    // 4.4 配置 Collision Distance（TCTL.COLD) 为合适的值，
    /* 
        * full duplex 40h.
        * gigabit half duplex 200h.
        * 10/100 half duplex 40h.
    */
    tctl_reg->cold = 0x40;
    
    // 5. 按照手册编写TIPG寄存器，以获取最小合法数据包间隙, ref 13.4.34 table 13-77
    struct e1000_tipg* tipg_reg = (struct e1000_tipg*) E1000REG(E1000_TIPG / 4);
    tipg_reg->ipgt = 10;
    tipg_reg->ipgr1 = 4;
    tipg_reg->ipgr2 = 6;

    // init finished.
}

void receive_init(){
    // 完成e1000网卡传输过程所需要的初始化过程。 refer to https://pdos.csail.mit.edu/6.828/2017/readings/hardware/8254x_GBe_SDM.pdf 14.4
    // 1. 接收地址寄存器RAL/ RAH为以合适的太网地址 MAC地址

    // 2. 初始化MTA（Multicast Table Array）为0

    // 3. 配置Interrupt Mask Set/Read (IMS)寄存器 以启用软件驱动程序希望在事件发生时收到通知的任何中断， RXT RXO RXDMT RXSEQ LSC suggested

    // 4. Receive Descriptor Minimum Threshold Interrupt 

    // 5. 分配一部分区域为接收描述符队列， 配置 RDBAL/RDBAH 寄存器

    // 6. 设置Receive Descriptor Length（RDLEN)寄存器为 (in bytes) 描述符队列的长度

    // 7. 

}
int send_package(char* msg, ssize_t len){ // 发送数据包
    // 将数据复制到传输描述符中。插入到尾部。
    int idx = tdt_reg->tdt;
    if(!(tx_desc_list[idx].status & E1000_TXD_STAT_DD)) {
        return -E_TRANSMIT_RETRY;
    }
    if(len > TX_PKT_SIZE) {
        return -E_INVAL;
    }
    tx_desc_list[idx].length = len;
    tx_desc_list[idx].cmd = (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
    tx_desc_list[idx].status &= ~E1000_TXD_STAT_DD;
    
    memcpy(KADDR(tx_desc_list[idx].addr), msg, len); // 复制到缓冲区
    //memcpy(tx_desc_list[idx].addr, msg, len) 这句会报内核错误，为啥，因为这里addr被初始化为了一个物理地址，而memcpy应该使用虚拟地址。
    
    // 内存屏障，确保描述符更新完成
    
    // __sync_synchronize();

    // 更新TDT寄存器（环形缓冲区指针）
    tdt_reg->tdt = (idx + 1) % TXDESCS;
    return 0;
}
#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H


#include <kern/pci.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEV_ID    0x100E

// 传输相关的宏
#define E1000_STATUS    0x00008  /* Device Status - RO */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */

#define TX_PKT_SIZE 2048 // 实际上是max是1518 但是为了避免跨页 选用2048 
#define TXDESCS     32


// 接收相关的宏
#define RDDESCS     128 // lab6文档明确指出至少需要128个描述符 

// 传输相关的结构体
struct tx_desc //描述符结构体
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
}__attribute__((packed)); //取消结构体的内存对齐，使得结构体按紧凑方式存储。

struct e1000_tdbal{
    uint32_t zero: 4;
    uint32_t tdbal: 28;
};

struct e1000_tdbah{
    uint32_t tdbah;
};

struct e1000_tdlen{
    uint32_t zero:  7;
    uint32_t len:    13;
    uint32_t rsv:   12;
}; // 位字段（bit field) 语法 用于在一个32位证书精确划分不同功能的位段。

struct e1000_tdt{
    uint16_t tdt;
    uint16_t rsv;
};

struct e1000_tdh{
    uint16_t tdh;
    uint16_t rsv;
};

struct e1000_tctl{
    uint32_t rsv1:  1;
    uint32_t en:    1;
    uint32_t rsv2:  1;
    uint32_t psp:  1;
    uint32_t ct:    8;
    uint32_t cold:  12;
    uint32_t swxoff:    1;
    uint32_t rsv:   8;
};

struct e1000_tipg
{
    uint32_t ipgt:  10;
    uint32_t ipgr1: 10;
    uint32_t ipgr2: 10;
    uint32_t rsv:   2;
};

extern volatile uint32_t *e1000_reg; // 为什么需要extern

// 接收相关的结构体
struct rd_desc{
    uint64_t addr;
    uint16_t len;
    uint16_t checksum;
    uint8_t status;
    uint8_t err;
    uint16_t special;
}__attribute__((packed));
enum {
    E_TRANSMIT_RETRY = 1,
    E_RECEIVE_RETRY
};
int E1000_attachfn(struct pci_func *pcif);
void transmit_init(); //传输的初始化函数

int send_package(char* msg, ssize_t len);
#endif  // SOL >= 6
#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#endif  // SOL >= 6

#include <kern/pci.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEV_ID    0x100E
#define E1000_STATUS   0x00008  /* Device Status - RO */

extern volatile uint32_t *e1000_reg; // 为什么需要extern
int E1000_attachfn(struct pci_func *pcif);
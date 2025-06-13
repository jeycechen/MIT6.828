#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000_reg;

#define E1000REG(offset) (void *)(e1000_reg + offset)

// LAB 6: Your driver code here
int E1000_attachfn(struct pci_func *pcif){
    pci_func_enable(pcif);
    e1000_reg = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]); // 分配一个MMIOBASE区域的虚拟地址 映射到 这个物理地址
    
    uint32_t *status_reg = (uint32_t *) E1000REG(E1000_STATUS / 4);
    cprintf("e1000_status_reg %x \n", *status_reg);
    assert(*status_reg == 0x80080783);
    return 0;
}
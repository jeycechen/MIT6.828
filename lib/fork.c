// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf) // copy-on-write 页面错误处理程序
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW))) //确保这个错误是写页面的时候发生的，且 确保页面是写时复制
		panic("pgfault: invalid UTrapFrame");
	
	addr = ROUNDDOWN(addr, PGSIZE); 
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// 分配一个新页 到 临时地址PFTEMP， 将原来的需要复制的页面先复制到这个新页面上
	// 然后将这个 新页面 映射到 addr这个地址上
	// 最后取消 新页面对PFTEMP 的映射关系
	// 为什么要这么做，如果直接分配一个新页 到 addr，那么就无法获取原来的页面了（被新分配的页面顶掉了）
	// LAB 4: Your code here.
	envid_t envid = sys_getenvid();
	if(sys_page_alloc(envid, (void *)PFTEMP, PTE_U | PTE_W | PTE_P) < 0) //分配一个物理页
        panic("pgfault: sys_page_alloc failed.");
	memcpy((void *)PFTEMP, addr, PGSIZE);
    if(sys_page_map(envid, PFTEMP, envid, addr, PTE_U | PTE_W | PTE_P) < 0) 
        panic("pgfault: sys_page_map failed.");
    if(sys_page_unmap(envid, (void *)PFTEMP) < 0)
        panic("pgfault: sys_page_unmap failed.");
	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 4: Your code here.
	// panic("duppage not implemented");
	void *addr = (void *)(pn * PGSIZE);
	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		if(sys_page_map(0, addr, envid, addr, PTE_U | PTE_P | PTE_COW) < 0) // 为什么这里是0，0代表当前的env（进程）吗
			panic("duppage: patent->child sys_page_map failed.");
		if(sys_page_map(0, addr, 0, addr, PTE_U | PTE_COW | PTE_P) < 0)  // 重新标记为PTE_COW， 为什么需要重新标记 多核 + 多进程？
			panic("duppage: single sys_page_map failed.");
	} else { // 否则这个页面就是只读的 只需要映射就行（复制）
		if(sys_page_map(0, addr, envid, addr, PTE_P | PTE_U) < 0)
			panic("duppage: single sys_page_map failed.");
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	extern void _pgfault_upcall(void); //这是一个汇编程序
	set_pgfault_handler(pgfault); // 设置缺页处理函数
	envid_t env_id = sys_exofork(); // 系统调用，只是简单创建一个Env结构，复制当前用户环境寄存器状态，UTOP以下的页目录还没有建立
	if(env_id < 0) 
		panic("fork: sys_exofork failed.");
	if(env_id == 0){ // 0 表示 子进程 也就是子进程更新thisenv 然后ret
		// thisenv = envs + ENVX(sys_getenvid());
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	uint32_t addr; // 这里是父进程 父进程的环境 env_id 返回的是 子进程的env_id
	// uvpt page table 页表 uvpd 页目录 page directory
	// cprintf("thisid: %d, son's id: %d\n",thisenv->env_id, env_id);
	for(addr = 0; addr < USTACKTOP; addr += PGSIZE){
		if((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U)) // 在内存中 才复制 如果不在内存中，又会怎么样？？
			duppage(env_id, PGNUM(addr));
	}
	
	if (sys_page_alloc(env_id, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P) < 0) // 这一步的作用是什么？ 分配一个初始的UXSTK栈
        panic("fork: sys_page_alloc failed.");

	sys_env_set_pgfault_upcall(env_id, _pgfault_upcall); //为子进程 设置页面错误处理程序upcall

    if (sys_env_set_status(env_id, ENV_RUNNABLE) < 0) { // 设置子进程为可运行
        panic("fork: sys_env_set_status failed.");
	}
	
    return env_id; // 返回子进程的id
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

    // LAB 3: Your code here.
    void handler_divide();
	SETGATE(idt[T_DIVIDE], 0, GD_KT, handler_divide, 0);
	void handler_debug();
	SETGATE(idt[T_DEBUG], 0, GD_KT, handler_debug, 0);
	void handler_nmi();
	SETGATE(idt[T_NMI], 0, GD_KT, handler_nmi, 0);
	void handler_brkpt();
	SETGATE(idt[T_BRKPT], 0, GD_KT, handler_brkpt, 3);
	void handler_oflow();
	SETGATE(idt[T_OFLOW], 0, GD_KT, handler_oflow, 0);
	void handler_bound();
	SETGATE(idt[T_BOUND], 0, GD_KT, handler_bound, 0);
	void handler_illop();
	SETGATE(idt[T_ILLOP], 0, GD_KT, handler_illop, 0);
	void handler_device();
	SETGATE(idt[T_DEVICE], 0, GD_KT, handler_device, 0);
	void handler_dblflt();
	SETGATE(idt[T_DBLFLT], 0, GD_KT, handler_dblflt, 0);
	void handler_tss();
	SETGATE(idt[T_TSS], 0, GD_KT, handler_tss, 0);
	void handler_segnp();
	SETGATE(idt[T_SEGNP], 0, GD_KT, handler_segnp, 0);
	void handler_stack();
	SETGATE(idt[T_STACK], 0, GD_KT, handler_stack, 0);
	void handler_gpflt();
	SETGATE(idt[T_GPFLT], 0, GD_KT, handler_gpflt, 0);
	void handler_pgflt();
	SETGATE(idt[T_PGFLT], 0, GD_KT, handler_pgflt, 0);
	void handler_fperr();
	SETGATE(idt[T_FPERR], 0, GD_KT, handler_fperr, 0);
	void handler_align();
	SETGATE(idt[T_ALIGN], 0, GD_KT, handler_align, 0);
	void handler_mchk();
	SETGATE(idt[T_MCHK], 0, GD_KT, handler_mchk, 0);
	void handler_simderr();
	SETGATE(idt[T_SIMDERR], 0, GD_KT, handler_simderr, 0);
	void handler_syscall();
	SETGATE(idt[T_SYSCALL], 0, GD_KT, handler_syscall, 3);
	void handler_default();
	SETGATE(idt[T_DEFAULT], 0, GD_KT, handler_default, 0);
	
	// 这里是增加的硬件中断
	void handler_timer();
	void handler_kbd();
	void handler_serial();
	void handler_spurious();
	void handler_ide();
	void handler_error();
	
	SETGATE(idt[IRQ_OFFSET + IRQ_TIMER],    0, GD_KT, handler_timer,    0);
	SETGATE(idt[IRQ_OFFSET + IRQ_KBD],      0, GD_KT, handler_kbd,      0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL],   0, GD_KT, handler_serial,   0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SPURIOUS], 0, GD_KT, handler_spurious, 0);
	SETGATE(idt[IRQ_OFFSET + IRQ_IDE],      0, GD_KT, handler_ide,      0);
	SETGATE(idt[IRQ_OFFSET + IRQ_ERROR],    0, GD_KT, handler_error,    0);
	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
	//     from doing IO (0 is not the correct value!)
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:
	/// Setup a TSS so that we get the right stack
	// // when we trap to the kernel.
	// ts.ts_esp0 = KSTACKTOP;
	// ts.ts_ss0 = GD_KD;
	// ts.ts_iomb = sizeof(struct Taskstate);
	struct Taskstate *thists = &thiscpu->cpu_ts;
	
    // thists->ts_esp0 = KSTACKTOP - thiscpu->cpu_id * (KSTKSIZE + KSTKGAP);
	thists->ts_esp0 = (uintptr_t) percpu_kstacks[cpunum()];
    thists->ts_ss0 = GD_KD;
    thists->ts_iomb = sizeof(struct Taskstate);
	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id] = SEG16(STS_T32A, (uint32_t) (thists),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + (thiscpu->cpu_id << 3));

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	// cprintf("trap_dispatch.. \n");
	if(tf->tf_trapno == T_PGFLT){
		// cprintf("page_fault handler\n");
		page_fault_handler(tf);
		return;
	}
	if(tf->tf_trapno == T_BRKPT){
		monitor(tf);
		return;
	}
	if(tf->tf_trapno == T_SYSCALL){
		tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx, tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
		return;
	}
	
	// Handle spurious interrupts // 虚假中断 IRQ line上的噪声
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}
	
	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	// Add time tick increment to clock interrupts.
	// Be careful! In multiprocessors, clock interrupts are
	// triggered on every CPU.
	// LAB 6: Your code here.


	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_KBD){
		kbd_intr();
		return;
	}
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL){
		serial_intr();
		return;
	}
	// 时钟中断 
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER){
		lapic_eoi(); // lapic local advanced programmable interrupt contrloller
		// 硬件设备产生中断的时候 中断控制器会接受信号，然后传递给CPU，CPU响应中断之后，会暂停当前任务，去执行中断处理程序
		// CPU完成处理之后， 告知lapic处理完毕，以便处理后续的中断。lapic_eoi就是用来通知lapci的 （eoi, end of interrupt）
		// 会向本地lapic发送一个EOI信号，1：清楚中断标志位 2、允许后续中断
		sched_yield();
		return;
	}
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT){
		panic("unhandled trap in kernel");
		return;
	}
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	// 检查中断被屏蔽，如果assert失败，不要试图使用"cli" fix it 也就是陷入中断的时候要保证屏蔽了中断，否则会嵌套... 不安全
	// cprintf("assert %s \n",read_eflags() & FL_IF);
	// IF标志位 置位 表示允许 响应外部的可屏蔽中断请求， 否则 0 就是不允许响应外部的可屏蔽中断
	// cprintf("tf_eip: %x\n", tf->tf_eip);
	// cprintf("tf_trapno: %s\n",trapname(tf->tf_trapno));
	// cprintf("syscallno: %d\n",tf->tf_regs.reg_eax);
	assert(!(read_eflags() & FL_IF)); // 标志位 是 0 才允许通过 TODO

	if ((tf->tf_cs & 3) == 3) { // 用户导致的中断
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();
		assert(curenv);
		
		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}
		// 在这之前的tf是在栈上的，不稳定
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf; // 保存curenv使用的trapfram方便恢复
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf; // 这里是在干嘛？ 是否有些多余
		// 在这里，tf复制到了curenv里面，这是放在内核的全局变量的，比较稳定
		//1、确保后续操作使用正确的陷阱帧：通过复制陷阱帧并更新指针，后续的代码可以一致地使用保存在环境变量中的陷阱帧副本，而不必担心栈上的原始陷阱帧可能会被覆盖或修改。
		//2、方便恢复环境：保存了陷阱帧的副本后，可以在需要时方便地恢复环境，因为可以直接从环境变量中获取陷阱帧信息，而不需要再次从栈上获取可能已经不可靠的陷阱帧
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf); // 中断处理完成，继续执行curenv

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf) // 这里是一个内核级别中断
{
	uint32_t fault_va;
	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	
	// LAB 3: Your code here.
	// return;

	if((tf->tf_cs & 3) == 0) // 内核页面 出错 直接寄
		panic("Page fault in kernel-mode\n");

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// It is convenient for our code which returns from a page fault
	// (lib/pfentry.S) to have one word of scratch space at the top of the
	// trap-time stack; it allows us to more easily restore the eip/esp. In
	// the non-recursive case, we don't have to worry about this because
	// the top of the regular user stack is free.  In the recursive case,
	// this means we have to leave an extra word between the current top of
	// the exception stack and the new stack frame because the exception
	// stack _is_ the trap-time stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	if(curenv->env_pgfault_upcall){ // 用户注册的中断处理函数非空
		struct UTrapframe *utf; // 用户级别中断 帧
		uintptr_t addr;
		// UXSTKTOP  high
		//				PG <------ tf->esp 如果在这里说明就是多次发生了用户级中断， 发生嵌套
		// 				PG
		// USTKTOP   low
		if(UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp < UXSTACKTOP)
			addr = tf->tf_esp - sizeof(struct UTrapframe) - 4; // 不是第一次， 已经发生嵌套
			// 为什么这里需要额外-4 ？方便恢复现场
		else
			addr = UXSTACKTOP - sizeof(struct UTrapframe); // 第一次进入user exception stack
		user_mem_assert(curenv, (void *) addr, sizeof(struct UTrapframe), PTE_W | PTE_P | PTE_U);
		// 构建utf
		utf = (struct UTrapframe *)addr;
		utf->utf_fault_va = fault_va;
        utf->utf_err = tf->tf_err;
        utf->utf_regs = tf->tf_regs;
        utf->utf_eip = tf->tf_eip; //用户能够返回到用户态的设置
        utf->utf_eflags = tf->tf_eflags;
        utf->utf_esp = tf->tf_esp;

		// Branch to handler function
		curenv->env_tf.tf_eip = (uint32_t) curenv->env_pgfault_upcall;
        curenv->env_tf.tf_esp = addr; 

		env_run(curenv); //继续运行？其实就会执行 curenv->env_pgfault_upcall;
	}
	// Destroy the environment that caused the fault.
	// 为什么需要destroy 这个env？？ 直接销毁导致页面错误的env
	
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}


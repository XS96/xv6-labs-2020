// Physical memory layout
// xv6物理地址内核布局
// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L                       // UART（通用异步收发传输器）设备的基地址 物理地址
#define UART0_IRQ 10                            // 设备的中断号，为 10

// virtio mmio interface
#define VIRTIO0 0x10001000                      //  Virtio 设备的基地址，物理地址为 0x10001000。Virtio 是一种标准的虚拟设备接口，常用于虚拟化环境
#define VIRTIO0_IRQ 1

// local interrupt controller, which contains the timer.
#define CLINT 0x2000000L                        // 本地中断控制器的基地址
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))        // 宏，用于获取 hart（硬件线程）对应的比较寄存器地址。hartid 是硬件线程的 ID
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.          定义了一个寄存器地址，用于获取自启动以来的时间周期数

// qemu puts programmable interrupt controller here.
#define PLIC 0x0c000000L                        // 可编程中断控制器的基地址
// 定义了 PLIC 的各个寄存器地址，分别用于配置中断优先级、获取中断挂起状态、使能中断、设置中断优先级、声明中断处理等
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L                // 内核空间的基地址
// 0x8800 0000
#define PHYSTOP (KERNBASE + 128*1024*1024)  // 物理内存的终止地址，内核和用户页可以使用的内存范围是从 0x80000000 到 0x80000000 + 128MB

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)         // 跳板页的虚拟地址，映射到最高地址，用户态和内核态都可以访问。跳板页用于处理系统调用和中断

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)      // 每个进程的内核栈地址，在跳板页之下，并且每个内核栈都由无效的保护页（guard page）包围，以防止栈溢出

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)     // 陷阱帧的虚拟地址，位于跳板页的下方

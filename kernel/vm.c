#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

// lab3.2
#include "spinlock.h"
#include "proc.h"



/*
 * 内核页表
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code. 指向内核代码段的结束位置

extern char trampoline[]; // trampoline.S 指向 trampoline.S 中定义的汇编代码，用于处理陷阱（trap）进入和退出

/*
 * create a direct-map page table for the kernel.
 * 创建和初始化内核页表
 */
void
kvminit()
{
  // 分配一页物理内存来存放内核页表，并将其地址赋值给kernel_pagetable
  kernel_pagetable = (pagetable_t) kalloc();
  // 将这一页内存清零，确保页表初始状态为空
  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT 64KB
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC 4MB
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  // 将内核代码段映射到自身，从 KERNBASE 开始到 etext 结束
  // KERNBASE: 0x80000000 （虚拟地址）
  // etext:    kernel text的结束位置
  kvmmap(KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  // 将内核数据段和物理 RAM 映射到自身，从 etext 开始到 PHYSTOP 结束
  // etext:    kernel text的结束位置
  // PHYSTOP:  0x86400000 物理 RAM 的结束位置
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  // 将 trampoline 代码映射到虚拟地址空间的最高地址 TRAMPOLINE 处，大小为一页
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}

void
uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
    if(mappages(pagetable, va, sz, pa, perm) != 0)
        panic("uvmmap");
}

// lab3.2 为新进程创建新的内核页表
pagetable_t
prockvminit()
{
//  pagetable_t prock_pagetable = (pagetable_t) kalloc();
//  if(prock_pagetable == 0) return 0;
//  memset(prock_pagetable, 0, PGSIZE);
//// 直接映射
//  uvmmap(prock_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
//  uvmmap(prock_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
//  uvmmap(prock_pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
//  uvmmap(prock_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
//  uvmmap(prock_pagetable, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
//  uvmmap(prock_pagetable, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
//  uvmmap(prock_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
//  return prock_pagetable;

  pagetable_t kernelpt = uvmcreate();
  if (kernelpt == 0) return 0;
  uvmmap(kernelpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  uvmmap(kernelpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  uvmmap(kernelpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  uvmmap(kernelpt, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  uvmmap(kernelpt, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  uvmmap(kernelpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return kernelpt;
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
// 切换到内核的页表并启用分页
void
kvminithart()
{
  // 将参数写入到 satp 寄存器
  w_satp(MAKE_SATP(kernel_pagetable));
  // 汇编指令，用于刷新 TLB 当修改页表或切换页表时，必须刷新 TLB，以确保地址翻译使用的是最新的页表
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
//  用于遍历（walk）页表的函数 walk。该函数根据给定的虚拟地址 va 查找或创建对应的页表条目（PTE）
//  参数： 页表 pagetable，虚拟地址 va，以及一个标志 alloc，表示在缺页时是否分配新的页表
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  // 检查虚拟地址 va 是否超过最大有效虚拟地址 MAXVA。如果超出范围，调用 panic 函数终止程序
  if(va >= MAXVA)
    panic("walk");
  // 循环从最高层次的页表（通常是3级页表结构中的2级）遍历到最低层次的页表（0级除外，因为这是具体的页表项）
  for(int level = 2; level > 0; level--) {
    // PX(level, va) 是一个宏，用于从虚拟地址 va 和层次 level 计算出对应的页表索引
    // pte 指向当前层次页表的相应条目
    pte_t *pte = &pagetable[PX(level, va)];

    // 检查当前页表条目 *pte 是否有效
    if(*pte & PTE_V) {
      // 如果有效，则更新 pagetable 为该条目指向的下一级页表
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      // 如果无效，检查是否允许分配新页表（alloc 标志）
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      // 初始化新页表并设置当前页表条目为新页表的物理地址，并标记为有效（PTE_V）
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  // 遍历到最底层的页表时，返回该层次页表中对应虚拟地址的页表条目的地址
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
// 将虚拟地址 va 转换为物理地址 pa，前提是该虚拟地址有效且用户级访问（通过页表条目）
// 参数： 页表 pagetable 和虚拟地址 va
// 返回:  64 位的物理地址
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  // 检查虚拟地址 va 是否超过最大有效虚拟地址 MAXVA。如果超出范围，调用 panic 函数终止程序
  if(va >= MAXVA)
    return 0;
  // 查找虚拟地址 va 对应的页表条目 pte，不允许分配新的页表条目 alloc = 0
  pte = walk(pagetable, va, 0);

  // 无法找到对应的页表条目，返回 0 表示无效地址
  if(pte == 0)
    return 0;
  // 如果无效，返回 0 表示无效地址
  if((*pte & PTE_V) == 0)
    return 0;
  // 不允许用户级访问，返回 0 表示无效地址
  if((*pte & PTE_U) == 0)
    return 0;

  // 将页表条目 *pte 中存储的物理地址提取出来
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
// 用于将一段虚拟地址空间映射到物理地址空间，主要是在内核页表中建立映射
// va (虚拟地址): 要映射的虚拟地址的起始地址。
// pa (物理地址): 要映射到的物理地址的起始地址。
// sz (大小): 要映射的内存大小。
// perm (权限): 内存页面的权限标志，例如读/写/执行权限。
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
// 用于将虚拟地址转换为物理地址
uint64
kvmpa(uint64 va)
{
  // 计算虚拟地址 va 在页面内的偏移量
  // 此偏移量用于最后将物理页框地址加上该偏移量
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  // 使用 walk 函数在 kernel_pagetable 中查找与虚拟地址 va 对应的页表项
  // pte = walk(kernel_pagetable, va, 0);

  // lab3.2 使用进程自身的内核页表
//  pte = walk(myproc()->kenel_pagetable, va, 0);
  pte = walk(myproc()->kenel_pagetable, va, 0);
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  // 将页表项转换为物理地址
  pa = PTE2PA(*pte);
  // 物理页地址加上之前计算的页内偏移量，返回完整的物理地址
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
// 将一段虚拟地址空间映射到物理地址空间中。它在页表中设置虚拟地址到物理地址的映射，并为每个页设置相应的权限
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  // a：向下取整到页面边界的虚拟地址 va
  a = PGROUNDDOWN(va);
  // last：向下取整到页面边界的最后一个虚拟地址，即 va + size - 1
  last = PGROUNDDOWN(va + size - 1);

  // 遍历虚拟地址范围并进行映射
  for(;;){
    // 使用 walk 函数查找或创建页表项
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    // 设置页表项，将物理地址转换为页表项格式并设置权限和有效位
    *pte = PA2PTE(pa) | perm | PTE_V;
    // 检查是否达到最后一个页面，如果是则退出循环
    if(a == last)
      break;
    // 更新虚拟地址 a 和物理地址 pa ，以映射下一个页面
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
// 解除页表中一段虚拟地址范围的映射，并根据需要释放对应的物理内存
// pagetable (页表): 要操作的页表。
// va (虚拟地址): 起始虚拟地址。
// npages (页数): 要解除映射的页数。
// do_free (是否释放物理内存): 如果非零，则释放对应的物理内存
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  // 检查 va 是否对齐到页面边界
  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");
  // 遍历并解除映射
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 查找PTE失败
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    //
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    // 是否为叶子节点（即，映射的是实际物理页）
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
// 创建一个新的用户页表
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a ;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
// 递归地遍历并释放给定页表中的所有页表条目
// 检查每个页表条目，如果该条目指向一个较低级别的页表，则递归调用 freewalk 释放该页表；如果该条目是有效的叶节点，则抛出错误
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
// 将数据从内核空间复制到用户空间。通过用户页表将内核空间的源数据复制到用户空间的目标虚拟地址
// pagetable: 用户进程的页表。
// dstva: 目标虚拟地址。
// src: 源数据的内核地址。
// len: 要复制的数据长度。
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 计算目标虚拟地址的页对齐地址
    va0 = PGROUNDDOWN(dstva);
    // 获取页对齐地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 计算当前页可以复制的数据大小
    // 若 n 大于剩余要复制的数据长度 len，则 n 设置为 len
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    // 将 n 个字节的数据从 src 复制到目标物理地址对应的内存位置
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    // 更新目标虚拟地址 dstva，移动到下一个页的开始位置
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
/**
 * 将数据从用户空间复制到内核空间
 * @param pagetable 用户进程的页表
 * @param dst 目标内核地址
 * @param srcva 源虚拟地址
 * @param len 要复制的数据长度
 * @return
 */
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    // 计算目标虚拟地址的页对齐地址
    va0 = PGROUNDDOWN(srcva);
    // 获取页对齐地址对应的物理地址
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    // 计算当前页可以复制的数据大小
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    // 将 n 个字节的数据从源物理地址对应的内存位置复制到目标内核地址 dst
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
/**
 * 将一个以 NULL 结尾的字符串从用户空间复制到内核空间
 * @param pagetable 用户进程的页表
 * @param dst 目标内核地址
 * @param srcva 源虚拟地址
 * @param max 最大可复制的字节数
 * @return
 */
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

/**
 * 打印页表内容
 * @param pagetable 进程的
 */
void
vmprint(pagetable_t pagetable, int level) {
  if(level > 2) return ;
  if(level == 0) {
    printf("page table %p\n", pagetable);
  }

  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){    // 只打印有效的PTE
      for (int j = 0; j <= level; j++) {
        if (j) printf(" ");
        printf("..");
      }
      uint64 child = PTE2PA(pte);
      printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        vmprint((pagetable_t)child, level + 1);
      }
    }
  }
}

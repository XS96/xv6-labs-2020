// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// lab5. cow
struct ref_cnt {
    struct spinlock lock;
    int cnt[(PHYSTOP - KERNBASE)/ PGSIZE];  // 引用计数
} cow_ref_cnt;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cow_ref_cnt.lock, "cow_ref_cnt");   // lab5. cow
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
      cow_ref_cnt.cnt[(uint64)p / PGSIZE] = 1;
      kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  kdescrefcnt(pa);
  // lab5. 引用计数减一，若减到0才释放空间
  if(krefcnt(pa) > 0) {
      return;
  }

  acquire(&cow_ref_cnt.lock);
  cow_ref_cnt.cnt[((uint64)pa - KERNBASE) / PGSIZE] = 0;
  release(&cow_ref_cnt.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
      kmem.freelist = r->next;
      // lab5. cow 引用计数初始化为1
      acquire(&cow_ref_cnt.lock);
      cow_ref_cnt.cnt[((uint64)r - KERNBASE) / PGSIZE] = 1;
      release(&cow_ref_cnt.lock);
  }

  release(&kmem.lock);

  if(r) {
      memset((char*)r, 5, PGSIZE); // fill with junk
  }

  return (void*)r;
}


int krefcnt(void* pa) {
    return cow_ref_cnt.cnt[((uint64)pa - KERNBASE) / PGSIZE];
}

int kaddrefcnt(void* pa) {
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        return -1;
    acquire(&cow_ref_cnt.lock);
    ++cow_ref_cnt.cnt[((uint64)pa - KERNBASE) / PGSIZE];
    release(&cow_ref_cnt.lock);
    return 0;
}

int kdescrefcnt(void* pa) {
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        return -1;
    acquire(&cow_ref_cnt.lock);
    --cow_ref_cnt.cnt[((uint64)pa - KERNBASE) / PGSIZE];
    release(&cow_ref_cnt.lock);
    return 0;
}

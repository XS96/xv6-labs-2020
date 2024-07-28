#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

//extern void backtrace(void);

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

void
backtrace()
{
    printf("backtrace:\n");
    uint64 fp = r_fp();                 // 当前函数的帧指针

    uint64 down = PGROUNDDOWN(fp);      // 栈底
    uint64 up = PGROUNDUP(fp);          // 栈首

    uint64 *frame = (uint64 *) fp;

    while(fp < up && fp > down) {
        printf("%p\n", frame[-1]);
        fp = frame[-2];
        frame = (uint64 *) fp;
    }
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);

  // lab4.1
  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigalarm(void)
{
    int ticks;              // 报警间隔
    uint64 alarm_handler;   // 处理函数指针

    argint(0, &ticks);
    argaddr(1, &alarm_handler);

    struct proc *p = myproc();
    p->ticks = ticks;
    p->alarm_handler = alarm_handler;
    p->ticks_cnt = 0;
    return 0;
}

uint64
sys_sigreturn(void)
{
    struct proc *p = myproc();
    p->alarm_handing = 0;
    *p->trapframe = *p->ticks_trapframe;
    return 0;
}
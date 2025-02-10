#include "kernel.h"
#include "common.h"

extern char __kernel_base[], __stack_top[], __bss[], __bss_end[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

struct process procs[PROCS_MAX];
struct process *current_proc;
struct process *idle_proc;
struct process *proc_a;
struct process *proc_b;

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3,
                       long arg4, long arg5, long fid, long eid) {
  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;
  register long a7 __asm__("a7") = eid;
  __asm__ __volatile__("ecall"
                       : "=r"(a0), "=r"(a1)
                       : "r"(a0), "r"(a1), "r"(a2), "r"(a3),
                         "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                       : "memory");
  return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) { sbi_call(ch, 0, 0, 0, 0, 0, 0, 1); }

__attribute__((section(".text.boot"))) __attribute__((naked))
void boot(void) {
  __asm__ __volatile__(
    "mv sp, %[st]\n"
    "j kernel_main\n"
    :
    : [st] "r"(__stack_top));
}

void kernel_main(void) {
  memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
  WRITE_CSR(stvec, (uint32_t)kernel_entry);
  idle_proc = create_process(NULL, 0);
  idle_proc->pid = -1;
  current_proc = idle_proc;
  create_process(_binary_shell_bin_start, (size_t)_binary_shell_bin_size);
  yield();
  PANIC("switched to idle process");
}

__attribute__((aligned(4))) __attribute__((naked))
void kernel_entry(void) {
  __asm__ __volatile__(
      "csrrw sp, sscratch, sp\n"
      "addi sp, sp, -4 * 31\n"
      "sw ra, 0(sp)\n"     //   : [store registers]
      "sw gp, 4(sp)\n"
      "sw tp, 8(sp)\n"
      "sw t0, 12(sp)\n"
      "sw t1, 16(sp)\n"
      "sw t2, 20(sp)\n"
      "sw t3, 24(sp)\n"
      "sw t4, 28(sp)\n"
      "sw t5, 32(sp)\n"
      "sw t6, 36(sp)\n"
      "sw a0, 40(sp)\n"
      "sw a1, 44(sp)\n"
      "sw a2, 48(sp)\n"
      "sw a3, 52(sp)\n"
      "sw a4, 56(sp)\n"
      "sw a5, 60(sp)\n"
      "sw a6, 64(sp)\n"
      "sw a7, 68(sp)\n"
      "sw s0, 72(sp)\n"
      "sw s1, 76(sp)\n"
      "sw s2, 80(sp)\n"
      "sw s3, 84(sp)\n"
      "sw s4, 88(sp)\n"
      "sw s5, 92(sp)\n"
      "sw s6, 96(sp)\n"
      "sw s7, 100(sp)\n"
      "sw s8, 104(sp)\n"
      "sw s9, 108(sp)\n"
      "sw s10, 112(sp)\n"
      "sw s11, 116(sp)\n"
      "csrr a0, sscratch\n"
      "sw a0, 120(sp)\n"
      "addi a0, sp, 4*31\n"
      "csrw sscratch, a0\n"
      "mv a0, sp\n"
      "call handle_trap\n"
      "lw ra, 0(sp)\n"     //   : [restore registers]
      "lw gp, 4(sp)\n"
      "lw tp, 8(sp)\n"
      "lw t0, 12(sp)\n"
      "lw t1, 16(sp)\n"
      "lw t2, 20(sp)\n"
      "lw t3, 24(sp)\n"
      "lw t4, 28(sp)\n"
      "lw t5, 32(sp)\n"
      "lw t6, 36(sp)\n"
      "lw a0, 40(sp)\n"
      "lw a1, 44(sp)\n"
      "lw a2, 48(sp)\n"
      "lw a3, 52(sp)\n"
      "lw a4, 56(sp)\n"
      "lw a5, 60(sp)\n"
      "lw a6, 64(sp)\n"
      "lw a7, 68(sp)\n"
      "lw s0, 72(sp)\n"
      "lw s1, 76(sp)\n"
      "lw s2, 80(sp)\n"
      "lw s3, 84(sp)\n"
      "lw s4, 88(sp)\n"
      "lw s5, 92(sp)\n"
      "lw s6, 96(sp)\n"
      "lw s7, 100(sp)\n"
      "lw s8, 104(sp)\n"
      "lw s9, 108(sp)\n"
      "lw s10, 112(sp)\n"
      "lw s11, 116(sp)\n"
      "lw sp, 120(sp)\n"
      "sret\n");
}

void handle_trap(struct trap_frame *f) {
  uint32_t scause = READ_CSR(scause), stval = READ_CSR(stval);
  uint32_t sepc = READ_CSR(sepc);
  if (scause == SCAUSE_ECALL) {
    handle_syscall(f);
    sepc += 4;
  } else {
    PANIC("unexpected trap occured; scause=%x, stval=%x, sepc=%x\n",
          scause, stval, sepc);
  }
  WRITE_CSR(sepc, sepc);
}

void handle_syscall(struct trap_frame *f) {
  switch (f->a3) {
    case SYS_PUTCHAR:
      putchar(f->a0);
      break;
    default:
      PANIC("unexpected syscall a3=%x\n", f->a3);
  }
}

paddr_t alloc_pages(uint32_t n) {
  static paddr_t next_paddr = (paddr_t)__free_ram;
  paddr_t paddr = next_paddr;
  next_paddr += n * PAGE_SIZE;
  if (next_paddr > (paddr_t)__free_ram_end) PANIC("out of memory");
  memset((void *)paddr, 0, n * PAGE_SIZE);
  return paddr;
}

/* 
   map_page() handles multi-level page tables. It checks alignment, 
   allocates a second-level table if needed, then sets the appropriate 
   entry for the physical address. 
*/
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
  if (!is_aligned(vaddr, PAGE_SIZE)) PANIC("unaligned vaddr %x", vaddr);
  if (!is_aligned(paddr, PAGE_SIZE)) PANIC("unaligned paddr %x", paddr);
  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  if (!(table1[vpn1] & PAGE_V)) {
    uint32_t pt_paddr = alloc_pages(1);
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
  }
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}

__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
  __asm__ __volatile__(
      "addi sp, sp, -13*4\n"
      "sw ra, 0(sp)\n"
      "sw s0, 4(sp)\n"
      "sw s1, 8(sp)\n"
      "sw s2, 12(sp)\n"
      "sw s3, 16(sp)\n"
      "sw s4, 20(sp)\n"
      "sw s5, 24(sp)\n"
      "sw s6, 28(sp)\n"
      "sw s7, 32(sp)\n"
      "sw s8, 36(sp)\n"
      "sw s9, 40(sp)\n"
      "sw s10, 44(sp)\n"
      "sw s11, 48(sp)\n"
      "sw sp, (a0)\n"
      "lw sp, (a1)\n"
      "lw ra, 0(sp)\n"
      "lw s0, 4(sp)\n"
      "lw s1, 8(sp)\n"
      "lw s2, 12(sp)\n"
      "lw s3, 16(sp)\n"
      "lw s4, 20(sp)\n"
      "lw s5, 24(sp)\n"
      "lw s6, 28(sp)\n"
      "lw s7, 32(sp)\n"
      "lw s8, 36(sp)\n"
      "lw s9, 40(sp)\n"
      "lw s10, 44(sp)\n"
      "lw s11, 48(sp)\n"
      "addi sp, sp, 52\n"
      "ret\n");
}

/*
   yield() tries to find a new runnable process. If found, it changes 
   the page table, updates sscratch, and does a context switch.
*/
void yield(void) {
  struct process *next = idle_proc;
  for (int i = 0; i < PROCS_MAX; i++) {
    struct process *p = &procs[(current_proc->pid + i) % PROCS_MAX];
    if (p->state == PROC_RUNNABLE && p->pid > 0) { next = p; break; }
  }
  if (next == current_proc) return;
  __asm__ __volatile__("sfence.vma");
  WRITE_CSR(satp, SATP_SV32 | ((uint32_t)next->page_table / PAGE_SIZE));
  __asm__ __volatile__("sfence.vma");
  WRITE_CSR(sscratch, (uint32_t)&next->stack[sizeof(next->stack)]);
  struct process *prev = current_proc;
  current_proc = next;
  switch_context(&prev->sp, &next->sp);
}

void user_entry(void) {
  WRITE_CSR(sepc, USER_BASE);
  __asm__ __volatile__("sret\n");
}

/* 
   create_process() searches for an unused slot, prepares the stack, 
   sets up page tables (kernel + user image), and initializes 
   the process structure so it can be scheduled.
*/
struct process *create_process(const void *image, size_t image_size) {
  struct process *proc = NULL;
  for (int i = 0; i < PROCS_MAX; i++) {
    if (procs[i].state == PROC_UNUSED) { proc = &procs[i]; break; }
  }
  if (!proc) PANIC("no free process slots");
  uint32_t *sp = (uint32_t *)&proc->stack[sizeof(proc->stack)];
  *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
  *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
  *--sp = 0; *--sp = 0; *--sp = 0; *--sp = 0;
  *--sp = (uint32_t)user_entry;
  uint32_t *page_table = (uint32_t *)alloc_pages(1);

  for (paddr_t pa = (paddr_t)__kernel_base; pa < (paddr_t)__free_ram_end;
       pa += PAGE_SIZE) {
    map_page(page_table, pa, pa, PAGE_R | PAGE_W | PAGE_X);
  }
  for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
    paddr_t pg = alloc_pages(1);
    memcpy((void *)pg, image + off, PAGE_SIZE);
    map_page(page_table, USER_BASE + off, pg, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
  }
  proc->pid = (int)(proc - procs) + 1;
  proc->state = PROC_RUNNABLE;
  proc->sp = (uint32_t)sp;
  proc->page_table = page_table;
  return proc;
}

/* Example user process function that prints A in a loop (unused here) */
void proc_a_entry(void) {
  printf("starting process A\n");
  while (1) {
    putchar('A');
    yield();
    for (int i = 0; i < 30000000; i++) __asm__ __volatile__("nop");
  }
}

/* Example user process function that prints B in a loop (unused here) */
void proc_b_entry(void) {
  printf("starting process B\n");
  while (1) {
    putchar('B');
    yield();
    for (int i = 0; i < 30000000; i++) __asm__ __volatile__("nop");
  }
}

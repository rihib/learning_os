#include "kernel.h"

#include "common.h"

extern char __kernel_base[];
extern char __stack_top[];
extern char __bss[], __bss_end[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

size_t next_alloc = (size_t)__free_ram;
size_t alloc_page(size_t page_num);

void trap_main(void)
{
  uint32_t scause = READ_CSR(scause);
  uint32_t stval = READ_CSR(stval);
  uint32_t sepc = READ_CSR(sepc);
  uint32_t sstatus = READ_CSR(sstatus);
  printf("trap handled; scause: %d, stval: %x, sepc: %x, sstatus: %x\n",
         scause, stval, sepc, sstatus);
}

__attribute__((aligned(4))) __attribute__((naked)) void
trap_handler(void)
{
  __asm__ __volatile__(
      "csrw sscratch, sp\n"
      "sw ra, 0(sp)\n"
      "sw gp, -4(sp)\n"
      "sw tp, -8(sp)\n"
      "sw t0, -12(sp)\n"
      "sw t1, -16(sp)\n"
      "sw t2, -20(sp)\n"
      "sw t3, -24(sp)\n"
      "sw t4, -28(sp)\n"
      "sw t5, -32(sp)\n"
      "sw t6, -36(sp)\n"
      "sw a0, -40(sp)\n"
      "sw a1, -44(sp)\n"
      "sw a2, -48(sp)\n"
      "sw a3, -52(sp)\n"
      "sw a4, -56(sp)\n"
      "sw a5, -60(sp)\n"
      "sw a6, -64(sp)\n"
      "sw a7, -68(sp)\n"
      "sw s0, -72(sp)\n"
      "sw s1, -76(sp)\n"
      "sw s2, -80(sp)\n"
      "sw s3, -84(sp)\n"
      "sw s4, -88(sp)\n"
      "sw s5, -92(sp)\n"
      "sw s6, -96(sp)\n"
      "sw s7, -100(sp)\n"
      "sw s8, -104(sp)\n"
      "sw s9, -108(sp)\n"
      "sw s10, -112(sp)\n"
      "sw s11, -116(sp)\n"
      "csrr t0, sscratch\n"
      "sw t0, -120(sp)\n"
      "addi sp, sp, -124\n"
      "call trap_main\n"
      "lw sp, -120(sp)\n"
      "lw s11, -116(sp)\n"
      "lw s10, -112(sp)\n"
      "lw s9, -108(sp)\n"
      "lw s8, -104(sp)\n"
      "lw s7, -100(sp)\n"
      "lw s6, -96(sp)\n"
      "lw s5, -92(sp)\n"
      "lw s4, -88(sp)\n"
      "lw s3, -84(sp)\n"
      "lw s2, -80(sp)\n"
      "lw s1, -76(sp)\n"
      "lw s0, -72(sp)\n"
      "lw a7, -68(sp)\n"
      "lw a6, -64(sp)\n"
      "lw a5, -60(sp)\n"
      "lw a4, -56(sp)\n"
      "lw a3, -52(sp)\n"
      "lw a2, -48(sp)\n"
      "lw a1, -44(sp)\n"
      "lw a0, -40(sp)\n"
      "lw t6, -36(sp)\n"
      "lw t5, -32(sp)\n"
      "lw t4, -28(sp)\n"
      "lw t3, -24(sp)\n"
      "lw t2, -20(sp)\n"
      "lw t1, -16(sp)\n"
      "lw t0, -12(sp)\n"
      "lw tp, -8(sp)\n"
      "lw gp, -4(sp)\n"
      "lw ra, 0(sp)\n"
      "sret");
}

void kernel_main(void)
{
  memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

  WRITE_CSR(stvec, trap_handler);

  size_t allocated_ptr = alloc_page(1);
  printf("allocated_ptr: %x\n", allocated_ptr);

  PANIC("I'm here!\n");
}

__attribute__((section(".text.boot"))) __attribute__((naked)) void boot(void)
{
  __asm__ __volatile__(
      "mv sp, %[stack_top]\n"
      "j kernel_main\n"
      :
      : [stack_top] "r"(__stack_top));
}

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid)
{
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
                       : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                         "r"(a6), "r"(a7)
                       : "memory");
  return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch)
{
  sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

size_t alloc_page(size_t page_num)
{
  size_t ret = next_alloc;
  next_alloc += page_num * PAGE_SIZE;
  if (next_alloc > (size_t)__free_ram_end)
  {
    PANIC("Out of memory!\n");
  }

  memset((void *)ret, 0, page_num * PAGE_SIZE);
  return ret;
}

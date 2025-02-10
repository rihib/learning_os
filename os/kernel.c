#include "kernel.h"

#include "common.h"

extern char __kernel_base[];
extern char __stack_top[];
extern char __bss[], __bss_end[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];
paddr_t pagealoc_start = (paddr_t)__free_ram;


__attribute__((aligned(4))) void traphandle(void){
  // save registers
  __asm__ __volatile__ (
    "sw sp, -4(sp)\n"
    "sw ra, -8(sp)\n"
    "sw gp, -12(sp)\n"
    "sw tp, -16(sp)\n"
    "sw t0, -20(sp)\n"
    "sw t1, -24(sp)\n"
    "sw t2, -28(sp)\n"
    "sw t3, -32(sp)\n"
    "sw t4, -36(sp)\n"
    "sw t5, -40(sp)\n"
    "sw t6, -44(sp)\n"
    "sw a0, -48(sp)\n"
    "sw a1, -52(sp)\n"
    "sw a2, -56(sp)\n"
    "sw a3, -60(sp)\n"
    "sw a4, -64(sp)\n"
    "sw a5, -68(sp)\n"
    "sw a6, -72(sp)\n"
    "sw a7, -76(sp)\n"
    "sw s0, -80(sp)\n"
    "sw s1, -84(sp)\n"
    "sw s2, -88(sp)\n"
    "sw s3, -92(sp)\n"
    "sw s4, -96(sp)\n"
    "sw s5, -100(sp)\n"
    "sw s6, -104(sp)\n"
    "sw s7, -108(sp)\n"
    "sw s8, -112(sp)\n"
    "sw s9, -116(sp)\n"
    "sw s10, -120(sp)\n"
    "sw s11, -124(sp)\n"
    "addi sp, sp, -4*31\n"
    "csrw sscratch, sp\n"
    : 
    : 
    : "memory"
  );
  printf("scause: %x\n", READ_CSR(SCAUSE));
  printf("sepc: %x\n", READ_CSR(SEPC));
  printf("stval: %x\n", READ_CSR(STVAL));
  PANIC("trap!");
  // restore registers
  __asm__ __volatile__ (
    "csrr sp, sscratch\n"
    "lw s11, 4(sp)\n"
    "lw s10, 4(sp)\n"
    "lw s9, 8(sp)\n"
    "lw s8, 12(sp)\n"
    "lw s7, 16(sp)\n"
    "lw s6, 20(sp)\n"
    "lw s5, 24(sp)\n"
    "lw s4, 28(sp)\n"
    "lw s3, 32(sp)\n"
    "lw s2, 36(sp)\n"
    "lw s1, 40(sp)\n"
    "lw s0, 44(sp)\n"
    "lw a7, 48(sp)\n"
    "lw a6, 52(sp)\n"
    "lw a5, 56(sp)\n"
    "lw a4, 60(sp)\n"
    "lw a3, 64(sp)\n"
    "lw a2, 68(sp)\n"
    "lw a1, 72(sp)\n"
    "lw a0, 76(sp)\n"
    "lw t6, 80(sp)\n"
    "lw t5, 84(sp)\n"
    "lw t4, 88(sp)\n"
    "lw t3, 92(sp)\n"
    "lw t2, 96(sp)\n"
    "lw t1, 100(sp)\n"
    "lw t0, 104(sp)\n"
    "lw tp, 108(sp)\n"
    "lw gp, 112(sp)\n"
    "lw ra, 116(sp)\n"
    "lw sp, 120(sp)\n"
    : 
  );
  // return from trap
  __asm__ __volatile__ (
    "sret"
  );
}

void trapinit(void){
  WRITE_CSR(STVEC, traphandle);
}

paddr_t pagealloc(size_t pageCount){
  size_t size = pageCount * PAGE_SIZE;
  paddr_t start= pagealoc_start;
  if(start + size > __free_ram_end)
    PANIC("run out of memory!");
  pagealoc_start += size;
  memset(start, 0, size);
  return start;
}

void ptemake(uint32_t* pagetable, vaddr_t va, paddr_t pa, uint32_t flags){
  if(!(is_aligned(va, PAGE_SIZE) || is_aligned(pa, PAGE_SIZE))){
    PANIC("ptmake: va or pa is not aligned!");
  }
  uint32_t pte = pa | PAGE_V | flags;
  uint32_t t1_pageindex = va >> 22;
  uint32_t t0_pageindex = (va >> 12) & 1024;
  uint32_t t1_pte = pagetable[t1_pageindex];
  if(t1_pte & PAGE_V != PAGE_V){
    paddr_t t0 = pagealloc(1);
    pagetable[t1_pageindex] = t0 & 1024 | PAGE_V;
    t1_pte = pagetable[t1_pageindex];
  }
  uint32_t* t0 = t1_pte & 4096;
  t0[t0_pageindex] = pte;
}

void kernel_main(void) {
  memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);
  trapinit();
  printf("trap initialized\n");
  //__asm__ __volatile__("unimp");
  paddr_t test_page = pagealloc(2);
  printf("page_addr: %x\n", test_page);
  test_page = pagealloc(2);
  printf("page_addr: %x\n", test_page);
  PANIC("booted!");
  printf("unreachable here!\n");
}

__attribute__((section(".text.boot"))) __attribute__((naked)) void boot(void) {
  __asm__ __volatile__(
      "mv sp, %[stack_top]\n"
      "j kernel_main\n"
      :
      : [stack_top] "r"(__stack_top));
}

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
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

void putchar(char ch) {
  sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

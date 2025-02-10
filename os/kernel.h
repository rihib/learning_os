#pragma once
#include "common.h"

#define PROCS_MAX 8
#define PROC_UNUSED 0
#define PROC_RUNNABLE 1
#define SATP_SV32 (1u << 31)
#define SCAUSE_ECALL 8
#define PAGE_V (1 << 0)
#define PAGE_R (1 << 1)
#define PAGE_W (1 << 2)
#define PAGE_X (1 << 3)
#define PAGE_U (1 << 4)
#define USER_BASE 0x1000000

#define PANIC(fmt, ...) \
  do { \
    printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    while (1) {} \
  } while (0)

#define READ_CSR(reg) \
  ({ unsigned long t; __asm__ __volatile__("csrr %0, " #reg : "=r"(t)); t; })

#define WRITE_CSR(reg, value) \
  do { uint32_t tmp = (value); __asm__ __volatile__("csrw " #reg ", %0" :: "r"(tmp)); } while (0)

struct sbiret {
  long error;
  long value;
};

struct trap_frame {
  uint32_t ra, gp, tp, t0, t1, t2, t3, t4, t5, t6;
  uint32_t a0, a1, a2, a3, a4, a5, a6, a7;
  uint32_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  uint32_t sp;
} __attribute__((packed));

struct process {
  int pid;
  int state;
  vaddr_t sp;
  uint32_t *page_table;
  uint8_t stack[8192];
};

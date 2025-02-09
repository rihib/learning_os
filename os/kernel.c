#include "kernel.h"

#include "common.h"

extern char __kernel_base[];
extern char __stack_top[];
extern char __bss[], __bss_end[];
// シンボルの外部宣言
// リンカスクリプト内で定義されたシンボルはexternで使えるようになる。(複数同時に宣言することも可能)
extern char __free_ram[], __free_ram_end[];
paddr_t __ram_top;
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];
void traphandler(void);
paddr_t mallocate_pages(int n);
paddr_t pagealloc(size_t pageCount);

void kernel_main(void) {
  // memsetはユーザーモードで実行されるので、OSなどが格納されているデータを汚さないようになっている。 → ram(ランダムにアクセスできるメモリ)
  // bss領域を0でとりあえず、全部０クリアする
  memset(__bss, 0, (size_t)__bss_end - (size_t)__bss);

  // trapが起こったとき、stvec（特権レジスタ）に保存された関数pointer先にとぶ（ハンドラー）
  WRITE_CSR(stvec,traphandler);
  __ram_top = (paddr_t)__free_ram;
  printf("before: %x\n",__free_ram);
  paddr_t after = mallocate_pages(1);
  paddr_t result = __ram_top - after;
  printf("result: %d\n", result);

  printf("before: %x\n",__ram_top);
  paddr_t after1 = mallocate_pages(1);
  paddr_t result1 = __ram_top - after1;
  printf("result: %d\n", result1);
  __asm__ __volatile__(
    "unimp;\n"
  );
  printf("continue\n");
}

// PointerAddress_t
// pagetableはポインタのリスト
// プロセスに領域を割り当てる処理
void page_table(paddr_t table1[], paddr_t p, paddr_t v, paddr_t flags){
  if (!(is_aligned(p,PAGE_SIZE))){
    PANIC();
  }
  if (!(is_aligned(v,PAGE_SIZE))){
    PANIC();
  }
  // 仮想アドレスの上位10ビットを取り出して、ページ番号を把握する
  paddr_t vpn1 = v >> 22;
  //　一段目のページリストがあるか
  paddr_t *table0 = table1[vpn1];
  // 右に12ビットシフトして、
  // フラグを取り除いて、table0（物理ページ番号と）とPAGESIZEをかけてあげれば、物理アドレス（pointer）が手に入る。

  // 上位20ビットを取り出して、ページ番号を把握する
  paddr_t vpn0 = v / PAGE_SIZE;
  // 中10ビットを抽出
  vpn0 = vpn0 >> 12;
  vpn0 = vpn0 & 1024;

  // ゼロ埋めされたモノが来たら(もしくは無効なものが来たら)
  if (*table0 & PAGE_V == 0) {
    // フラグを立てる
    *table0 = mallocate_pages(1);
  }
  table0[vpn0] = p | flags;
  table1[vpn1] = table0 | flags;
}

paddr_t mallocate_pages(int page){
  paddr_t free_ram = (paddr_t)__ram_top;
  paddr_t end = (paddr_t)__free_ram_end;
  paddr_t top = free_ram + page * PAGE_SIZE;
  if (end < top){
    PANIC("failed to allocate pages");
  }

  __ram_top = top;
  paddr_t r = memset(free_ram, 0, page * PAGE_SIZE);
  return r;
}

//　__attribute__((aligned(4)))はメモリへの効率的な配置を指令
__attribute__((aligned(4))) void traphandler(void){
  // まずはmedlegレジスタをCPUが確認して、Sモードに切り変え、CSRと呼ばれるレジスタに状態を保存。
  // なので、そこから値を読み取る必要がある。
  // spレジスタにはstuckメモリの先頭アドレスが格納されている。（下から上に）
  // sw a0というレジスタからオフセット~でstuckメモリに保存する。（変数名とかはない）
  // 32bitアーキテクチャ（レジスタ）なので4byte分のスタック領域を確保しなきゃいけない。
  __asm__ __volatile__(
    "sw sp, 0(sp);\n"
    "sw a0, -4(sp);\n"
    "sw a1, -8(sp);\n"
    "sw a2, -12(sp);\n"
    "sw a3, -16(sp);\n"
    "sw a4, -20(sp);\n"
    "sw a5, -24(sp);\n"
    "sw a6, -28(sp);\n"
    "sw a7, -32(sp);\n"
    "sw s0, -36(sp);\n"
    "sw s1, -40(sp);\n"
    "sw s2, -44(sp);\n"
    "sw s3, -48(sp);\n"
    "sw s4, -52(sp);\n"
    "sw s5, -56(sp);\n"
    "sw s6, -60(sp);\n"
    "sw s7, -64(sp);\n"
    "sw s8, -68(sp);\n"
    "sw s9, -72(sp);\n"
    "sw s10, -76(sp);\n"
    "sw s11, -80(sp);\n"
    "sw t0, -84(sp);\n"
    "sw t1, -88(sp);\n"
    "sw t2, -92(sp);\n"
    "sw t3, -96(sp);\n"
    "sw t4, -100(sp);\n"
    "sw t5, -104(sp);\n"
    "sw t6, -108(sp);\n"
    "sw ra, -112(sp);\n"
    "sw gp, -116(sp);\n"
    "sw tp, -120(sp);\n"
    "addi sp, sp, -124;\n"
    : 
    : 
    : "memory"
  );
  uint32_t error = READ_CSR(scause); 
  uint32_t where = READ_CSR(sepc);
  // ---トラップ処理---
  PANIC("error occured:%x At : %x\n",error,where);
  // ---トラップ処理---
  __asm__ __volatile__(
    "lw tp, 4(sp);\n"
    "lw gp, 8(sp);\n"
    "lw ra, 12(sp);\n"
    "lw t6, 16(sp);\n"
    "lw t5, 20(sp);\n"
    "lw t4, 24(sp);\n"
    "lw t3, 28(sp);\n"
    "lw t2, 32(sp);\n"
    "lw t1, 36(sp);\n"
    "lw t0, 40(sp);\n"
    "lw s11, 44(sp);\n"
    "lw s10, 48(sp);\n"
    "lw s9, 52(sp);\n"
    "lw s8, 56(sp);\n"
    "lw s7, 60(sp);\n"
    "lw s6, 64(sp);\n"
    "lw s5, 68(sp);\n"
    "lw s4, 72(sp);\n"
    "lw s3, 76(sp);\n"
    "lw s2, 80(sp);\n"
    "lw s1, 84(sp);\n"
    "lw s0, 88(sp);\n"
    "lw a7, 92(sp);\n"
    "lw a6, 96(sp);\n"
    "lw a5, 100(sp)\n"
    "lw a4, 104(sp)\n"
    "lw a3, 108(sp)\n"
    "lw a2, 112(sp)\n"
    "lw a1, 116(sp)\n"
    "lw a0, 120(sp)\n"
    "lw sp, 124(sp)"
    "sret;\n"
  );
  // sret命令が実行されると、sstatusレジスタのSPPビットに設定された動作モードに復帰して、
  // specに格納されたアドレスをプログラムカウンタにセットしなおす
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

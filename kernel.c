#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char __bss[], __bss_end[], __stack_top[];

extern char __free_ram[], __free_ram_end[];

struct process procs[PROCS_MAX];
struct process *current_proc;
struct process* idle_proc;

__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp){
    __asm__ __volatile__(
        // 実行中プロセスのスタックへレジスタを保存
        "addi sp, sp, -13 * 4\n"
        "sw ra,  0  * 4(sp)\n"
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // スタックポインタの切り替え. a0が引数.
        "sw sp, (a0)\n" // spの値を、a0が指すメモリ位置に保存する. つまりproc->spに今のspを保存する.
        "lw sp, (a1)\n" // a1が指すメモリをspにロードする.
        // 次のプロセスのスタックからレジスタを復元
        "lw ra,  0  * 4(sp)\n"
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"
        "ret\n"
    );
}

void yield(void){
    struct process *next = idle_proc;
    for(int i = 0; i < PROCS_MAX; i++){
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        // pidはidle processじゃないかどうかの確認
        if(proc->state == PROC_RUNNABLE && proc->pid > 0){
            next = proc;
            break;
        }
    }

    if(next == current_proc){
        return;
    }

    // カーネルが好きに使っていいsscratchレジスタに
    // 次のプロセスのstackの最初のポインタを入れておく.
    __asm__ __volatile__(
        "csrw sscratch, %[sscratch]\n"
        :
        : [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

struct process* create_process(uint32_t pc){
    struct process *proc = NULL;
    int i;
    for (i = 0; i < PROCS_MAX; i++){
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slots");
    
    // next->stack 配列の 終端直後のメモリアドレスを刺す
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) pc;          // ra

    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    return proc;
}

void delay(void) {
    for (int i = 0; i < 30000000; i++)
        __asm__ __volatile__("nop"); // 何もしない命令
}

struct process *proc_a;
struct process *proc_b;

void putchar(char ch);

void proc_a_entry(void){
    printf("staring process A\n");

    while(1){
        putchar('A');
        yield();
        delay();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        putchar('B');
        yield();
        delay();
    }
}

paddr_t alloc_pages(uint32_t n){
    static uint32_t next_paddr = (uint32_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;
    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");
    
    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

// SBIの仕様に則ってOpenSBIを呼び出すためのもの.
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    
    // 以下の7つの命令は指定したレジスタに値を入れるようコンパイラに指示するもの
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    // インラインアセンブラで, ecall命令を呼び出す
    // CPUの実行モードをカーネル用(S-Mode)からOpenSBI用(M-mode)に代わり、
    // OpenSBIの処理ハンドラが呼び出される.
    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch){
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1 /* Console Putchar */);
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void){
    __asm__ __volatile__(
        // 実行中プロセスのカーネルスタックをsscratchから取り出す
        "csrrw sp, sscratch, sp\n" // sp レジスタの値を sscratch に保存. sscratch レジスタに格納されている元の値を sp レジスタにロード.
        "addi sp, sp, -4 * 31\n" // スタックに31個分のレジスタを保存するスペースを確保
        
        // 各レジスタの値をスタックに保存
        "sw ra,  4 * 0(sp)\n" // 戻りアドレス（ra）をスタックに保存します
        "sw gp,  4 * 1(sp)\n" // グローバルポインタ（gp）をスタックに保存
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"
        // これは例外発生時のスタックポインタをスタックに保管してるだけ
        "csrr a0, sscratch\n" // sscratch CSRから値を読み取り、それを汎用レジスタ a0 に格納
        "sw a0, 4 * 30(sp)\n" // a0レジスタの値をスタックに保存しています。具体的には、スタックポインタ（sp）からオフセット 4 * 30 の場所に a0 の内容を保存
        
        // カーネルスタックを設定し直す. sscratchに例外発生時のスタックポインタを入れちゃったので、もう一回設定し直してる.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n" // a0の値をsscratchレジスタに保存

        "mv a0, sp\n" // spの値を、a0 レジスタにコピー. handle_trapの引数.
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

// カーネルのメインエントリポイント
void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    idle_proc = create_process((uint32_t) NULL);
    idle_proc->pid = -1; // idle
    current_proc = idle_proc;

    proc_a = create_process((uint32_t) proc_a_entry);
    proc_b = create_process((uint32_t) proc_b_entry);

    yield();

    PANIC("unreachable here!");
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause); // 例外の発生事由 (scause) を取得
    uint32_t stval = READ_CSR(stval); 
    uint32_t user_pc = READ_CSR(sepc); // 例外発生時のプログラムカウンタ (sepc) を取得

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

// この関数を.text.boot セクションに配置する
// ブート時に実行するコード
__attribute__((section(".text.boot")))
// コンパイラに対して関数の前後にプロローグやエピローグを挿入しないように指示する
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // スタックポインタ（sp）を__stack_topに設定
        "j kernel_main\n" // kernel_main関数にジャンプ
        :
        : [stack_top] "r" (__stack_top) // この部分は、__stack_topというCの変数を入力として渡す. 
    );
}

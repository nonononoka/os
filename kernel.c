#include "kernel.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char __bss[], __bss_end[], __stack_top[];

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

// カーネルのメインエントリポイント
void kernel_main(void) {
    const char*s = "\n\nHello World!\n";
    for (int i = 0; s[i] != '\0'; i++){
        putchar(s[i]);
    }

    for(;;){
        __asm__ __volatile__("wfi");
    }
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
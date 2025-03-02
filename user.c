#include "user.h"

extern char __stack_top[];

// a3にシステムコール番号. a0~a2レジスタにシステムコールの引数.
// ecall 命令を実行すると、例外ハンドラが呼び出され、カーネルに処理が移る.
int syscall(int sysno, int arg0, int arg1, int arg2)
{
    register int a0 __asm__("a0") = arg0;
    register int a1 __asm__("a1") = arg1;
    register int a2 __asm__("a2") = arg2;
    register int a3 __asm__("a3") = sysno;

    __asm__ __volatile__("ecall"
                         : "=r"(a0)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                         : "memory");

    return a0;
}

void putchar(char ch)
{
    syscall(SYS_PUTCHAR, ch, 0, 0);
}

int getchar(void)
{
    return syscall(SYS_GETCHAR, 0, 0, 0);
}

__attribute__((noreturn)) void exit(void)
{
    syscall(SYS_EXIT, 0, 0, 0);
    for (;;)
        ; // 念のため
}

__attribute__((section(".text.start")))
__attribute__((naked)) void
start(void)
{
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "call main\n"
        "call exit\n" ::[stack_top] "r"(__stack_top));
}
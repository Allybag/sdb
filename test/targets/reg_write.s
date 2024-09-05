# This is AT&T syntax assembly, default on GCC
# % prefix means a register
# $ prefix means a constant

# SYSV ABI General Purpose Registers
# A Caller saved register can be overwritten inside a function
# A Callee saved register must be saved and restored at the end of a function
# rax: Caller saved general register, used for return values
# rbx: Callee saved general register,
# rcx: Used to pass the fourth integer argument to functions
# rdx: Used to pass the third argument to functions
# rsp: Stack pointer
# rbp: Callee saved general register, point to top of stack frame
# rsi: Used to pass the second argument to functions
# rdi: Used to pass the first argument to functions
# r8 : Used to pass the fifth argument to functions
# r9 : Used to pass the sixth argument to functions
# r10: Caller saved general register
# r11: Caller saved general register
# r12: Callee saved general register
# r13: Callee saved general register
# r14: Callee saved general register
# r15: Callee saved general register

# Opcodes
# Instructions which use the stack will change the value of the rsp register
# movq: Move quad word, copy 64 bits from source to destination
# push: Copy the value from a register to the top of the stack
# popq: Copy the value from the top of the stack to a register
# leaq: Load effective address quadword: copy address of source to destination

# syscall: Call the syscall by id, read from rax, overwrite rax with result
# printf : print, rax contains number of vector registers containing arguments
# fflush : First argument is probably stream index somehow, 0 means all streams

# Directives
# .asciz: Encode the given string in ASCII with null terminator

# Unknowns
# %rip: use RIP-relative addressing, required when assembling with -pie ??
# @plt: use the Procedure Linkage Table, used to call functions in shared libraries

# Constants
SYSCALL_GET_PID = 39
SYSCALL_KILL    = 62

SIGTRAP         = 5

# Declare main as a symbol
.global main

# Global data
.section .data
hex_format: .asciz "%#x"

# Code:
.section .text

.macro trap
    movq $SYSCALL_KILL, %rax
    movq %r12, %rdi
    movq $SIGTRAP, %rsi
    syscall
.endm

main:
    # Function prologue
    push %rbp
    movq %rsp, %rbp

    # Function body
    # Get pid and store in r12
    movq $SYSCALL_GET_PID, %rax
    syscall
    movq %rax, %r12

    trap

    # Print contents of rsi
    leaq hex_format(%rip), %rdi
    movq $0, %rax
    call printf@plt
    movq $0, %rdi
    call fflush@plt

    trap

    # Function epilogue
    popq %rbp
    movq $0, %rax
    ret

SYSCALL_GET_PID = 39

.global main

.section .data
my_double: .double 64.125

.section .text

.macro trap
    movq $62, %rax
    movq %r12, %rdi
    movq $5, %rsi
    syscall
.endm

main:
    push %rbp
    movq %rsp, %rbp

    # Get pid and store in r12, which we need for trap
    movq $SYSCALL_GET_PID, %rax
    syscall
    movq %rax, %r12

    # Store to r13 general purpose register
    movq $0xcafecafe, %r13
    trap

    # Store to r13b subregister
    movb $42, %r13b
    trap

    # Store to mm0 MMX register
    movq $0xba5eba11, %r13
    movq %r13, %mm0

    # Copy floating point data to XMM register
    movsd my_double(%rip), %xmm0
    trap

    # Store to st0
    # emms: Empty MMX state
    # fldl: Load floating point value - pushes to FPU stack
    emms
    fldl my_double(%rip)
    trap

    popq %rbp
    movq $0, %rax
    ret

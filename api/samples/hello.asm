// as -o hello.o hello.s
// ld -o hello hello.o

// File: hello.s
.global _start

.section .data
msg:    .asciz "Hello, ARM64!\n"

.section .text
_start:
    // Print the string
    mov x8, 64
    mov x0, 1
    ldr x1, =msg
    ldr x2, =14
    mov x16, 0
    svc 0
    // Exit the program
    mov x8, 93
    mov x0, 0
    mov x16, 0
    svc 0

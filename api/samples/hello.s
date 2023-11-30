// as -o hello.o hello.s
// ld -o hello hello.o

// File: hello.s
.global _start

.section .data
msg:    .asciz "Hello, ARM64!\n"

.section .text
_start:
    // Print the string
    mov x8, 64               // System call number for write
    mov x0, 1                // File descriptor 1 (stdout)
    ldr x1, =msg              // Pointer to the string
    ldr x2, =14               // Length of the string
    mov x16, 0                // ARM64 Linux syscall instruction
    svc 0

    // Exit the program
    mov x8, 93               // System call number for exit
    mov x0, 0                // Exit code 0
    mov x16, 0               // ARM64 Linux syscall instruction
    svc 0

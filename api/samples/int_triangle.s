.global _start
.arch armv8-a
.section .data
buffer:     .space 1024

.section .text

// x0: buffer address, x1 number of size
print_buffer:
    sub sp, sp, #0x20 //32 bytes
    str x0, [sp, #24] //x0 save to 24:32
    str x1, [sp, #16] //x1 save to 16:24
    ldr x8, [sp, #16] //x8 = x1
    ldr x9, [sp, #24] //x9 = x0
    str x8, [sp, #8]  //x8/x1 save to [8:16]
    ldr x10, [sp, #8] //x10 = x8 = x1
    mov x8, #0x40     // x8 = 64
    mov x0, #0x1      // x0 = 1
    mov x2, #0x100     // x2 = 16
    mov x1, x9        // x1 = x0
    mov x16, #0x0     // x16 = 0
    svc #0x0          // call printf
    add sp, sp, #0x20 // restore stack
    ret


// print_buffer:
    // Load the address of the message into register x1
    // mov x1, x0

    // Length of the message
    // LDR x2, =14

    // System call number for write
    // MOV x8, 64

    // File descriptor: stdout
    // MOV x0, 1

    // Make the system call
    // MOV x16, 0       // ARM64 Linux syscall instruction
    // SVC 0            // Make a system call


_start:
    adr x0, buffer // x0 = &buffer
    mov w1, wzr     // w1 = 0
    add w1, w1, #1  //w1 += 1
    mov w2, wzr     // w2 = 0

line:
    MOV x3, #0x2a2a
    str x3, [x0, #0]   // *X0 = w3
    add x0, x0, #8      // x0 += 4
    add w2, w2, #1      // w2 += 1
    cmp w1, w2          // if w1==w2
    b.ne line           // jump to line if not equal

lineDone:
    mov x3, #0x2b2b
    str x3, [x0, #0]   // *X0 = w3
    add x0, x0, #8      // x0 += 1
    mov w2, wzr         // w2 = 0
    add w1, w1, #1      // w1 += 1
    cmp w1, #9
    b.ne line

print:
    adr     x0, buffer
    mov     x1, #0x80
    bl print_buffer
    MOV x8, 93       // System call number for exit
    MOV x0, 0        // Exit code 0
    MOV x16, 0       // ARM64 Linux syscall instruction
    SVC 0            // Make a system call

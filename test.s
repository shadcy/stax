.global _start
_start:
    ldr r0, =0x101f1000
    mov r1, #'A'
    str r1, [r0]
    b _start

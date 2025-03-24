.data
n:      .word 10

.text

_start:
    lui x10, 0x10000
    lw x10, 0(x10)
    slti x11, x10, 1
    bne x11, x0, end

    addi x5, x0, 0
    addi x6, x0, 1
    lui x7, 0x10004

    sw x5, 0(x7)
    addi x7, x7, 4

    addi x10, x10, -1
    slti x11, x10, 1
    bne x11, x0, end

    sw x6, 0(x7)
    addi x7, x7, 4

    addi x10, x10, -1

loop:
    slti x11, x10, 1
    bne x11, x0, end

    add x8, x5, x6
    sw x8, 0(x7)

    addi x5, x6, 0
    addi x6, x8, 0
    addi x7, x7, 4

    addi x10, x10, -1
    jal x0, loop

end:
    addi x17, x0, 93
    addi x10, x0, 0
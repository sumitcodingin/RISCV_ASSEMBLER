 .text
    addi x5, x0, 10
    addi x6, x0, 0
    addi x7, x0, 1
    addi x8, x0, 2
fib_loop:
    beq x5, x0, fib_end
    beq x5, x8, fib_end
    add x9, x6, x7
    add x6, x7, x0
    add x7, x9, x0
    addi x5, x5, -1
    jal x0, fib_loop
fib_end:
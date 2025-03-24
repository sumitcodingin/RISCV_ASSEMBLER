.data
a1: .word 1,  3, 5, 7, 9, 11, 13, 15, 17, 19
a2: .word 2, 4, 6, 8, 10, 12, 14, 16, 18, 20

.text
addi x11, x0, 10  # no. of elements in first array
addi x12, x0, 10  # no. of elements in second array

lui x13, 0x10000  # points to first element of a1
lui x14, 0x10000

add x5, x11, x0
slli x5, x5, 2
add x14, x14, x5  # points to first element of second array

addi x5, x0, 0
addi x6, x0, 0

lui x15, 0x10002  # points to the merged array

loop1:
blt x5, x11, label1
blt x6, x12, large
beq x0, x0, exit

label1:
blt x6, x12, label2
beq x0, x0, small

label2:
lw x28, 0(x13)
lw x29, 0(x14)
blt x28, x29, small
beq x0, x0, large

small:
sw x28, 0(x15)
addi x13, x13, 4
addi x5, x5, 1
addi x15, x15, 4
beq x0, x0, loop1

large:
sw x29, 0(x15)
addi x14, x14, 4
addi x6, x6, 1
addi x15, x15, 4
beq x0, x0, loop1

exit:


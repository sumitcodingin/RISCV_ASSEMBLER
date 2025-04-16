# brajesh khokad , 2023csb1111 , question_2
.text
.data 
a1: .word 1,2,3,4,5           # created array a1 with n1 sorted elements
a2: .word 5,6,7,8             # created array a2 with n2 sorted elements

.text
_start:

addi x18, x0, 5              # x18 = s2 = n1
addi x19, x0, 4              # x19 = s3 = n2
addi x30, x0, 4              # x30 = t5 = 4
mul  x30, x30, x18           # x30 = 4 * n1 (used to calculate a2 address)

add x20, x18, x19            # x20 = s4 = total merged length

addi x21, x0, 0              # x21 = s5 = counter for a1
addi x22, x0, 0              # x22 = s6 = counter for a2

lui x5, 0x10000              # x5 = t0 = base address of a1
addi x5, x5, 0x000           # full address of a1
add x6, x5, x30              # x6 = t1 = base address of a2
lui x7, 0x10002              # x7 = t2 = base address of a3
addi x7, x7, 0x000

merge:
beq x21, x18, case3
beq x22, x19, case4

lw x28, 0(x5)                # x28 = t3 = a1 element
lw x29, 0(x6)                # x29 = t4 = a2 element
bge x28, x29, case1
blt x28, x29, case2

case1:
addi x6, x6, 4               # move to next element of a2
sw x29, 0(x7)                # store smaller in a3
addi x7, x7, 4               # move to next position in a3
addi x22, x22, 1             # increment a2 counter
jal x0, merge

case2:
addi x5, x5, 4
sw x28, 0(x7)
addi x7, x7, 4
addi x21, x21, 1
jal x0, merge

case3:
lw x29, 0(x6)
addi x6, x6, 4
sw x29, 0(x7)
addi x7, x7, 4
addi x22, x22, 1
beq x22, x19, done
jal x0, case3

case4:
lw x28, 0(x5)
addi x5, x5, 4
sw x28, 0(x7)
addi x7, x7, 4
addi x21, x21, 1
beq x21, x18, done
jal x0, case4

done:
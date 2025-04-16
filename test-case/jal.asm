.data
n: .word -10
.text

addi x4 , x0 , 4
jal x1 , fact
addi x5 , x0 , 6
beq x0 , x0 ,exit

fact:

addi x6 , x4 ,-2
jalr x7 , x1 , 0
exit:
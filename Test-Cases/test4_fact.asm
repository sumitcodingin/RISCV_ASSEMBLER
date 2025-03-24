.data
n: .word 5

.text

lui x5, 0x10000
lw x5, 0(x5)
jal x1, fact

beq x0, x0, exit
fact:

addi x2, x2, -8

sw x5, 0(x2)
sw x1, 4(x2)

addi x3, x0, 1 # 1

blt x3, x5, else  # if n>1 go to else

addi x5, x0, 1 # n=1
jalr x0, x1, 0 # return


else:



addi x5, x5, -1 # n-1
jal x1, fact # go to fact

add x4, x0, x5 # x4 is n
lw x5, 0(x2) # load prev n

lw x1, 4(x2)
addi x2, x2, 8

mul x5, x5, x4
jalr x0, x1, 0 # return add

exit:
.data

string : .string "naman"
ispalindrom: .word 0
n : .word 5
.text

main:

# address of string 
lui x5 , 0x10000 # x5 address of string

# start 
addi x6 , x0 , 0 # start = 0
lui x30 , 0x10000
addi x30 , x30 , 12
lw x7 , 0(x30) # n = 5
addi x7 , x7 , -1 # end = n -1 = 4

jal x1 , isPalindrome
jal x0 , exit



isPalindrome:

addi x2 , x2 , -12
sw x6 , 0(x2)
sw x7 , 0(x2)
sw x1 , 8(x2)



bge x6 , x7 , returnOne

# a register is required to acess string inside memory
# x8 for calculating offset
add x8 , x6 , x5


# x10 for string[start]
lb x10 , 0(x8)

# x11 for string[end]
add x8 , x7 , x5

# x11 for string[end]
lb x11 , 0(x8)


bne x10 , x11 , returnZero

addi x6 , x6 , 1
addi x7 , x7 , -1

jal x1 , isPalindrome


returnOne:
addi x17 , x0 , 1
jal x0 , exit

returnZero:
addi x17 , x0 ,0
jal x0 , exit


exit:

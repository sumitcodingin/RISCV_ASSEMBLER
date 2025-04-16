.data
.word 0, 10, 8, 6, 2, 10, 7, 5, 3, 4, 9, 1

.text

lui x11, 0x10000
lw x12, 0(x11)  # type of operation
addi x11, x11, 4
lw x13, 0(x11)  # no. of elements
addi x11, x11, 4 

addi x19, x0, 0  # we will save the CPI in x19 and also we can see it after running code

addi x17, x11, 0  # storing base address of first element of a1
addi x16, x0, 1
addi x15, x0, 0
addi x14, x13, -1  # counter for j (since it needs to compare a1[j] with a1[j+1], so n-1)
beq x12, x16, loop2_opt  # optimized code will run

addi x5, x0, 0  # counter for i

loop1:
addi x19, x19, 1
bge x5, x13, exit  # if i is greater than or equal to n then exit
bge x6, x14, increment_i

addi x28, x11, 0
addi x29, x11, 4

lw x30, 0(x28)
lw x31, 0(x29)

blt x30, x31, next  # if they are already sorted then no need to swap

sw x31, 0(x28)
sw x30, 0(x29)

next:  # increment the value of j
addi x6, x6, 1
addi x11, x11, 4
beq x0, x0, loop1  

increment_i:  # increment the value of i
addi x5, x5, 1
addi x6, x0, 0
addi x11, x17, 0
beq x0, x0, loop1

loop2_opt:
addi x19, x19, 1  # adding cycle count
bge x5, x13, exit  
bge x6, x14, increment_i_opt

addi x28, x11, 0
addi x29, x11, 4

lw x30, 0(x28)
lw x31, 0(x29)

blt x30, x31, next_opt

sw x31, 0(x28)
sw x30, 0(x29)
addi x15, x0, 1

next_opt:  # these functions are same as the above one
addi x6, x6, 1
addi x11, x11, 4
beq x0, x0, loop2_opt

increment_i_opt:  
beq x15, x0, exit  # checking the condition if the flag is true or not
addi x15, x0, 0  # again setting flag to false
addi x5, x5, 1
addi x6, x0, 0
addi x11, x17, 0
addi x14, x14, -1  # decreasing loop for j 
beq x0, x0, loop2_opt

exit:  # we have copied the content of sorted file into another given location
lui x12, 0x10005
addi x11, x17, 0
addi x5, x0, 0

loop_add:
bge x5, x13, ee
lw x30, 0(x11)
sw x30, 0(x12)
addi x11, x11, 4
addi x12, x12, 4
addi x5, x5, 1
beq x0, x0, loop_add

ee:
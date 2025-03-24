.data
pr: .string "life_vikash"

.text
lui x11, 0x10000  # Load upper immediate for source string
lui x12, 0x10001  # Load upper immediate for destination string
addi x5, x0, 0    # counter
addi x6, x0, 59   # total characters
addi x7, x0, 97   # ASCII value of 'a'
addi x8, x0, 123  # ASCII value of '{' (one past 'z')

loop1:
beq x5, x6, exit
lb x13, 0(x11)    # Load byte from source
bge x13, x7, label2
beq x0, x0, skip

label2:
blt x13, x8, label3
beq x0, x0, skip

label3:
addi x13, x13, -32  # Convert lowercase to uppercase
beq x0, x0, skip

skip:
sb x13, 0(x12)  # Store byte to destination
addi x11, x11, 1
addi x12, x12, 1
addi x5, x5, 1
beq x0, x0, loop1

exit:


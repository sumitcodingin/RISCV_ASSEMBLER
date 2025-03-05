ERROR: Unknown instruction # Arithmetic Instructions
0x4 0x00000000001100010000000010110011 , add x1, x2, x3       # x1 = x2 + x3
0x8 0x01000000011000101000001000110011 , sub x4, x5, x6       # x4 = x5 - x6
ERROR: Unknown instruction mul x7, x8, x9       # x7 = x8 * x9
ERROR: Unknown instruction div x10, x11, x12    # x10 = x11 / x12
ERROR: Unknown instruction 
ERROR: Unknown instruction # Logical Instructions
0x1c 0x00000000111101110111011010110011 , and x13, x14, x15    # x13 = x14 & x15
0x20 0x00000001001010001110100000110011 , or x16, x17, x18     # x16 = x17 | x18
0x24 0x00000001010110100100100110110011 , xor x19, x20, x21    # x19 = x20 ^ x21
0x28 0x00000001100010111001101100110011 , sll x22, x23, x24    # x22 = x23 << x24 (shift left logical)
0x2c 0x00000001101111010101110010110011 , srl x25, x26, x27    # x25 = x26 >> x27 (shift right logical)
ERROR: Unknown instruction 
ERROR: Unknown instruction # Immediate Instructions
0x38 0x00000000101011101000111000010011 , addi x28, x29, 10    # x28 = x29 + 10
0x3c 0x00000000111111111111111100010011 , andi x30, x31, 15    # x30 = x31 & 15
0x40 0x00000001010000010110000010010011 , ori x1, x2, 20       # x1 = x2 | 20
ERROR: Unknown instruction 
ERROR: Unknown instruction # Upper Immediate Instructions
ERROR: Unknown instruction lui x2, 0x12345      # Load upper immediate: x2 = 0x12345000
ERROR: Unknown instruction auipc x3, 0x678      # Add upper immediate to PC: x3 = PC + 0x678000
ERROR: Unknown instruction 
ERROR: Unknown instruction # Control Flow Instructions
ERROR: Unknown instruction beq x3, x4, label    # if (x3 == x4) jump to label
ERROR: Unknown instruction bne x5, x6, label    # if (x5 != x6) jump to label
ERROR: Unknown instruction jal x7, label        # jump and link to label, store return address in x7
ERROR: Unknown instruction jalr x8, x9, 0       # jump and link register, store return address in x8
ERROR: Unknown instruction 
ERROR: Unknown instruction # Load and Store Instructions
ERROR: Invalid registers in lw x10, 0(x11)       # load word from memory address x11 + 0 into x10
ERROR: Unknown instruction sw x12, 4(x13)       # store word from x12 into memory address x13 + 4
ERROR: Unknown instruction 
ERROR: Unknown instruction # Set Instructions
ERROR: Unknown instruction slt x14, x15, x16    # x14 = (x15 < x16) ? 1 : 0
ERROR: Unknown instruction slti x17, x18, 5     # x17 = (x18 < 5) ? 1 : 0

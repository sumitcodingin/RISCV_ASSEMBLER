# Arithmetic Instructions
add x1, x2, x3       # x1 = x2 + x3
sub x4, x5, x6       # x4 = x5 - x6
mul x7, x8, x9       # x7 = x8 * x9
div x10, x11, x12    # x10 = x11 / x12

# Logical Instructions
and x13, x14, x15    # x13 = x14 & x15
or x16, x17, x18     # x16 = x17 | x18
xor x19, x20, x21    # x19 = x20 ^ x21
sll x22, x23, x24    # x22 = x23 << x24 (shift left logical)
srl x25, x26, x27    # x25 = x26 >> x27 (shift right logical)

# Immediate Instructions
addi x28, x29, 10    # x28 = x29 + 10
andi x30, x31, 15    # x30 = x31 & 15
ori x1, x2, 20       # x1 = x2 | 20

# Upper Immediate Instructions
lui x2, 0x12345      # Load upper immediate: x2 = 0x12345000
auipc x3, 0x678      # Add upper immediate to PC: x3 = PC + 0x678000

# Control Flow Instructions
beq x3, x4, label    # if (x3 == x4) jump to label
bne x5, x6, label    # if (x5 != x6) jump to label
jal x7, label        # jump and link to label, store return address in x7
jalr x8, x9, 0       # jump and link register, store return address in x8

# Load and Store Instructions
lw x10, 0(x11)       # load word from memory address x11 + 0 into x10
sw x12, 4(x13)       # store word from x12 into memory address x13 + 4

# Set Instructions
slt x14, x15, x16    # x14 = (x15 < x16) ? 1 : 0
slti x17, x18, 5     # x17 = (x18 < 5) ? 1 : 0
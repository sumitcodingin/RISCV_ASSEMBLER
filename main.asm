# Data
.data
       .word 1 2 3
       .half 4
       .asciiz "Risc-V"

# Text segment
.text 
# Label
label:
       # R
       add x2, x0, x3
       # I
       addi x4, x2, 10
       # S
       sw x2, 12(x10)
       # SB
       bne x5, x2, label
       # U
       lui x10, 1024
       # UJ
       jal x10, label
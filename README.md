# *RISC-V Assembler*

This project implements a simple assembler for a subset of the RISC-V 32-bit Instruction Set Architecture (ISA). The assembler reads an assembly source file (main.asm) and generates a machine code output file (main.mc). It supports encoding instructions, resolving labels, and initializing memory segments as defined in the assembly code.


## *Authors*

### SUMIT SHARMA

*Entry No:* 2023CSB1165
 SUMIT is Specializes in software development with expertise in C++. Worked on instruction set architecture and ensuring accurate translation of assembly instructions into machine code.
  

  


### Ayush Sonika

*Entry No:* 2023CSB1107
AYUSH is  Responsible for overall project management and integration of various components.and Focused on implementing parsing and code generation modules.




---

## *Features*

### *Supported Instruction Formats*

The assembler supports the following RISC-V instruction formats and operations:

- *R-Type Instructions*: add, sub, and, or, xor, sll, srl, sra, slt, mul, div, rem
- *I-Type Instructions*: addi, andi, ori, lb, lh, lw, ld, jalr
- *S-Type Instructions*: sb, sh, sw, sd
- *SB-Type Instructions*: beq, bne, bge, blt
- *U-Type Instructions*: lui, auipc
- *UJ-Type Instructions*: jal


### *Supported Directives*

- .text and .data segments
- Data directives: .byte, .half, .word, .dword
- String directive: .asciz


### *Memory Layout*

- Code segment starts at: **0x1000 0000**
- Heap starts at: **0x1000 8000**
- Stack starts at: **0x7FFF FFDC**

---

## *File Structure*

### *Main Folder Contents*

| File Name | Description |
| :-- | :-- |
| main.asm | Input assembly file containing RISC-V instructions |
| main.mc | Output machine code file with code and data segments |
| part1code.cpp | Core implementation of the assembler logic |
| Auxiliary_Functions.h | Header file containing helper functions for parsing and encoding |
| Instructions_Func.h | Header file defining functions for instruction encoding |
| Riscv_Instructions.h | Header file defining constants and formats for RISC-V instructions |
| README.md | Documentation for the project |

---

## *Build and Execution*

### *Compilation*

To compile the assembler, run the following command in the terminal:

bash
g++ -std=c++11 part1code.cpp -o assembler



### *Execution*

To execute the assembler:

bash
./assembler


The program reads from the input assembly file (main.asm) and generates the output machine code file (main.mc) in the same directory.

---

## *Input and Output Example*

### Input File (main.asm)
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
```


### Output File (main.mc)

00000000000000000000000000000000 00000000001100000000000100110011
00000000000000000000000000000100 00000000101000010000001000010011
00000000000000000000000000001000 00000000001001010010011000100011
00000000000000000000000000001100 11111110001000101001101011100011
00000000000000000000000000010000 00000000010000000000010100110111
00000000000000000000000000010100 11111110110111111111010101101111
00010000000000000000000000000000 00000000000000000000000000000001
00010000000000000000000000000100 00000000000000000000000000000010
00010000000000000000000000001000 00000000000000000000000000000011
00010000000000000000000000001100 0000000000000100
00010000000000000000000000001110 01010010
00010000000000000000000000001111 01101001
00010000000000000000000000010000 01110011
00010000000000000000000000010001 01100011
00010000000000000000000000010010 00101101
00010000000000000000000000010011 01010110

## *Usage Notes*

1. Ensure that your input assembly file (main.asm) follows standard RISC-V syntax.
2. The assembler parses instructions line by line and resolves labels during a second pass.
3. The output machine code includes both a detailed breakdown of instruction bit-fields for debugging and a data segment for memory initialization.

---

## *Limitations*

1. Pseudoinstructions are not supported.
2. Floating-point instructions are not supported.
3. Branch offsets are computed relative to the next instruction (PC + 4).
4. The assembler does not simulate runtime memory updates.

---
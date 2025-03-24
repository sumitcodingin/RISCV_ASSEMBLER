#include "Assembler.h"
#include "Simulator.h"

#include <iostream>

int main()
{
    Assembler assembler("input.asm", "output.mc", "PC.pc");
    Simulator simulator("PC.pc", "FinalOutput.txt", "memory.mem", "register.reg");
    cout << "Sumit" << endl;
    return 0;
}
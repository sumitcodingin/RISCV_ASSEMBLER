#ifndef RISCV_INSTRUCTIONS_H // This needs to be unique in each header
#define RISCV_INSTRUCTIONS_H

#include <unordered_map>
#include <string.h>
#include <limits.h>

using namespace std;

unordered_map<string, int> dataTypeSize = {
    {".byte", 1},
    {".half", 2},
    {".word", 4},
    {".dword", 8},
    {".asciiz", 1},
    {".string" , 1}
};

// Op Codes Map
unordered_map<string, string> R_opcode_map = {
    // R_Type
    {"add", "0110011"},
    {"and", "0110011"},
    {"or", "0110011"},
    {"sll", "0110011"},
    {"slt", "0110011"},
    {"sra", "0110011"},
    {"srl", "0110011"},
    {"sub", "0110011"},
    {"xor", "0110011"},
    {"mul", "0110011"},
    {"div", "0110011"},
    {"rem", "0110011"},
};

unordered_map<string, string> I_opcode_map = {
    // I_Type
    {"addi", "0010011"},
    {"andi", "0010011"},
    {"ori", "0010011"},
    {"lb", "0000011"},
    {"ld", "0000011"},
    {"lh", "0000011"},
    {"lw", "0000011"},
    {"jalr", "1100111"},
    {"slti", "0010011"},
    {"sltiu", "0010011"},
    {"xori", "0010011"},
    {"lbu", "0000011"},
    {"lhu", "0000011"},
};
unordered_map<string, string> I_new_opcode{
  
    
    {"slli", "0010011"},
    {"srli", "0010011"},
    {"srai", "0010011"},
    
  

}
;
unordered_map<string, string> S_opcode_map = {
    // S_Type
    {"sb", "0100011"},
    {"sw", "0100011"},
    {"sd", "0100011"},
    {"sh", "0100011"},
};

unordered_map<string, string> SB_opcode_map = {
    // SB_Type
    {"beq", "1100011"},
    {"bne", "1100011"},
    {"bge", "1100011"},
    {"blt", "1100011"},
    
};

unordered_map<string, string> U_opcode_map = {
    // U_Type
    {"auipc", "0010111"},
    {"lui", "0110111"},
};

unordered_map<string, string> UJ_opcode_map = {
    // UJ_Type
    {"jal", "1101111"}};

// func3 map
unordered_map<string, string> func3_map = {
    // R-Type
    {"add", "000"},
    {"and", "111"},
    {"or", "110"},
    {"sll", "001"},
    {"slt", "010"},
    {"sra", "101"},
    {"srl", "101"},
    {"sub", "000"},
    {"xor", "100"},
    {"mul", "000"},
    {"div", "100"},
    {"rem", "110"},

    // I_Type
    {"addi", "000"},
    {"andi", "111"},
    {"ori", "110"},
    {"lb", "000"},
    {"ld", "011"},
    {"lh", "001"},
    {"lw", "010"},
    {"jalr", "000"},
    {"slti", "010"},
    {"sltiu", "011"},
    {"xori", "100"},
    {"lbu", "100"}, 
    {"lhu", "101"},
    
    // I new
    
  
    {"slli", "001"},
    {"srli", "101"},
    {"srai", "101"},
    



    // S_Type
    {"sb", "000"},
    {"sw", "010"},
    {"sd", "011"},
    {"sh", "001"},

    // SB_Type
    {"beq", "000"},
    {"bne", "001"},
    {"bge", "101"},
    {"blt", "100"},
    
};

// func7 map
unordered_map<string, string> func7_map = {
    // R-Type
    {"add", "0000000"},
    {"and", "0000000"},
    {"or", "0000000"},
    {"sll", "000000"},
    {"slt", "000000"},
    {"sra", "0100000"},
    {"srl", "0000000"},
    {"sub", "0100000"},
    {"xor", "0000000"},
    {"mul", "0000001"},
    {"div", "0000001"},
    {"rem", "0000001"},
    

    // I new Type
    {"slli", "0000000"},
    {"srli", "0000000"},
    {"srai", "0100000"}, 
    
   

};

// Risc-V Directives
class Directives
{
    int TEXT, DATA, BYTE, HALF,
        WORD, DWORD, ASCIZ;
};

// Inherting all instructions into a single class
class RISC_V_Instructions
{
    // To Make elements available for public
public:
    int rd = INT_MIN, rs1 = INT_MIN, rs2 = INT_MIN, imm = INT_MIN, type = 1;
    string OpCode = "NONE", Instruction = "NONE", func3 = "NONE", func7 = "NONE";

    void printInstruction()
    {
        cout << "OpCode: " << OpCode << endl;
        cout << "rd: " << rd << endl;
        cout << "rs1: " << rs1 << endl;
        cout << "rs2: " << rs2 << endl;
        cout << "func3: " << func3 << endl;
        cout << "func7: " << func7 << endl;
        cout << "Imm:" << imm <<endl;
    }
};

// Function Definations
const bool isDirective(const string &line)
{
    return line[0] == '.';
}

#endif

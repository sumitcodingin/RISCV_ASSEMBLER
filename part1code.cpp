#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <string>

using namespace std;

// Register Mapping
unordered_map<string, int> reg_map = {
    {"x0", 0}, {"x1", 1}, {"x2", 2}, {"x3", 3}, {"x4", 4}, {"x5", 5}, {"x6", 6}, {"x7", 7},
    {"x8", 8}, {"x9", 9}, {"x10", 10}, {"x11", 11}, {"x12", 12}, {"x13", 13}, {"x14", 14}, {"x15", 15},
    {"x16", 16}, {"x17", 17}, {"x18", 18}, {"x19", 19}, {"x20", 20}, {"x21", 21}, {"x22", 22}, {"x23", 23},
    {"x24", 24}, {"x25", 25}, {"x26", 26}, {"x27", 27}, {"x28", 28}, {"x29", 29}, {"x30", 30}, {"x31", 31}
};

// Instruction Structures
struct RType { int opcode, funct3, funct7; };
struct IType { int opcode, funct3; };
struct SType { int opcode, funct3; };
struct SBType { int opcode, funct3; };
struct UType { int opcode; };
struct UJType { int opcode; };

// Instruction Sets
unordered_map<string, RType> r_instructions = {
    {"add", {0x33, 0x0, 0x0}}, {"sub", {0x33, 0x0, 0x20}}, {"and", {0x33, 0x7, 0x0}}, {"or", {0x33, 0x6, 0x0}},
    {"sll", {0x33, 0x1, 0x0}}, {"slt", {0x33, 0x2, 0x0}}, {"sra", {0x33, 0x5, 0x20}}, {"srl", {0x33, 0x5, 0x0}}
};

unordered_map<string, IType> i_instructions = {
    {"addi", {0x13, 0x0}}, {"andi", {0x13, 0x7}}, {"ori", {0x13, 0x6}}, {"lb", {0x03, 0x0}}, {"jalr", {0x67, 0x0}}
};

unordered_map<string, SType> s_instructions = {
    {"sb", {0x23, 0x0}}, {"sw", {0x23, 0x2}}, {"sh", {0x23, 0x1}}
};

unordered_map<string, SBType> sb_instructions = {
    {"beq", {0x63, 0x0}}, {"bne", {0x63, 0x1}}, {"blt", {0x63, 0x4}}, {"bge", {0x63, 0x5}}
};

unordered_map<string, UType> u_instructions = {
    {"lui", {0x37}}, {"auipc", {0x17}}
};

unordered_map<string, UJType> uj_instructions = {
    {"jal", {0x6f}}
};

// Utility Functions
string reg_to_bin(int reg) { return bitset<5>(reg).to_string(); }
string imm_to_bin(int imm, int bit_size) { return bitset<32>(imm).to_string().substr(32 - bit_size); }

// Assembler Class
class RISCAssembler {
public:
    static string assemble_r_type(string instruction, string rd, string rs1, string rs2) {
        RType instr = r_instructions[instruction];
        return bitset<7>(instr.funct7).to_string() + reg_to_bin(reg_map[rs2]) +
               reg_to_bin(reg_map[rs1]) + bitset<3>(instr.funct3).to_string() +
               reg_to_bin(reg_map[rd]) + bitset<7>(instr.opcode).to_string();
    }

    static string assemble_i_type(string instruction, string rd, string rs1, int imm) {
        IType instr = i_instructions[instruction];
        return imm_to_bin(imm, 12) + reg_to_bin(reg_map[rs1]) +
               bitset<3>(instr.funct3).to_string() + reg_to_bin(reg_map[rd]) +
               bitset<7>(instr.opcode).to_string();
    }

    static string assemble_s_type(string instruction, string rs1, string rs2, int imm) {
        SType instr = s_instructions[instruction];
        return imm_to_bin(imm, 12).substr(0, 7) + reg_to_bin(reg_map[rs2]) +
               reg_to_bin(reg_map[rs1]) + bitset<3>(instr.funct3).to_string() +
               imm_to_bin(imm, 12).substr(7, 5) + bitset<7>(instr.opcode).to_string();
    }

    static string assemble_sb_type(string instruction, string rs1, string rs2, int imm) {
        SBType instr = sb_instructions[instruction];
        return imm_to_bin(imm, 13).substr(0, 7) + reg_to_bin(reg_map[rs2]) +
               reg_to_bin(reg_map[rs1]) + bitset<3>(instr.funct3).to_string() +
               imm_to_bin(imm, 13).substr(7, 6) + bitset<7>(instr.opcode).to_string();
    }

    static string assemble_u_type(string instruction, string rd, int imm) {
        UType instr = u_instructions[instruction];
        return imm_to_bin(imm, 20) + reg_to_bin(reg_map[rd]) +
               bitset<7>(instr.opcode).to_string();
    }

    static string assemble_uj_type(string instruction, string rd, int imm) {
        UJType instr = uj_instructions[instruction];
        return imm_to_bin(imm, 21) + reg_to_bin(reg_map[rd]) +
               bitset<7>(instr.opcode).to_string();
    }
};

// Main Function
int main() {
    ifstream asm_file("input.asm");
    ofstream mc_file("output.mc");
    string line;
    int address = 0;

    if (asm_file.is_open()) {
        while (getline(asm_file, line)) {
            stringstream ss(line);
            string instruction;
            ss >> instruction;

            if (r_instructions.count(instruction)) {
                string rd, rs1, rs2;
                ss >> rd >> rs1 >> rs2;
                mc_file << RISCAssembler::assemble_r_type(instruction, rd, rs1, rs2) << endl;
            } else if (i_instructions.count(instruction)) {
                string rd, rs1;
                int imm;
                ss >> rd >> rs1 >> imm;
                mc_file << RISCAssembler::assemble_i_type(instruction, rd, rs1, imm) << endl;
            } else if (s_instructions.count(instruction)) {
                string rs1, rs2;
                int imm;
                ss >> rs2 >> imm >> rs1;
                mc_file << RISCAssembler::assemble_s_type(instruction, rs1, rs2, imm) << endl;
            } else if (sb_instructions.count(instruction)) {
                string rs1, rs2;
                int imm;
                ss >> rs1 >> rs2 >> imm;
                mc_file << RISCAssembler::assemble_sb_type(instruction, rs1, rs2, imm) << endl;
            } else if (u_instructions.count(instruction)) {
                string rd;
                int imm;
                ss >> rd >> imm;
                mc_file << RISCAssembler::assemble_u_type(instruction, rd, imm) << endl;
            } else if (uj_instructions.count(instruction)) {
                string rd;
                int imm;
                ss >> rd >> imm;
                mc_file << RISCAssembler::assemble_uj_type(instruction, rd, imm) << endl;
            }
            address += 4;
        }
        asm_file.close();
        mc_file.close();
    } else {
        cerr << "Error: Unable to open input file!" << endl;
    }

    return 0;
}

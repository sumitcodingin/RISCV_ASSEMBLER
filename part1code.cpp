#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <string>

using namespace std;

unordered_map<string, int> reg_map;  // Global variable

void init_register_map() {
    for (int i = 0; i < 32; ++i) {
        reg_map["x" + to_string(i)] = i;
    }

    // ABI Register Names
    reg_map["zero"] = 0; reg_map["ra"] = 1; reg_map["sp"] = 2;
    reg_map["gp"] = 3; reg_map["tp"] = 4; reg_map["t0"] = 5;
    reg_map["t1"] = 6; reg_map["t2"] = 7; reg_map["s0"] = 8; reg_map["fp"] = 8;
    reg_map["s1"] = 9; reg_map["a0"] = 10; reg_map["a1"] = 11;
    reg_map["a2"] = 12; reg_map["a3"] = 13; reg_map["a4"] = 14;
    reg_map["a5"] = 15; reg_map["a6"] = 16; reg_map["a7"] = 17;
    reg_map["s2"] = 18; reg_map["s3"] = 19; reg_map["s4"] = 20;
    reg_map["s5"] = 21; reg_map["s6"] = 22; reg_map["s7"] = 23;
    reg_map["s8"] = 24; reg_map["s9"] = 25; reg_map["s10"] = 26;
    reg_map["s11"] = 27; reg_map["t3"] = 28; reg_map["t4"] = 29;
    reg_map["t5"] = 30; reg_map["t6"] = 31;
}
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
    {"sll", {0x33, 0x1, 0x0}}, {"slt", {0x33, 0x2, 0x0}}, {"sra", {0x33, 0x5, 0x20}}, {"srl", {0x33, 0x5, 0x0}}, 
    {"mul", {0x33, 0x0, 0x01}}, {"div", {0x33, 0x4, 0x01}},{"rem", {0x33, 0x6, 0x01}},{"xor", {0x33, 0x4, 0x00}},{"slt", {0x33, 0x2, 0x0}}
};

unordered_map<string, IType> i_instructions = {
    {"addi", {0x13, 0x0}}, {"andi", {0x13, 0x7}}, {"ori", {0x13, 0x6}}, {"lb", {0x03, 0x0}}, {"jalr", {0x67, 0x0}},{"lh", {0x03, 0x1}}, {"lw", {0x03, 0x2}},
    {"slti", {0x13, 0x2}}
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

string handle_pseudo(string &instruction, string &rd, string &rs1, int &imm) {
    if (instruction == "nop") {
        instruction = "addi";
        rd = "x0";
        rs1 = "x0";
        imm = 0;
    } else if (instruction == "mv") {
        instruction = "addi";
        imm = 0;
    } else if (instruction == "not") {
        instruction = "xori";
        imm = -1;
    } else if (instruction == "neg") {
        instruction = "sub";
        rs1 = "x0";
    } else if (instruction == "li") {
        instruction = "addi";
        rs1 = "x0";
    }
    return instruction;
}

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
            int imm;
            string rd, rs1;
            
            instruction = handle_pseudo(instruction, rd, rs1, imm);

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

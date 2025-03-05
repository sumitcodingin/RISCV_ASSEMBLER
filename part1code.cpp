#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <string>
#include <vector>

using namespace std;

class Assembler {
private:
    unordered_map<string, int> reg_map;
    unordered_map<string, tuple<int, int, int>> r_instructions;
    unordered_map<string, tuple<int, int>> i_instructions;
    unordered_map<string, tuple<int, int>> s_instructions;
    unordered_map<string, tuple<int, int>> sb_instructions;
    unordered_map<string, int> u_instructions;
    unordered_map<string, int> uj_instructions;

    void initRegMap() {
        for (int i = 0; i < 32; ++i) {
            reg_map["x" + to_string(i)] = i;
        }
    }

    void initInstructions() {
        r_instructions = {
            {"add", {0x33, 0x0, 0x00}},
            {"sub", {0x33, 0x0, 0x20}},
            {"and", {0x33, 0x7, 0x00}},
            {"or",  {0x33, 0x6, 0x00}},
            {"xor", {0x33, 0x4, 0x00}},
            {"sll", {0x33, 0x1, 0x00}},
            {"srl", {0x33, 0x5, 0x00}},
            {"sra", {0x33, 0x5, 0x20}}
        };
        
        i_instructions = {
            {"addi", {0x13, 0x0}},
            {"andi", {0x13, 0x7}},
            {"ori",  {0x13, 0x6}},
            {"lw",   {0x03, 0x2}}
        };
    }

    string regToBin(int reg) {
        return bitset<5>(reg).to_string();
    }

    string immToBin(int imm, int bit_size) {
        return bitset<32>(imm).to_string().substr(32 - bit_size);
    }

    string assembleRType(string op, int rd, int rs1, int rs2) {
        if (r_instructions.find(op) == r_instructions.end()) return "ERROR";
        int opcode = get<0>(r_instructions[op]);
        int funct3 = get<1>(r_instructions[op]);
        int funct7 = get<2>(r_instructions[op]);
        return bitset<7>(funct7).to_string() + regToBin(rs2) + regToBin(rs1) +
               bitset<3>(funct3).to_string() + regToBin(rd) + bitset<7>(opcode).to_string();
    }

    string assembleIType(string op, int rd, int rs1, int imm) {
        if (i_instructions.find(op) == i_instructions.end()) return "ERROR";
        int opcode = get<0>(i_instructions[op]);
        int funct3 = get<1>(i_instructions[op]);
        return immToBin(imm, 12) + regToBin(rs1) + bitset<3>(funct3).to_string() +
               regToBin(rd) + bitset<7>(opcode).to_string();
    }

public:
    Assembler() {
        initRegMap();
        initInstructions();
    }

    void assemble(const string &input_file, const string &output_file) {
        ifstream asm_file(input_file);
        ofstream mc_file(output_file);
        string line;
        int address = 0;

        while (getline(asm_file, line)) {
            stringstream ss(line);
            string op, rd, rs1, rs2;
            int imm;
            ss >> op;

            if (r_instructions.find(op) != r_instructions.end()) {
                ss >> rd >> rs1 >> rs2;
                rd.pop_back(); rs1.pop_back();
                if (reg_map.find(rd) == reg_map.end() || reg_map.find(rs1) == reg_map.end() || reg_map.find(rs2) == reg_map.end()) {
                    mc_file << "ERROR: Invalid registers in " << line << "\n";
                    continue;
                }
                string mc = assembleRType(op, reg_map[rd], reg_map[rs1], reg_map[rs2]);
                mc_file << "0x" << hex << address << " 0x" << mc << " , " << line << "\n";
            } else if (i_instructions.find(op) != i_instructions.end()) {
                ss >> rd >> rs1 >> imm;
                rd.pop_back(); rs1.pop_back();
                if (reg_map.find(rd) == reg_map.end() || reg_map.find(rs1) == reg_map.end()) {
                    mc_file << "ERROR: Invalid registers in " << line << "\n";
                    continue;
                }
                string mc = assembleIType(op, reg_map[rd], reg_map[rs1], imm);
                mc_file << "0x" << hex << address << " 0x" << mc << " , " << line << "\n";
            } else {
                mc_file << "ERROR: Unknown instruction " << line << "\n";
            }
            address += 4;
        }
    }
};

int main() {
    Assembler assembler;
    assembler.assemble("input.asm", "output.mc");
    return 0;
} 
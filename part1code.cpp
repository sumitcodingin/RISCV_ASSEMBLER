#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>

using namespace std;

// Global Maps
unordered_map<string, int> reg_map;
unordered_map<string, int> label_map;

// Instruction Structures
struct RType { int opcode, funct3, funct7; };
struct IType { int opcode, funct3; };
struct SType { int opcode, funct3; };
struct SBType { int opcode, funct3; };
struct UType { int opcode; };
struct UJType { int opcode; };

// Instruction Sets
unordered_map<string, RType> r_instructions = {
    {"add",  {0x33, 0x0, 0x00}},
    {"sub",  {0x33, 0x0, 0x20}},
    {"mul",  {0x33, 0x0, 0x01}},
    {"div",  {0x33, 0x4, 0x01}},
    {"rem",  {0x33, 0x6, 0x01}},
    {"and",  {0x33, 0x7, 0x00}},
    {"or",   {0x33, 0x6, 0x00}},
    {"xor",  {0x33, 0x4, 0x00}},
    {"sll",  {0x33, 0x1, 0x00}},
    {"srl",  {0x33, 0x5, 0x00}},
    {"sra",  {0x33, 0x5, 0x20}},
    {"slt",  {0x33, 0x2, 0x00}},
    {"sltu", {0x33, 0x3, 0x00}}
};

unordered_map<string, IType> i_instructions = {
    {"addi",  {0x13, 0x0}},
    {"slti",  {0x13, 0x2}},
    {"sltiu", {0x13, 0x3}},
    {"xori",  {0x13, 0x4}},
    {"ori",   {0x13, 0x6}},
    {"andi",  {0x13, 0x7}},
    {"slli",  {0x13, 0x1}},
    {"srli",  {0x13, 0x5}},
    {"srai",  {0x13, 0x5}},
    {"lb",    {0x03, 0x0}},
    {"lh",    {0x03, 0x1}},
    {"lw",    {0x03, 0x2}},
    {"lbu",   {0x03, 0x4}},
    {"lhu",   {0x03, 0x5}},
    {"jalr",  {0x67, 0x0}}
};

unordered_map<string, SType> s_instructions = {
    {"sb", {0x23, 0x0}},
    {"sh", {0x23, 0x1}},
    {"sw", {0x23, 0x2}}
};

unordered_map<string, SBType> sb_instructions = {
    {"beq",  {0x63, 0x0}},
    {"bne",  {0x63, 0x1}},
    {"blt",  {0x63, 0x4}},
    {"bge",  {0x63, 0x5}},
    {"bltu", {0x63, 0x6}},
    {"bgeu", {0x63, 0x7}}
};

unordered_map<string, UType> u_instructions = {
    {"lui",   {0x37}},
    {"auipc", {0x17}}
};

unordered_map<string, UJType> uj_instructions = {
    {"jal", {0x6f}}
};

// Utility Functions
void init_register_map() {
    for (int i = 0; i < 32; ++i) {
        reg_map["x" + to_string(i)] = i;
    }
    
    // ABI names
    reg_map["zero"] = 0; reg_map["ra"] = 1; reg_map["sp"] = 2;
    reg_map["gp"] = 3; reg_map["tp"] = 4; reg_map["t0"] = 5;
    reg_map["t1"] = 6; reg_map["t2"] = 7; reg_map["s0"] = 8;
    reg_map["fp"] = 8; reg_map["s1"] = 9; reg_map["a0"] = 10;
    reg_map["a1"] = 11; reg_map["a2"] = 12; reg_map["a3"] = 13;
    reg_map["a4"] = 14; reg_map["a5"] = 15; reg_map["a6"] = 16;
    reg_map["a7"] = 17; reg_map["s2"] = 18; reg_map["s3"] = 19;
    reg_map["s4"] = 20; reg_map["s5"] = 21; reg_map["s6"] = 22;
    reg_map["s7"] = 23; reg_map["s8"] = 24; reg_map["s9"] = 25;
    reg_map["s10"] = 26; reg_map["s11"] = 27; reg_map["t3"] = 28;
    reg_map["t4"] = 29; reg_map["t5"] = 30; reg_map["t6"] = 31;
}

string reg_to_bin(int reg) {
    if (reg < 0 || reg > 31) {
        throw runtime_error("Invalid register number: " + to_string(reg));
    }
    return bitset<5>(reg).to_string();
}

string sign_extend(const string& bin, int target_size) {
    char sign_bit = bin[0];
    string extension(target_size - bin.length(), sign_bit);
    return extension + bin;
}string imm_to_bin(int imm, int bit_size, bool is_upper = false) {
    // For upper immediate (LUI, AUIPC)
    if (is_upper) {
        // Verify the immediate is properly aligned for upper immediate
        if (imm % (1 << 12) != 0) {
            throw runtime_error("Upper immediate must be 12-bit aligned");
        }
        // Shift right by 12 to get the upper 20 bits
        imm = imm >> 12;
        bit_size = 20;
    }

    // Calculate range limits based on bit size
    int max_val = (1 << (bit_size - 1)) - 1;
    int min_val = -(1 << (bit_size - 1));
    
    if (imm > max_val || imm < min_val) {
        throw runtime_error("Immediate value " + to_string(imm) + 
                          " out of range for " + to_string(bit_size) + " bits");
    }

    // Handle negative numbers with 2's complement
    if (imm < 0) {
        // Convert to positive and take 2's complement
        unsigned int abs_val = abs(imm);
        bitset<32> bits(abs_val);
        string bin = bits.to_string();
        
        // Invert bits
        for (char& c : bin) {
            c = (c == '0') ? '1' : '0';
        }
        
        // Add 1 for 2's complement
        bool carry = true;
        for (int i = bin.length() - 1; i >= 0 && carry; i--) {
            if (bin[i] == '1') {
                bin[i] = '0';
            } else {
                bin[i] = '1';
                carry = false;
            }
        }
        
        return bin.substr(32 - bit_size);
    } else {
        // For positive numbers
        bitset<32> bits(imm);
        return bits.to_string().substr(32 - bit_size);
    }
}


string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

void first_pass(ifstream& asm_file) {
    string line;
    int address = 0;
    
    while (getline(asm_file, line)) {
        size_t comment_pos = line.find('#');
        if (comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        line = trim(line);
        if (line.empty()) continue;

        size_t label_end = line.find(':');
        if (label_end != string::npos) {
            string label = trim(line.substr(0, label_end));
            label_map[label] = address;
            line = trim(line.substr(label_end + 1));
        }

        if (!line.empty()) {
            stringstream ss(line);
            string instruction;
            ss >> instruction;
            
            if (instruction == "li") {
                string rd;
                int imm;
                ss >> rd >> imm;
                if (imm >= -2048 && imm < 2048) {
                    address += 4;
                } else {
                    address += 8;
                }
            } else {
                address += 4;
            }
        }
    }
    
    asm_file.clear();
    asm_file.seekg(0);
}

// RISCAssembler class
class RISCAssembler {
    public:
        static string assemble_r_type(string instruction, string rd, string rs1, string rs2) {
            if (!reg_map.count(rd) || !reg_map.count(rs1) || !reg_map.count(rs2)) {
                throw runtime_error("Invalid register name");
            }
    
            RType instr = r_instructions[instruction];
            return bitset<7>(instr.funct7).to_string() +"   "+
                   reg_to_bin(reg_map[rs2]) +"   "+
                   reg_to_bin(reg_map[rs1]) +"   "+
                   bitset<3>(instr.funct3).to_string()+"   " +
                   reg_to_bin(reg_map[rd])+"   " +
                   bitset<7>(instr.opcode).to_string();
        }
    
        static string assemble_i_type(string instruction, string rd, string rs1, int imm) {
            if (!reg_map.count(rd) || !reg_map.count(rs1)) {
                throw runtime_error("Invalid register name");
            }
    
            IType instr = i_instructions[instruction];
            return imm_to_bin(imm, 12) +"   "+
                   reg_to_bin(reg_map[rs1]) +"   "+
                   bitset<3>(instr.funct3).to_string()+"   " +
                   reg_to_bin(reg_map[rd])+"   " +
                   bitset<7>(instr.opcode).to_string();
        }
    
        static string assemble_s_type(string instruction, string rs1, string rs2, int imm) {
            if (!reg_map.count(rs1) || !reg_map.count(rs2)) {
                throw runtime_error("Invalid register name");
            }
    
            SType instr = s_instructions[instruction];
            string imm_bin = imm_to_bin(imm, 12);
            return imm_bin.substr(0, 7) +"   "+
                   reg_to_bin(reg_map[rs2])+"   " +
                   reg_to_bin(reg_map[rs1])+"   " +
                   bitset<3>(instr.funct3).to_string()+"   " +
                   imm_bin.substr(7, 5)+"   " +
                   bitset<7>(instr.opcode).to_string();
        }
    
        static string assemble_sb_type(string instruction, string rs1, string rs2, int imm) {
            if (!reg_map.count(rs1) || !reg_map.count(rs2)) {
                throw runtime_error("Invalid register name");
            }
    
            if (imm % 2 != 0) {
                throw runtime_error("Branch offset must be even");
            }
    
            SBType instr = sb_instructions[instruction];
            string imm_bin = imm_to_bin(imm >> 1, 12);
            return imm_bin[0] +
                   imm_bin.substr(2, 6) +"   "+
                   reg_to_bin(reg_map[rs2])+"   " +
                   reg_to_bin(reg_map[rs1]) +"   "+
                   bitset<3>(instr.funct3).to_string() +"   "+
                   imm_bin.substr(8, 4)+"   " +
                   imm_bin[1]+"   "+
                   bitset<7>(instr.opcode).to_string();
        }
    
        static string assemble_u_type(string instruction, string rd, int imm) {
            if (!reg_map.count(rd)) {
                throw runtime_error("Invalid register name");
            }
    
            UType instr = u_instructions[instruction];
            return imm_to_bin(imm, 20) +"   "+
                   reg_to_bin(reg_map[rd]) +"   "+
                   bitset<7>(instr.opcode).to_string();
        }
    
        static string assemble_uj_type(string instruction, string rd, int imm) {
            if (!reg_map.count(rd)) {
                throw runtime_error("Invalid register name");
            }
    
            if (imm % 2 != 0) {
                throw runtime_error("Jump offset must be even");
            }
    
            UJType instr = uj_instructions[instruction];
            string imm_bin = imm_to_bin(imm >> 1, 20);
            return imm_bin[0]+"   " +
                   imm_bin.substr(10, 10)+"   " +
                   imm_bin[9]+"   " +
                   imm_bin.substr(1, 8)+"   " +
                   reg_to_bin(reg_map[rd]) +"   "+
                   bitset<7>(instr.opcode).to_string();
        }
    };
    
    int main() {
        init_register_map();
        ifstream asm_file("input.asm");
        ofstream mc_file("output.mc");
        
        if (!asm_file.is_open()) {
            cerr << "Error: Unable to open input file!" << endl;
            return 1;
        }
    
        // First pass: collect labels
        first_pass(asm_file);
    
        // Second pass: assemble instructions
        string line;
        int line_number = 0;
        int current_address = 0;
    
        while (getline(asm_file, line)) {
            line_number++;
            
            // Remove comments and trim
            size_t comment_pos = line.find('#');
            if (comment_pos != string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            line = trim(line);
            if (line.empty()) continue;
    
            // Skip labels
            size_t label_end = line.find(':');
            if (label_end != string::npos) {
                line = trim(line.substr(label_end + 1));
                if (line.empty()) continue;
            }
    
            try {
                stringstream ss(line);
                string instruction;
                ss >> instruction;
                
                vector<string> operands;
                string operand;
                while (ss >> operand) {
                    if (operand.back() == ',') {
                        operand.pop_back();
                    }
                    operands.push_back(trim(operand));
                }
    
                // Handle each instruction type
                if (r_instructions.count(instruction)) {
                    if (operands.size() != 3) {
                        throw runtime_error("R-type instruction requires 3 operands");
                    }
                    mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                           << RISCAssembler::assemble_r_type(instruction, operands[0], operands[1], operands[2]) << endl;
                }
                else if (i_instructions.count(instruction)) {
                    if (instruction == "lb" || instruction == "lh" || instruction == "lw" || 
                        instruction == "lbu" || instruction == "lhu") {
                        // Handle load instructions
                        if (operands.size() != 2) {
                            throw runtime_error("Load instruction requires 2 operands");
                        }
                        string rd = operands[0];
                        size_t paren_start = operands[1].find('(');
                        size_t paren_end = operands[1].find(')');
                        if (paren_start == string::npos || paren_end == string::npos) {
                            throw runtime_error("Invalid load instruction format");
                        }
                        int imm = stoi(operands[1].substr(0, paren_start));
                        string rs1 = operands[1].substr(paren_start + 1, paren_end - paren_start - 1);
                        mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                               << RISCAssembler::assemble_i_type(instruction, rd, rs1, imm) << endl;
                    }
                    else {
                        if (operands.size() != 3) {
                            throw runtime_error("I-type instruction requires 3 operands");
                        }
                        mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                               << RISCAssembler::assemble_i_type(instruction, operands[0], operands[1], stoi(operands[2])) << endl;
                    }
                }
                else if (s_instructions.count(instruction)) {
                    if (operands.size() != 2) {
                        throw runtime_error("S-type instruction requires 2 operands");
                    }
                    string rs2 = operands[0];
                    size_t paren_start = operands[1].find('(');
                    size_t paren_end = operands[1].find(')');
                    if (paren_start == string::npos || paren_end == string::npos) {
                        throw runtime_error("Invalid store instruction format");
                    }
                    int imm = stoi(operands[1].substr(0, paren_start));
                    string rs1 = operands[1].substr(paren_start + 1, paren_end - paren_start - 1);
                    mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                           << RISCAssembler::assemble_s_type(instruction, rs1, rs2, imm) << endl;
                }
                else if (sb_instructions.count(instruction)) {
                    if (operands.size() != 3) {
                        throw runtime_error("Branch instruction requires 3 operands");
                    }
                    int offset;
                    if (label_map.count(operands[2])) {
                        offset = label_map[operands[2]] - current_address;
                    } else {
                        offset = stoi(operands[2]);
                    }
                    mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                           << RISCAssembler::assemble_sb_type(instruction, operands[0], operands[1], offset) << endl;
                }
                else if (u_instructions.count(instruction)) {
                    if (operands.size() != 2) {
                        throw runtime_error("U-type instruction requires 2 operands");
                    }
                    mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                           << RISCAssembler::assemble_u_type(instruction, operands[0], stoi(operands[1])) << endl;
                }
                else if (uj_instructions.count(instruction)) {
                    if (operands.size() != 2) {
                        throw runtime_error("JAL instruction requires 2 operands");
                    }
                    int offset;
                    if (label_map.count(operands[1])) {
                        offset = label_map[operands[1]] - current_address;
                    } else {
                        offset = stoi(operands[1]);
                    }
                    mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                           << RISCAssembler::assemble_uj_type(instruction, operands[0], offset) << endl;
                }
                else if (instruction == "li") {
                    if (operands.size() != 2) {
                        throw runtime_error("LI requires 2 operands");
                    }
                    int imm = stoi(operands[1]);
                    if (imm >= -2048 && imm < 2048) {
                        mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                               << RISCAssembler::assemble_i_type("addi", operands[0], "x0", imm) << endl;
                    } else {
                        int upper = (imm + 0x800) >> 12;
                        int lower = imm - (upper << 12);
                        mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                               << RISCAssembler::assemble_u_type("lui", operands[0], upper) << endl;
                        current_address += 4;
                        mc_file << hex << setw(8) << setfill('0') << current_address << ": "
                               << RISCAssembler::assemble_i_type("addi", operands[0], operands[0], lower) << endl;
                    }
                }
                else {
                    throw runtime_error("Unknown instruction: " + instruction);
                }
    
                current_address += 4;
            }
            catch (const exception& e) {
                cerr << "Error on line " << line_number << ": " << e.what() << endl;
                continue;
            }
        }
    
        asm_file.close();
        mc_file.close();
        return 0;
    }
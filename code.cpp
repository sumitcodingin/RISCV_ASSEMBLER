#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <cctype>
#include <set>
#include "Riscv_Instructions.h"
#include "Instructions_Func.h"
#include "Auxiliary_Functions.h"

using namespace std;

// Global simulation state
uint32_t pc = 0;                        // Program Counter
uint32_t ir = 0;                        // Instruction Register
array<int32_t, 32> reg_file = {0};      // Register File (x0-x31)
int32_t rm = 0, ry = 0, rz = 0, mar = 0;// Temporary registers
int32_t reg_a_val = 0, reg_b_val = 0;   // ALU operands
uint32_t dst_reg = 0, rs2 = 0;          // Destination and RS2 indices
vector<pair<uint32_t, uint32_t>> code;  // Instruction memory (address, instruction)
vector<pair<uint32_t, int32_t>> memory; // Data memory (address, value)
int clock_cycles = 0;                   // Clock counter
set<uint32_t> visited_pcs;              // Track visited PCs
const int MAX_CYCLES = 10000;           // Max cycles to prevent infinite loop

// Control signals
struct Control {
    bool mem_read = false;
    bool mem_write = false;
    bool reg_write = false;
    bool branch = false;
    bool use_imm = false;
    int output_sel = 0; // 0: ALU, 1: Memory, 2: PC
    string alu_op = "";
    string mem_size = "";
};
Control ctrl;

// Convert uint32_t to hex string
string to_hex(uint32_t val) {
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << uppercase << val;
    return ss.str();
}

// Validate hex string
bool is_valid_hex(const string& str) {
    if (str.empty() || str.length() < 3 || str.substr(0, 2) != "0x") return false;
    for (size_t i = 2; i < str.length(); ++i) {
        if (!isxdigit(str[i])) return false;
    }
    return true;
}

// Initialize simulation
void init_sim() {
    // Clear register file
    reg_file.fill(0);
    reg_file[2] = 0x7FFFFFE4; // Stack pointer
    reg_file[3] = 0x10000000; // Link register
    reg_file[10] = 0x00000001;
    reg_file[11] = 0x07FFFFE4;

    // Clear memory structures
    code.clear();
    memory.clear();
    visited_pcs.clear();

    // Reset program counter and instruction register
    pc = 0;
    ir = 0;

    // Reset clock cycles
    clock_cycles = 0;

    // Reset temporary registers
    rm = 0;
    ry = 0;
    rz = 0;
    mar = 0;

    // Reset ALU operands
    reg_a_val = 0;
    reg_b_val = 0;
    dst_reg = 0;
    rs2 = 0;

    // Reset control signals
    ctrl = Control();
    ctrl.mem_read = false;
    ctrl.mem_write = false;
    ctrl.reg_write = false;
    ctrl.branch = false;
    ctrl.use_imm = false;
    ctrl.output_sel = 0;
    ctrl.alu_op = "";
    ctrl.mem_size = "";
}

// Load text.mc file
bool load_mc_file(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Cannot open " << filename << endl;
        return false;
    }

    string line;
    int line_num = 0;
    while (getline(file, line)) {
        ++line_num;
        size_t comment_pos = line.find('#');
        if (comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty()) continue;

        stringstream ss(line);
        string addr_str, instr_str;
        if (!(ss >> addr_str >> instr_str)) {
            cout << "Warning: Skipping malformed line " << line_num << ": " << line << endl;
            continue;
        }

        if (!is_valid_hex(addr_str) || !is_valid_hex(instr_str)) {
            cout << "Warning: Skipping invalid hex at line " << line_num << ": " << addr_str << " " << instr_str << endl;
            continue;
        }

        try {
            uint32_t addr = stoul(addr_str, nullptr, 16);
            uint32_t instr = stoul(instr_str, nullptr, 16);
            code.emplace_back(addr, instr);
            cout << "Loaded instruction: " << to_hex(addr) << " -> " << to_hex(instr) << endl;
        } catch (const std::invalid_argument& e) {
            cout << "Warning: Invalid number format at line " << line_num << ": " << addr_str << " " << instr_str << endl;
            continue;
        } catch (const std::out_of_range& e) {
            cout << "Warning: Number out of range at line " << line_num << ": " << addr_str << " " << instr_str << endl;
            continue;
        }
    }
    file.close();

    if (code.empty()) {
        cout << "Error: No valid instructions loaded from " << filename << endl;
        return false;
    }
    return true;
}

// Load data.mc file
bool load_data_mc(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Cannot open " << filename << endl;
        return false;
    }

    string line;
    int line_num = 0;
    while (getline(file, line)) {
        ++line_num;
        size_t comment_pos = line.find('#');
        if (comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty()) continue;

        stringstream ss(line);
        string addr_str, value_str;
        if (!(ss >> addr_str >> value_str)) {
            cout << "Warning: Skipping malformed line " << line_num << ": " << line << endl;
            continue;
        }

        if (!is_valid_hex(addr_str) || !is_valid_hex(value_str)) {
            cout << "Warning: Skipping invalid hex at line " << line_num << ": " << addr_str << " " << value_str << endl;
            continue;
        }

        try {
            uint32_t addr = stoul(addr_str, nullptr, 16);
            uint32_t value_inst = stoul(value_str, nullptr, 16); // Signed for memory
            int32_t value = static_cast<int32_t>(value_inst);

            memory.emplace_back(addr, value);
            cout << "Loaded data: " << to_hex(addr) << " -> " << to_hex(value) << endl;
        } catch (const std::invalid_argument& e) {
            cout << "Warning: Invalid number format at line " << line_num << ": " << addr_str << " " << value_str << endl;
            continue;
        } catch (const std::out_of_range& e) {
            cout << "Warning: Number out of range at line " << line_num << ": " << addr_str << " " << value_str << endl;
            continue;
        }
    }
    file.close();

    if (memory.empty()) {
        cout << "Warning: No valid data loaded from " << filename << endl;
    }
    return true;
}

// Write memory to data.mc
void write_data_mc(const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Cannot write to " << filename << endl;
        return;
    }

    for (size_t i = 0; i < memory.size(); ++i) {
        uint32_t addr = memory[i].first;
        int32_t val = memory[i].second;
        file << to_hex(addr) << " " << to_hex(val) << endl;
    }
    file.close();
    cout << "Updated data.mc with current memory state\n";
}

// Fetch stage
void fetch() {
    cout << "\n--- Fetch Stage (Cycle " << clock_cycles << ") ---\n";
    ir = 0;
    for (size_t i = 0; i < code.size(); ++i) {
        uint32_t addr = code[i].first;
        uint32_t instr = code[i].second;
        if (addr == pc) {
            ir = instr;
            break;
        }
    }
    cout << "PC: " << to_hex(pc) << ", IR: " << to_hex(ir) << endl;
    pc += 4; // Default increment
}

// Decode stage
void decode() {
    cout << "\n--- Decode Stage ---\n";
    if (ir == 0) {
        cout << "No instruction to decode - terminating simulation" << endl;
        ctrl = Control();
        ctrl.alu_op = "EXIT";
        reg_a_val = 0;
        reg_b_val = 0;
        dst_reg = 0;
        rm = 0;
        return;
    }

    // Extract fields
    uint32_t opcode = ir & 0x7F;
    uint32_t rd     = (ir >> 7) & 0x1F;
    uint32_t func3  = (ir >> 12) & 0x7;
    uint32_t rs1    = (ir >> 15) & 0x1F;
    rs2    = (ir >> 20) & 0x1F;
    uint32_t func7  = (ir >> 25) & 0x7F;


    
    
    // I-type immediate
    int32_t imm_i = static_cast<int32_t>(ir) >> 20;  // auto sign-extends
    
    // S-type immediate
    int32_t imm_s = ((ir >> 25) << 5) | ((ir >> 7) & 0x1F);
    if (imm_s & 0x800) imm_s |= 0xFFFFF000;  // manual sign-extend
    
    // B-type immediate
    int32_t imm_b = ((ir >> 31) << 12) | (((ir >> 7) & 0x1) << 11) |
                    (((ir >> 25) & 0x3F) << 5) | (((ir >> 8) & 0xF) << 1);
    if (imm_b & 0x1000) imm_b |= 0xFFFFE000;  // sign-extend 13 bits
    
    // U-type immediate
    int32_t imm_u = ir & 0xFFFFF000;  // already aligned and zero-extended
    
    // J-type immediate
    int32_t imm_j = ((ir >> 31) << 20) | (((ir >> 12) & 0xFF) << 12) |
                    (((ir >> 20) & 0x1) << 11) | (((ir >> 21) & 0x3FF) << 1);
    if (imm_j & 0x100000) imm_j |= 0xFFE00000;  // sign-extend 21 bits
    
    cout << "rs1 is " << rs1 << "rs2 is " << rs2 << endl; 
 
    // Reset control signals
    ctrl = Control();
    dst_reg = 0; // Initialize to 0, set only for instructions with rd

    // Decode using string maps
    bool valid = false;
    string alu_op = "";

    // R-type
    if (opcode == stoul(R_opcode_map["add"], nullptr, 2)) {
        ctrl.reg_write = true;
        if (func3 == stoul(func3_map["add"], nullptr, 2) && func7 == stoul(func7_map["add"], nullptr, 2)) alu_op = "ADD";
        else if (func3 == stoul(func3_map["sub"], nullptr, 2) && func7 == stoul(func7_map["sub"], nullptr, 2)) alu_op = "SUB";
        else if (func3 == stoul(func3_map["mul"], nullptr, 2) && func7 == stoul(func7_map["mul"], nullptr, 2)) alu_op = "MUL";
        else if (func3 == stoul(func3_map["and"], nullptr, 2) && func7 == stoul(func7_map["and"], nullptr, 2)) alu_op = "AND";
        else if (func3 == stoul(func3_map["or"], nullptr, 2) && func7 == stoul(func7_map["or"], nullptr, 2)) alu_op = "OR";
        else if (func3 == stoul(func3_map["rem"], nullptr, 2) && func7 == stoul(func7_map["rem"], nullptr, 2)) alu_op = "REM";
        else if (func3 == stoul(func3_map["sll"], nullptr, 2) && func7 == stoul(func7_map["sll"], nullptr, 2)) alu_op = "SLL";
        else if (func3 == stoul(func3_map["slt"], nullptr, 2) && func7 == stoul(func7_map["slt"], nullptr, 2)) alu_op = "SLT";
        else if (func3 == stoul(func3_map["srl"], nullptr, 2) && func7 == stoul(func7_map["srl"], nullptr, 2)) alu_op = "SRL";
        else if (func3 == stoul(func3_map["sra"], nullptr, 2) && func7 == stoul(func7_map["sra"], nullptr, 2)) alu_op = "SRA";
        else if (func3 == stoul(func3_map["xor"], nullptr, 2) && func7 == stoul(func7_map["xor"], nullptr, 2)) alu_op = "XOR";
        else if (func3 == stoul(func3_map["div"], nullptr, 2) && func7 == stoul(func7_map["div"], nullptr, 2)) alu_op = "DIV";
        else {
            cout << "Error: Invalid R-type opcode=0x" << hex << opcode 
                 << " func3=0x" << func3 << " func7=0x" << func7 << endl;
            ir = 0;
            return;
        }
        valid = true;
        ctrl.alu_op = alu_op;
        dst_reg = rd;
    }
    // I-type
    else if (opcode == stoul(I_opcode_map["addi"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        if (func3 == stoul(func3_map["addi"], nullptr, 2)) alu_op = "ADD";
        else if (func3 == stoul(func3_map["andi"], nullptr, 2)) alu_op = "AND";
        else if (func3 == stoul(func3_map["ori"], nullptr, 2)) alu_op = "OR";
        else if (func3 == stoul(func3_map["slti"], nullptr, 2)) alu_op = "SLT";
        else if (func3 == stoul(func3_map["sltiu"], nullptr, 2)) alu_op = "SLTU";
        else if (func3 == stoul(func3_map["xori"], nullptr, 2)) alu_op = "XOR";
        else if (func3 == stoul(func3_map["slli"], nullptr, 2) && func7 == stoul(func7_map["slli"], nullptr, 2)) {
            alu_op = "SLLI";
            imm_i = (ir >> 20) & 0x1F;
        } else if (func3 == stoul(func3_map["srli"], nullptr, 2) && func7 == stoul(func7_map["srli"], nullptr, 2)) {
            alu_op = "SRLI";
            imm_i = (ir >> 20) & 0x1F;
        } else if (func3 == stoul(func3_map["srai"], nullptr, 2) && func7 == stoul(func7_map["srai"], nullptr, 2)) {
            alu_op = "SRAI";
            imm_i = (ir >> 20) & 0x1F;
        } else {
            cout << "Error: Invalid I-type opcode=0x" << hex << opcode 
                 << " func3=0x" << func3 << " func7=0x" << func7 << endl;
            ir = 0;
            return;
        }
        valid = true;
        ctrl.alu_op = alu_op;
        rm = imm_i;
        dst_reg = rd;
    }
    // Load
    else if (opcode == stoul(I_opcode_map["lb"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.mem_read = true;
        ctrl.use_imm = true;
        ctrl.output_sel = 1;
        ctrl.alu_op = "LOAD";
        if (func3 == stoul(func3_map["lb"], nullptr, 2)) ctrl.mem_size = "BYTE";
        else if (func3 == stoul(func3_map["lh"], nullptr, 2)) ctrl.mem_size = "HALF";
        else if (func3 == stoul(func3_map["lw"], nullptr, 2)) ctrl.mem_size = "WORD";
        else if (func3 == stoul(func3_map["ld"], nullptr, 2)) ctrl.mem_size = "DOUBLE";
        else if (func3 == stoul(func3_map["lbu"], nullptr, 2)) ctrl.mem_size = "BYTE";
        else if (func3 == stoul(func3_map["lhu"], nullptr, 2)) ctrl.mem_size = "HALF";
        else {
            cout << "Error: Invalid Load func3=0x" << hex << func3 << endl;
            ir = 0;
            return;
        }
        valid = true;
        rm = imm_i;
        dst_reg = rd;
    }
    
    // S-type
    // S-type
else if (opcode == stoul(S_opcode_map["sb"], nullptr, 2)) {
    ctrl.mem_write = true;
    ctrl.use_imm = true;
    ctrl.alu_op = "STORE";
    if (func3 == stoul(func3_map["sb"], nullptr, 2)) ctrl.mem_size = "BYTE";
    else if (func3 == stoul(func3_map["sh"], nullptr, 2)) ctrl.mem_size = "HALF";
    else if (func3 == stoul(func3_map["sw"], nullptr, 2)) ctrl.mem_size = "WORD";
    else {
        cout << "Error: Invalid Store func3=0x" << hex << func3 << endl;
        ir = 0;
        return;
    }
    valid = true;
    rm = imm_s;              // Immediate for address calculation
    reg_a_val = reg_file[rs1]; // Base address
    reg_b_val = rm;          // Immediate for ALU (address = rs1 + imm)
}
    // SB-type
    else if (opcode == stoul(SB_opcode_map["beq"], nullptr, 2)) {
        ctrl.branch = true;
        ctrl.use_imm = true; // For branch offset
        ctrl.reg_write = false; // No register write
        if (func3 == stoul(func3_map["beq"], nullptr, 2)) ctrl.alu_op = "BEQ";
        else if (func3 == stoul(func3_map["bne"], nullptr, 2)) ctrl.alu_op = "BNE";
        else if (func3 == stoul(func3_map["blt"], nullptr, 2)) ctrl.alu_op = "BLT";
        else if (func3 == stoul(func3_map["bge"], nullptr, 2)) ctrl.alu_op = "BGE";
        else {
            cout << "Error: Invalid SB-type func3=0x" << hex << func3 << endl;
            ir = 0;
            return;
        }
        valid = true;
        rm = imm_b;
        dst_reg = 0; // No destination register
    }
    // U-type
    else if (opcode == stoul(U_opcode_map["lui"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        ctrl.alu_op = "LUI";
        valid = true;
        rm = imm_u;
        dst_reg = rd;
    }
    else if (opcode == stoul(U_opcode_map["auipc"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        ctrl.alu_op = "AUIPC";
        valid = true;
        rm = imm_u;
        dst_reg = rd;
    }
    // UJ-type
    else if (opcode == stoul(UJ_opcode_map["jal"], nullptr, 2)) {  // opcode = 1101111
        ctrl.reg_write = true;
        ctrl.branch = true;
        ctrl.output_sel = 2;
        ctrl.alu_op = "JAL";
        valid = true;
        dst_reg = rd;  // Destination register for return address
        rm = imm_j;    // Jump offset
        
        // For JAL, we don't need reg_a_val or reg_b_val as operands
        reg_a_val = 0;  // Not used for JAL
        reg_b_val = 0;  // Not used for JAL  
    }
    // JALR
    else if (opcode == stoul(I_opcode_map["jalr"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.branch = true;
        ctrl.output_sel = 2;
        ctrl.alu_op = "JALR";
        ctrl.use_imm = true;
        valid = true;
        rm = imm_i;
        dst_reg = rd;
    }
    
    // EXIT
    else if (opcode == 0xFF) {
        ctrl.alu_op = "EXIT";
        valid = true;
    }

    if (!valid) {
        cout << "Error: Unknown opcode 0x" << hex << opcode << endl;
        ir = 0;
        return;
    }

    // Prepare operands
    reg_file[0] = 0;
    if (ctrl.alu_op == "BEQ" || ctrl.alu_op == "BNE" || ctrl.alu_op == "BLT" || ctrl.alu_op == "BGE") {
        reg_a_val = reg_file[rs1];
        reg_b_val = reg_file[rs2]; // Use rs2 for comparison
    }
    else if (ctrl.alu_op == "STORE"){
        reg_a_val = reg_file[rs1];
        cout << rs1<<" is " << to_hex(reg_a_val) << endl;
    } 
    else if (ctrl.alu_op != "LUI" && ctrl.alu_op != "EXIT") {
        reg_a_val = reg_file[rs1];
        reg_b_val = ctrl.use_imm ? rm : reg_file[rs2];
    }
    
    else {
        reg_a_val = 0;
        reg_b_val = rm;
    }

    // Customized output for clarity
    cout << "Opcode: 0x" << hex << opcode;
    if (ctrl.alu_op == "BEQ" || ctrl.alu_op == "BNE" || ctrl.alu_op == "BLT" || ctrl.alu_op == "BGE") {
        cout 
            << ctrl.alu_op << ", RS1: x" << dec << rs1 << " = " << to_hex(reg_a_val)
             << ", RS2: x" << rs2 << " = " << to_hex(reg_b_val)
             << ", Imm: " << to_hex(rm)
             << ", ALU Op: " << ctrl.alu_op << endl;
    } 
    
    else if (ctrl.alu_op == "JAL"){
        cout << "  ,JAL: rd = x" << dst_reg << ", offset = " << to_hex(rm) << endl;
    }
    else if(ctrl.alu_op == "STORE"){
        cout << " RS1 : x" << rs1  << " = "<< to_hex(reg_file[rs1])<< "  RS2 : x" << rs2 << " = " <<to_hex(reg_file[rs2]) <<"  imm : " <<to_hex(rm) << "  ALU_Store  "<< ctrl.alu_op << endl;
    }
    else {
        cout << ", RD: x" << dec << dst_reg
             << ", RS1: x" << rs1 << " = " << to_hex(reg_a_val) 
             << ", RS2/Imm: " << (ctrl.use_imm ? to_hex(reg_b_val) : "x" + to_string(rs2) + " = " + to_hex(reg_b_val))
             << ", ALU Op: " << ctrl.alu_op << endl;
    }
}

// Execute stage
void execute() {
    cout << "\n--- Execute Stage ---\n";
    if (ctrl.alu_op.empty()) {
        cout << "No operation to execute" << endl;
        return;
    }

    int32_t a = reg_a_val, b = reg_b_val;
    bool branch_taken = false;
    rz = 0;

    if (ctrl.alu_op == "ADD") rz = a + b;
    else if (ctrl.alu_op == "SUB") rz = a - b;
    else if (ctrl.alu_op == "MUL") rz = a * b;
    else if (ctrl.alu_op == "DIV") rz = b ? a / b : 0;
    else if (ctrl.alu_op == "REM") rz = b ? a % b : 0;
    else if (ctrl.alu_op == "AND") rz = a & b;
    else if (ctrl.alu_op == "OR") rz = a | b;
    else if (ctrl.alu_op == "XOR") rz = a ^ b;
    else if (ctrl.alu_op == "SLL" || ctrl.alu_op == "SLLI") rz = a << (b & 0x1F);
    else if (ctrl.alu_op == "SRL" || ctrl.alu_op == "SRLI") rz = static_cast<uint32_t>(a) >> (b & 0x1F);
    else if (ctrl.alu_op == "SRA" || ctrl.alu_op == "SRAI") rz = a >> (b & 0x1F);
    else if (ctrl.alu_op == "SLT") rz = (a < b) ? 1 : 0;
    else if (ctrl.alu_op == "LUI") rz = b;
    else if (ctrl.alu_op == "AUIPC") rz = pc - 4 + b;
    else if (ctrl.alu_op == "JAL") {
        rz = pc;  // Save return address (pc + 4)
         // Calculate and update jump target
        branch_taken = 1;
        cout << "JAL: Return address = " << to_hex(rz)   ;
    }else if (ctrl.alu_op == "BEQ") branch_taken = (a == b);
    else if (ctrl.alu_op == "BNE") branch_taken = (a != b);
    else if (ctrl.alu_op == "BLT") branch_taken = (a < b);
    else if (ctrl.alu_op == "BGE") branch_taken = (a >= b);
    else if (ctrl.alu_op == "LOAD" || ctrl.alu_op == "STORE") rz = a + b;
    else if (ctrl.alu_op == "EXIT") rz = 0;
    else if (ctrl.alu_op == "JALR"){ 
        branch_taken = 1;
        rz = pc;  // Save return address (pc + 4)
        cout << "JALR: Return address = " << to_hex(rz) << ", Target = " << to_hex((a + b) & ~1);
    }
    else {
        cout << "Error: Unknown ALU op " << ctrl.alu_op << endl;
        ir = 0;
        return;
    }

    if (ctrl.branch && branch_taken) {
        pc = (ctrl.alu_op == "JALR") ? (a + b) & ~1 : (pc - 4 + rm); // Use rm for branch offset
    }

    if (ctrl.alu_op == "JALR" || ctrl.alu_op == "JAL"){
       cout << ", New PC= " << to_hex(pc) << endl;
       branch_taken = true;
    }

    

    if (ctrl.alu_op == "LOAD" || ctrl.alu_op == "STORE") {
        mar = rz;
        rm = reg_file[rs2]; // For stores
        cout << reg_file[rs2] << rs2 << " in reg _ file " << endl; 
    }

    cout << "ALU Op: " << ctrl.alu_op << ", Result: " << to_hex(rz);
    if (ctrl.branch) cout << ", Branch Taken: " << branch_taken;
    cout << endl;

    
}
// Memory access stage
void memory_access() {
    cout << "\n--- Memory Access Stage ---\n";
    
    ry = rz; // Start by passing ALU result or return address

    cout << "ry  "<< to_hex(ry) << "  rz  "<< to_hex(rz) << endl;

    // Handle memory read
    if (ctrl.mem_read) {
        uint32_t addr = mar;
        int32_t val = 0;
        bool found = false;

        for (const auto& mem_pair : memory) {
            if (mem_pair.first == addr) {
                val = mem_pair.second;
                found = true;
                break;
            }
        }

        if (!found) val = 0; // Default value if address not found

        // Handle byte and halfword loads (sign-extended)
        if (ctrl.mem_size == "BYTE")
            val = (val & 0xFF) | ((val & 0x80) ? 0xFFFFFF00 : 0);
        else if (ctrl.mem_size == "HALF")
            val = (val & 0xFFFF) | ((val & 0x8000) ? 0xFFFF0000 : 0);

        ry = val;
        cout << "Read " << ctrl.mem_size << " from " << to_hex(addr) << ": " << to_hex(ry) << endl;

    }
    // Handle memory write
    else if (ctrl.mem_write) {
        bool found = false;

        for (auto& mem_pair : memory) {
            if (mem_pair.first == mar) {
                mem_pair.second = rm;
                found = true;
                break;
            }
        }

        if (!found)
            memory.push_back({mar, rm});
        cout << "mar  " << to_hex(mar) << " rm " << to_hex(rm) << endl; 
        cout << "Wrote " << ctrl.mem_size << " to " << to_hex(mar) << ": " << to_hex(rm) << endl;

        write_data_mc("data.mc"); // Save after store
    }

    // No override needed for JAL/JALR — ry = rz already has return address

    // Handle EXIT instruction
    if (ctrl.alu_op == "EXIT") {
        write_data_mc("data.mc");
        cout << "EXIT: Wrote memory to data.mc\n";
    }

    cout << "RY: " << to_hex(ry) << endl;
}


// Writeback stage
void writeback() {
    cout << "\n--- Writeback Stage ---\n";
    clock_cycles++;

    if (!ctrl.reg_write || dst_reg == 0) {
        cout << "No register write (RD=x0 or reg_write=0)" << endl;
        return;
    }

    else if( dst_reg == 0){
        cout << "No register write (RD=x0)" << endl;
    }
    
    reg_file[dst_reg] = ry;
    cout << "Wrote x" << dst_reg << " = " << to_hex(ry) << endl;
}

// Main simulation loop
int main() {
    init_sim();
    if (!load_mc_file("text.mc")) {
        cout << "Simulation aborted due to text.mc error\n";
        return 1;
    }
    if (!load_data_mc("data.mc")) {
        cout << "Simulation aborted due to data.mc error\n";
        return 1;
    }

    cout << "Starting RISC-V Simulation\n";
    while (true) {
        if (clock_cycles >= MAX_CYCLES) {
            cout << "Terminated: Maximum cycle limit (" << MAX_CYCLES << ") reached\n";
            write_data_mc("data.mc"); // Save final state
            break;
        }

        cout << "\n===== Cycle " << clock_cycles << " =====\n";
        bool pc_visited = !visited_pcs.insert(pc).second;
        
        // Pipeline stages
        fetch();
        decode();

        // Stop simulation if no instruction to decode
        if (ir == 0) {
            cout << "Simulation terminated: No instruction to decode\n";
            write_data_mc("data.mc"); // Save final state
            break;
        }

        execute();
        
        // Memory access stage
        if (ctrl.mem_read || ctrl.mem_write || ctrl.alu_op == "EXIT" || ctrl.output_sel == 2) {
            memory_access();
        } else {
            ry = rz;
            cout << "\n--- Memory Access Stage (Skipped) ---\n";
            cout << "RY: " << to_hex(ry) << endl;
        }
        
        writeback();

        // Check for EXIT instruction
        if (ctrl.alu_op == "EXIT") {
            cout << "Simulation terminated by EXIT instruction\n";
            break;
        }

        // Detect infinite loop
       
    }

    // Print final simulation statistics
    cout << "\nSimulation Ended\n";
    cout << "Total Clock Cycles: " << clock_cycles << "\n";
    cout << "Final Register State:\n";
    for (int i = 0; i < 32; ++i) {
        cout << "x" << i << ": " << to_hex(reg_file[i]) << endl;
    }
    cout << "Final Memory State:\n";
    for (size_t i = 0; i < memory.size(); ++i) {
        cout << to_hex(memory[i].first) << ": " << to_hex(memory[i].second) << endl;
    }

    return 0;
}
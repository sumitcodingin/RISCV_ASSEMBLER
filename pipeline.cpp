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
#include <map>
#include "Riscv_Instructions.h"
#include "Instructions_Func.h"
#include "Auxiliary_Functions.h"

using namespace std;

// Configuration knobs
struct Knobs {
    bool enable_pipelining = true;          // Knob1: Enable/disable pipelining
    bool enable_data_forwarding = true;     // Knob2: Enable/disable data forwarding
    bool print_reg_file = false;            // Knob3: Print register file after each cycle
    bool print_pipeline_regs = false;       // Knob4: Print pipeline registers after each cycle
    int trace_instruction = -1;             // Knob5: Trace specific instruction (-1 means disabled)
    bool print_branch_predictor = false;    // Knob6: Print branch predictor details after each cycle
    bool enable_verbose = false;          // Knob7: Enable/disable verbose output
} knobs;

// Statistics
struct Stats {
    int total_cycles = 0;                  // Stat1: Total number of cycles
    int total_instructions = 0;            // Stat2: Total instructions executed
    int data_transfer_instructions = 0;    // Stat4: Load/store instructions
    int alu_instructions = 0;              // Stat5: ALU instructions
    int control_instructions = 0;          // Stat6: Control instructions
    int stall_count = 0;                   // Stat7: Number of stalls/bubbles
    int data_hazards = 0;                  // Stat8: Number of data hazards
    int control_hazards = 0;               // Stat9: Number of control hazards
    int branch_mispredictions = 0;         // Stat10: Number of branch mispredictions
    int stalls_data_hazards = 0;           // Stat11: Stalls due to data hazards
    int stalls_control_hazards = 0;        // Stat12: Stalls due to control hazards

    double get_cpi() const {  // Stat3: CPI
        return total_instructions > 0 ? static_cast<double>(total_cycles) / total_instructions : 0;
    }
} stats;

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
    bool is_nop = false;
};

// Pipeline Registers
struct IF_ID_Register {
    uint32_t pc = 0;
    uint32_t ir = 0;
    bool is_valid = false;
    int instr_number = 0;   // For tracking instruction number
};

struct ID_EX_Register {
    uint32_t pc = 0;
    int32_t reg_a_val = 0;
    int32_t reg_b_val = 0;
    int32_t imm = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;   // For tracking instruction number
};

struct EX_MEM_Register {
    uint32_t pc = 0;
    int32_t alu_result = 0;
    int32_t rs2_val = 0;    // For store instructions
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;   // For tracking instruction number
};

struct MEM_WB_Register {
    uint32_t pc = 0;
    int32_t write_data = 0;
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;   // For tracking instruction number
};

// Branch Predictor
class BranchPredictor {
private:
    // 2-bit saturating counter states
    enum PredictorState { STRONGLY_NOT_TAKEN = 0, WEAKLY_NOT_TAKEN = 1, WEAKLY_TAKEN = 2, STRONGLY_TAKEN = 3 };
    static const int PHT_SIZE = 1024;  // Pattern History Table size
    static const int BTB_SIZE = 64;    // Branch Target Buffer size
    
    struct BTBEntry {
        uint32_t pc;
        uint32_t target;
        bool valid;
    };
    
    array<PredictorState, PHT_SIZE> PHT;  // Pattern History Table
    array<BTBEntry, BTB_SIZE> BTB;        // Branch Target Buffer

public:
    BranchPredictor() {
        // Initialize predictor to weakly not taken
        for (int i = 0; i < PHT_SIZE; i++) {
            PHT[i] = WEAKLY_NOT_TAKEN;
        }
        
        // Initialize BTB entries
        for (int i = 0; i < BTB_SIZE; i++) {
            BTB[i].valid = false;
            BTB[i].pc = 0;
            BTB[i].target = 0;
        }
    }
    
    bool predict(uint32_t pc) {
        int index = (pc >> 2) & (PHT_SIZE - 1);  // Hash PC to PHT index
        return PHT[index] >= WEAKLY_TAKEN;      // Predict taken if state is 2 or 3
    }
    
    uint32_t get_target(uint32_t pc) {
        int index = pc % BTB_SIZE;
        if (BTB[index].valid && BTB[index].pc == pc) {
            return BTB[index].target;
        }
        return pc + 4;  // Default prediction (not taken)
    }
    
    void update(uint32_t pc, bool taken, uint32_t actual_target) {
        int pht_index = (pc >> 2) & (PHT_SIZE - 1);
        
        // Update PHT based on actual outcome
        if (taken) {
            if (PHT[pht_index] < STRONGLY_TAKEN) {
                PHT[pht_index] = static_cast<PredictorState>(PHT[pht_index] + 1);
            }
        } else {
            if (PHT[pht_index] > STRONGLY_NOT_TAKEN) {
                PHT[pht_index] = static_cast<PredictorState>(PHT[pht_index] - 1);
            }
        }
        
        // Update BTB with actual target
        int btb_index = pc % BTB_SIZE;
        BTB[btb_index].pc = pc;
        BTB[btb_index].target = actual_target;
        BTB[btb_index].valid = true;
    }
    
    void print_state() {
        cout << "Branch Predictor State:" << endl;
        cout << "PHT (first 8 entries): ";
        for (int i = 0; i < 8; i++) {
            cout << PHT[i] << " ";
        }
        cout << endl;
        
        cout << "BTB (active entries):" << endl;
        for (int i = 0; i < BTB_SIZE; i++) {
            if (BTB[i].valid) {
                cout << "  Index " << i << ": PC=" << to_hex(BTB[i].pc) 
                     << ", Target=" << to_hex(BTB[i].target) << endl;
            }
        }
    }
};

// Global simulation state
uint32_t pc = 0;                         // Program Counter
array<int32_t, 32> reg_file = {0};       // Register File (x0-x31)
vector<pair<uint32_t, uint32_t>> text_memory;  // Instruction memory (address, instruction)
vector<pair<uint32_t, int32_t>> data_memory;   // Data memory (address, value)
const int MAX_CYCLES = 10000;            // Max cycles to prevent infinite loop
int instruction_count = 0;               // Count instructions fetched for unique ID
bool program_done = false;               // Flag to indicate program completion

// Pipeline registers
IF_ID_Register if_id;
ID_EX_Register id_ex;
EX_MEM_Register ex_mem;
MEM_WB_Register mem_wb;

// Hazard/stall controls
bool stall_pipeline = false;
bool flush_pipeline = false;
uint32_t branch_target = 0;

// Branch predictor
BranchPredictor branch_predictor;

// Convert uint32_t to hex string
string to_hex(uint32_t val) {
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << uppercase << val;
    return ss.str();
}

// Check if a hex string is valid
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
    text_memory.clear();
    data_memory.clear();

    // Reset program counter and instruction register
    pc = 0;

    // Reset pipeline registers
    if_id = IF_ID_Register();
    id_ex = ID_EX_Register();
    ex_mem = EX_MEM_Register();
    mem_wb = MEM_WB_Register();

    // Reset hazard controls
    stall_pipeline = false;
    flush_pipeline = false;
    branch_target = 0;

    // Reset stats
    stats = Stats();
    
    // Reset instruction counter
    instruction_count = 0;
    
    // Reset program done flag
    program_done = false;
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
            text_memory.emplace_back(addr, instr);
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

    if (text_memory.empty()) {
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
            uint32_t value_inst = stoul(value_str, nullptr, 16);
            int32_t value = static_cast<int32_t>(value_inst);

            data_memory.emplace_back(addr, value);
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

    return true;
}

// Write memory to data.mc
void write_data_mc(const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Cannot write to " << filename << endl;
        return;
    }

    for (const auto& mem_pair : data_memory) {
        uint32_t addr = mem_pair.first;
        int32_t val = mem_pair.second;
        file << to_hex(addr) << " " << to_hex(val) << endl;
    }
    file.close();
    cout << "Updated data.mc with current memory state\n";
}

// Hazard detection unit - check for data hazards
bool detect_data_hazard() {
    if (!knobs.enable_pipelining) return false;
    
    // No hazard if either stage is invalid
    if (!if_id.is_valid || !id_ex.is_valid) return false;
    
    uint32_t ir = if_id.ir;
    if (ir == 0) return false;
    
    // Extract fields from instruction in decode stage
    uint32_t opcode = ir & 0x7F;
    uint32_t func3 = (ir >> 12) & 0x7;
    uint32_t rs1 = (ir >> 15) & 0x1F;
    uint32_t rs2 = (ir >> 20) & 0x1F;
    
    // Check if instruction uses rs1 or rs2 that is being written by instructions in pipeline
    bool uses_rs1 = (opcode != stoul(U_opcode_map["lui"], nullptr, 2)) && 
                   (opcode != stoul(UJ_opcode_map["jal"], nullptr, 2));
    bool uses_rs2 = (opcode == stoul(R_opcode_map["add"], nullptr, 2)) || 
                   (opcode == stoul(SB_opcode_map["beq"], nullptr, 2)) || 
                   (opcode == stoul(S_opcode_map["sb"], nullptr, 2));
    
    // Check for hazard with instruction in EX stage
    if (id_ex.ctrl.reg_write && id_ex.rd != 0) {
        if ((uses_rs1 && rs1 == id_ex.rd) || (uses_rs2 && rs2 == id_ex.rd)) {
            if (!knobs.enable_data_forwarding || id_ex.ctrl.mem_read) { // Load-use hazard can't be forwarded
                stats.data_hazards++;
                stats.stalls_data_hazards++;
                return true;
            }
        }
    }
    
    // Check for hazard with instruction in MEM stage if forwarding is disabled
    if (!knobs.enable_data_forwarding && ex_mem.ctrl.reg_write && ex_mem.rd != 0) {
        if ((uses_rs1 && rs1 == ex_mem.rd) || (uses_rs2 && rs2 == ex_mem.rd)) {
            stats.data_hazards++;
            stats.stalls_data_hazards++;
            return true;
        }
    }
    
    return false;
}

// Forward unit - determine forwarding paths
void determine_forwarding(uint32_t& rs1_val, uint32_t& rs2_val, uint32_t rs1, uint32_t rs2) {
    if (!knobs.enable_data_forwarding || !knobs.enable_pipelining) return;
    
    // Forward from EX/MEM stage
    if (ex_mem.is_valid && ex_mem.ctrl.reg_write && ex_mem.rd != 0) {
        if (rs1 == ex_mem.rd) {
            rs1_val = ex_mem.alu_result;
            stats.data_hazards++;
        }
        if (rs2 == ex_mem.rd) {
            rs2_val = ex_mem.alu_result;
            stats.data_hazards++;
        }
    }
    
    // Forward from MEM/WB stage (takes precedence over EX/MEM)
    if (mem_wb.is_valid && mem_wb.ctrl.reg_write && mem_wb.rd != 0) {
        if (rs1 == mem_wb.rd) {
            rs1_val = mem_wb.write_data;
            stats.data_hazards++;
        }
        if (rs2 == mem_wb.rd) {
            rs2_val = mem_wb.write_data;
            stats.data_hazards++;
        }
    }
}

// Fetch stage
void fetch() {
    if (program_done) return;
    
    if (!knobs.enable_pipelining) {
        cout << "\n--- Fetch Stage (Cycle " << stats.total_cycles << ") ---\n";
    }

    // Check if we should stall due to hazards
    if (stall_pipeline) {
        if (!knobs.enable_pipelining) {
            cout << "Pipeline stalled - no fetch\n";
        }
        stats.stall_count++;
        return;
    }
    
    // Get instruction from memory
    uint32_t ir = 0;
    for (const auto& instr : text_memory) {
        if (instr.first == pc) {
            ir = instr.second;
            break;
        }
    }
    
    if (ir == 0) {
        // End of program or invalid PC
        program_done = true;
        if (!knobs.enable_pipelining) {
            cout << "No instruction found at PC=" << to_hex(pc) << " - End of program\n";
        }
        return;
    }

    // Use branch prediction for conditional branches
    uint32_t next_pc = pc + 4;
    bool predicted_taken = false;
    
    // Check if this is a branch instruction
    uint32_t opcode = ir & 0x7F;
    if (opcode == stoul(SB_opcode_map["beq"], nullptr, 2) || opcode == stoul(UJ_opcode_map["jal"], nullptr, 2)) {
        predicted_taken = branch_predictor.predict(pc);
        if (predicted_taken) {
            next_pc = branch_predictor.get_target(pc);
            if (!knobs.enable_pipelining) {
                cout << "Branch at " << to_hex(pc) << " predicted taken to " << to_hex(next_pc) << endl;
            }
        }
    }
    
    // Update IF/ID register
    if_id.pc = pc;
    if_id.ir = ir;
    if_id.is_valid = true;
    if_id.instr_number = ++instruction_count;
    
    // Update PC for next cycle
    pc = flush_pipeline ? branch_target : next_pc;
    flush_pipeline = false;
    
    if (!knobs.enable_pipelining) {
        cout << "PC: " << to_hex(if_id.pc) << ", IR: " << to_hex(if_id.ir) << endl;
    }
}

// Helper function to extract immediate values
int32_t extract_immediate(uint32_t ir, char type) {
    switch (type) {
        case 'I': // I-type immediate
            return static_cast<int32_t>(ir) >> 20;  // auto sign-extends
        case 'S': { // S-type immediate
            int32_t imm_s = ((ir >> 25) << 5) | ((ir >> 7) & 0x1F);
            if (imm_s & 0x800) imm_s |= 0xFFFFF000;  // manual sign-extend
            return imm_s;
        }
        case 'B': { // B-type immediate
            int32_t imm_b = ((ir >> 31) << 12) | (((ir >> 7) & 0x1) << 11) |
                         (((ir >> 25) & 0x3F) << 5) | (((ir >> 8) & 0xF) << 1);
            if (imm_b & 0x1000) imm_b |= 0xFFFFE000;  // sign-extend 13 bits
            return imm_b;
        }
        case 'U': // U-type immediate
            return ir & 0xFFFFF000;  // already aligned and zero-extended
        case 'J': { // J-type immediate
            int32_t imm_j = ((ir >> 31) << 20) | (((ir >> 12) & 0xFF) << 12) |
                         (((ir >> 20) & 0x1) << 11) | (((ir >> 21) & 0x3FF) << 1);
            if (imm_j & 0x100000) imm_j |= 0xFFE00000;  // sign-extend 21 bits
            return imm_j;
        }
        default:
            return 0;
    }
}

// Decode stage
void decode() {
    if (!if_id.is_valid) return;
    
    if (!knobs.enable_pipelining) {
        cout << "\n--- Decode Stage ---\n";
    }
    
    uint32_t ir = if_id.ir;
    
    // Check if we should stall due to hazards
    if (stall_pipeline) {
        if (!knobs.enable_pipelining) {
            cout << "Pipeline stalled - no decode\n";
        }
        return;
    }
    
    // Extract fields
    uint32_t opcode = ir & 0x7F;
    uint32_t rd     = (ir >> 7) & 0x1F;
    uint32_t func3  = (ir >> 12) & 0x7;
    uint32_t rs1    = (ir >> 15) & 0x1F;
    uint32_t rs2    = (ir >> 20) & 0x1F;
    uint32_t func7  = (ir >> 25) & 0x7F;
    
    // Initialize control signals
    Control ctrl;
    ctrl.is_nop = false;
    
    // Extract immediate values based on instruction format
    int32_t imm = 0;
    
    // Decode using string maps
    bool valid = false;
    
    // R-type
    if (opcode == stoul(R_opcode_map["add"], nullptr, 2)) {
        ctrl.reg_write = true;
        stats.alu_instructions++;
        
        if (func3 == stoul(func3_map["add"], nullptr, 2) && func7 == stoul(func7_map["add"], nullptr, 2)) 
            ctrl.alu_op = "ADD";
        else if (func3 == stoul(func3_map["sub"], nullptr, 2) && func7 == stoul(func7_map["sub"], nullptr, 2)) 
            ctrl.alu_op = "SUB";
        else if (func3 == stoul(func3_map["mul"], nullptr, 2) && func7 == stoul(func7_map["mul"], nullptr, 2)) 
            ctrl.alu_op = "MUL";
        else if (func3 == stoul(func3_map["and"], nullptr, 2) && func7 == stoul(func7_map["and"], nullptr, 2)) 
            ctrl.alu_op = "AND";
        else if (func3 == stoul(func3_map["or"], nullptr, 2) && func7 == stoul(func7_map["or"], nullptr, 2)) 
            ctrl.alu_op = "OR";
        else if (func3 == stoul(func3_map["rem"], nullptr, 2) && func7 == stoul(func7_map["rem"], nullptr, 2)) 
            ctrl.alu_op = "REM";
        else if (func3 == stoul(func3_map["sll"], nullptr, 2) && func7 == stoul(func7_map["sll"], nullptr, 2)) 
            ctrl.alu_op = "SLL";
        else if (func3 == stoul(func3_map["slt"], nullptr, 2) && func7 == stoul(func7_map["slt"], nullptr, 2)) 
            ctrl.alu_op = "SLT";
        else if (func3 == stoul(func3_map["srl"], nullptr, 2) && func7 == stoul(func7_map["srl"], nullptr, 2)) 
            ctrl.alu_op = "SRL";
        else if (func3 == stoul(func3_map["sra"], nullptr, 2) && func7 == stoul(func7_map["sra"], nullptr, 2)) 
            ctrl.alu_op = "SRA";
        else if (func3 == stoul(func3_map["xor"], nullptr, 2) && func7 == stoul(func7_map["xor"], nullptr, 2)) 
            ctrl.alu_op = "XOR";
        else if (func3 == stoul(func3_map["div"], nullptr, 2) && func7 == stoul(func7_map["div"], nullptr, 2)) 
            ctrl.alu_op = "DIV";
        else {
            cout << "Error: Invalid R-type opcode=0x" << hex << opcode 
                 << " func3=0x" << func3 << " func7=0x" << func7 << endl;
            valid = false;
        }
        valid = true;
    }
    // I-type
    else if (opcode == stoul(I_opcode_map["addi"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        stats.alu_instructions++;
        
        if (func3 == stoul(func3_map["addi"], nullptr, 2)) 
            ctrl.alu_op = "ADD";
        else if (func3 == stoul(func3_map["andi"], nullptr, 2)) 
            ctrl.alu_op = "AND";
        else if (func3 == stoul(func3_map["ori"], nullptr, 2)) 
            ctrl.alu_op = "OR";
        else if (func3 == stoul(func3_map["slti"], nullptr, 2)) 
            ctrl.alu_op = "SLT";
        else if (func3 == stoul(func3_map["sltiu"], nullptr, 2)) 
            ctrl.alu_op = "SLTU";
        else if (func3 == stoul(func3_map["xori"], nullptr, 2)) 
            ctrl.alu_op = "XOR";
        else if (func3 == stoul(func3_map["slli"], nullptr, 2) && func7 == stoul(func7_map["slli"], nullptr, 2)) {
            ctrl.alu_op = "SLLI";
            imm = (ir >> 20) & 0x1F;
        } 
        else if (func3 == stoul(func3_map["srli"], nullptr, 2) && func7 == stoul(func7_map["srli"], nullptr, 2)) {
            ctrl.alu_op = "SRLI";
            imm = (ir >> 20) & 0x1F;
        } 
        else if (func3 == stoul(func3_map["srli"], nullptr, 2) && func7 == stoul(func7_map["srli"], nullptr, 2)) {
            ctrl.alu_op = "SRLI";
            imm = (ir >> 20) & 0x1F;
        }
        else if (func3 == stoul(func3_map["srai"], nullptr, 2) && func7 == stoul(func7_map["srai"], nullptr, 2)) {
            ctrl.alu_op = "SRAI";
            imm = (ir >> 20) & 0x1F;
        }
        else {
            cout << "Error: Invalid I-type opcode=0x" << hex << opcode << " func3=0x" << func3 << endl;
            valid = false;
        }
        imm = extract_immediate(ir, 'I');
        valid = true;
    }
    // Load instructions
    else if (opcode == stoul(I_opcode_map["lb"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.mem_read = true;
        ctrl.use_imm = true;
        ctrl.output_sel = 1; // Memory
        stats.data_transfer_instructions++;
        
        if (func3 == stoul(func3_map["lb"], nullptr, 2)) 
            ctrl.mem_size = "BYTE";
        else if (func3 == stoul(func3_map["lh"], nullptr, 2)) 
            ctrl.mem_size = "HALF";
        else if (func3 == stoul(func3_map["lw"], nullptr, 2)) 
            ctrl.mem_size = "WORD";
        else if (func3 == stoul(func3_map["lbu"], nullptr, 2)) 
            ctrl.mem_size = "BYTE_U";
        else if (func3 == stoul(func3_map["lhu"], nullptr, 2)) 
            ctrl.mem_size = "HALF_U";
        else {
            cout << "Error: Invalid load opcode=0x" << hex << opcode << " func3=0x" << func3 << endl;
            valid = false;
        }
        ctrl.alu_op = "ADD";
        imm = extract_immediate(ir, 'I');
        valid = true;
    }
    // S-type (store)
    else if (opcode == stoul(S_opcode_map["sb"], nullptr, 2)) {
        ctrl.mem_write = true;
        ctrl.use_imm = true;
        stats.data_transfer_instructions++;
        
        if (func3 == stoul(func3_map["sb"], nullptr, 2)) 
            ctrl.mem_size = "BYTE";
        else if (func3 == stoul(func3_map["sh"], nullptr, 2)) 
            ctrl.mem_size = "HALF";
        else if (func3 == stoul(func3_map["sw"], nullptr, 2)) 
            ctrl.mem_size = "WORD";
        else {
            cout << "Error: Invalid store opcode=0x" << hex << opcode << " func3=0x" << func3 << endl;
            valid = false;
        }
        ctrl.alu_op = "ADD";
        imm = extract_immediate(ir, 'S');
        valid = true;
    }
    // SB-type (branch)
    else if (opcode == stoul(SB_opcode_map["beq"], nullptr, 2)) {
        ctrl.branch = true;
        ctrl.use_imm = true;
        stats.control_instructions++;
        
        if (func3 == stoul(func3_map["beq"], nullptr, 2)) 
            ctrl.alu_op = "BEQ";
        else if (func3 == stoul(func3_map["bne"], nullptr, 2)) 
            ctrl.alu_op = "BNE";
        else if (func3 == stoul(func3_map["blt"], nullptr, 2)) 
            ctrl.alu_op = "BLT";
        else if (func3 == stoul(func3_map["bge"], nullptr, 2)) 
            ctrl.alu_op = "BGE";
        else if (func3 == stoul(func3_map["bltu"], nullptr, 2)) 
            ctrl.alu_op = "BLTU";
        else if (func3 == stoul(func3_map["bgeu"], nullptr, 2)) 
            ctrl.alu_op = "BGEU";
        else {
            cout << "Error: Invalid branch opcode=0x" << hex << opcode << " func3=0x" << func3 << endl;
            valid = false;
        }
        imm = extract_immediate(ir, 'B');
        valid = true;
    }
    // U-type (LUI, AUIPC)
    else if (opcode == stoul(U_opcode_map["lui"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        stats.alu_instructions++;
        
        ctrl.alu_op = "LUI";
        imm = extract_immediate(ir, 'U');
        valid = true;
    }
    else if (opcode == stoul(U_opcode_map["auipc"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        stats.alu_instructions++;
        
        ctrl.alu_op = "AUIPC";
        imm = extract_immediate(ir, 'U');
        valid = true;
    }
    // J-type (JAL)
    else if (opcode == stoul(UJ_opcode_map["jal"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.output_sel = 2; // PC
        stats.control_instructions++;
        
        ctrl.alu_op = "JAL";
        imm = extract_immediate(ir, 'J');
        valid = true;
    }
    // I-type (JALR)
    else if (opcode == stoul(I_opcode_map["jalr"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        ctrl.output_sel = 2; // PC
        stats.control_instructions++;
        
        ctrl.alu_op = "JALR";
        imm = extract_immediate(ir, 'I');
        valid = true;
    }
    // Unknown instruction
    else {
        cout << "Error: Unknown opcode: 0x" << hex << opcode << endl;
        valid = false;
    }
    
    if (!valid) {
        cout << "Warning: Unsupported instruction at PC=" << to_hex(if_id.pc) << ": " << to_hex(ir) << endl;
        // Insert a NOP
        ctrl.is_nop = true;
    }
    
    // Get register values
    int32_t reg_a_val = (rs1 == 0) ? 0 : reg_file[rs1];
    int32_t reg_b_val = (rs2 == 0) ? 0 : reg_file[rs2];

    // Update ID/EX Register
    id_ex.pc = if_id.pc;
    id_ex.reg_a_val = reg_a_val;
    id_ex.reg_b_val = reg_b_val;
    id_ex.imm = imm;
    id_ex.rs1 = rs1;
    id_ex.rs2 = rs2;
    id_ex.rd = rd;
    id_ex.ctrl = ctrl;
    id_ex.is_valid = true;
    id_ex.instr_number = if_id.instr_number;
    
    if (!knobs.enable_pipelining) {
        cout << "Instruction: " << to_hex(ir) << ", PC: " << to_hex(if_id.pc) << endl;
        cout << "rs1: x" << rs1 << " = " << reg_a_val << ", rs2: x" << rs2 << " = " << reg_b_val << endl;
        cout << "rd: x" << rd << ", imm: " << imm << endl;
        cout << "Control: " << (ctrl.reg_write ? "RegWrite " : "") 
             << (ctrl.mem_read ? "MemRead " : "") 
             << (ctrl.mem_write ? "MemWrite " : "")
             << (ctrl.branch ? "Branch " : "")
             << "ALU=" << ctrl.alu_op << endl;
    }
}

// Execute stage
void execute() {
    if (!id_ex.is_valid) return;
    
    if (!knobs.enable_pipelining) {
        cout << "\n--- Execute Stage ---\n";
    }
    
    // Apply forwarding
   uint32_t reg_a_val = id_ex.reg_a_val;
    uint32_t reg_b_val = id_ex.reg_b_val;
    determine_forwarding(reg_a_val, reg_b_val, id_ex.rs1, id_ex.rs2);
    
    int32_t alu_result = 0;
    bool branch_taken = false;
    
    // Check for NOP
    if (id_ex.ctrl.is_nop) {
        if (!knobs.enable_pipelining) {
            cout << "NOP instruction" << endl;
        }
        goto alu_done;
    }
    
    // ALU operation
    if (id_ex.ctrl.alu_op == "ADD") {
        alu_result = reg_a_val + (id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val);
    } 
    else if (id_ex.ctrl.alu_op == "SUB") {
        alu_result = reg_a_val - reg_b_val;
    }
    else if (id_ex.ctrl.alu_op == "MUL") {
        alu_result = reg_a_val * reg_b_val;
    }
    else if (id_ex.ctrl.alu_op == "DIV") {
        if (reg_b_val == 0) {
            cout << "Error: Division by zero at PC=" << to_hex(id_ex.pc) << endl;
            alu_result = 0;
        } else {
            alu_result = reg_a_val / reg_b_val;
        }
    }
    else if (id_ex.ctrl.alu_op == "REM") {
        if (reg_b_val == 0) {
            cout << "Error: Remainder by zero at PC=" << to_hex(id_ex.pc) << endl;
            alu_result = 0;
        } else {
            alu_result = reg_a_val % reg_b_val;
        }
    }
    else if (id_ex.ctrl.alu_op == "AND") {
        alu_result = reg_a_val & (id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val);
    }
    else if (id_ex.ctrl.alu_op == "OR") {
        alu_result = reg_a_val | (id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val);
    }
    else if (id_ex.ctrl.alu_op == "XOR") {
        alu_result = reg_a_val ^ (id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val);
    }
    else if (id_ex.ctrl.alu_op == "SLT") {
        alu_result = (reg_a_val < (id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val)) ? 1 : 0;
    }
    else if (id_ex.ctrl.alu_op == "SLTU") {
        alu_result = (static_cast<uint32_t>(reg_a_val) < 
                      static_cast<uint32_t>(id_ex.ctrl.use_imm ? id_ex.imm : reg_b_val)) ? 1 : 0;
    }
    else if (id_ex.ctrl.alu_op == "SLL") {
        alu_result = reg_a_val << (id_ex.ctrl.use_imm ? (id_ex.imm & 0x1F) : (reg_b_val & 0x1F));
    }
    else if (id_ex.ctrl.alu_op == "SRL") {
        alu_result = static_cast<uint32_t>(reg_a_val) >> (id_ex.ctrl.use_imm ? (id_ex.imm & 0x1F) : (reg_b_val & 0x1F));
    }
    else if (id_ex.ctrl.alu_op == "SRA") {
        alu_result = reg_a_val >> (id_ex.ctrl.use_imm ? (id_ex.imm & 0x1F) : (reg_b_val & 0x1F));
    }
    else if (id_ex.ctrl.alu_op == "SLLI") {
        alu_result = reg_a_val << (id_ex.imm & 0x1F);
    }
    else if (id_ex.ctrl.alu_op == "SRLI") {
        alu_result = static_cast<uint32_t>(reg_a_val) >> (id_ex.imm & 0x1F);
    }
    else if (id_ex.ctrl.alu_op == "SRAI") {
        alu_result = reg_a_val >> (id_ex.imm & 0x1F);
    }
    else if (id_ex.ctrl.alu_op == "LUI") {
        alu_result = id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "AUIPC") {
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "JAL") {
        alu_result = id_ex.pc + id_ex.imm;
        branch_taken = true;
    }
    else if (id_ex.ctrl.alu_op == "JALR") {
        alu_result = (reg_a_val + id_ex.imm) & ~1;  // Clear least significant bit
        branch_taken = true;
    }
    // Branch operations
    else if (id_ex.ctrl.alu_op == "BEQ") {
        branch_taken = (reg_a_val == reg_b_val);
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "BNE") {
        branch_taken = (reg_a_val != reg_b_val);
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "BLT") {
        branch_taken = (reg_a_val < reg_b_val);
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "BGE") {
        branch_taken = (reg_a_val >= reg_b_val);
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "BLTU") {
        branch_taken = (static_cast<uint32_t>(reg_a_val) < static_cast<uint32_t>(reg_b_val));
        alu_result = id_ex.pc + id_ex.imm;
    }
    else if (id_ex.ctrl.alu_op == "BGEU") {
        branch_taken = (static_cast<uint32_t>(reg_a_val) >= static_cast<uint32_t>(reg_b_val));
        alu_result = id_ex.pc + id_ex.imm;
    }
    else {
        cout << "Error: Unknown ALU operation: " << id_ex.ctrl.alu_op << endl;
    }
    
alu_done:
    // Handle branch misprediction
    if (id_ex.ctrl.branch || id_ex.ctrl.alu_op == "JAL" || id_ex.ctrl.alu_op == "JALR") {
        // Get the predicted target from branch predictor
        bool predicted_taken = branch_predictor.predict(id_ex.pc);
        uint32_t predicted_target = predicted_taken ? branch_predictor.get_target(id_ex.pc) : id_ex.pc + 4;
        
        // Check if prediction was correct
        if ((branch_taken && !predicted_taken) || (!branch_taken && predicted_taken) || 
            (branch_taken && predicted_taken && alu_result != predicted_target)) {
            // Misprediction
            if (!knobs.enable_pipelining) {
                cout << "Branch misprediction at PC=" << to_hex(id_ex.pc) 
                     << " actual=" << (branch_taken ? "taken" : "not taken") 
                     << " predicted=" << (predicted_taken ? "taken" : "not taken") << endl;
            }
            
            // Flush pipeline
            flush_pipeline = true;
            branch_target = branch_taken ? alu_result : id_ex.pc + 4;
            stats.branch_mispredictions++;
            stats.control_hazards++;
            stats.stalls_control_hazards++;
        }
        
        // Update branch predictor
        branch_predictor.update(id_ex.pc, branch_taken, alu_result);
    }
    
    // Special case for JAL/JALR: save return address
    if (id_ex.ctrl.alu_op == "JAL" || id_ex.ctrl.alu_op == "JALR") {
        ex_mem.alu_result = id_ex.pc + 4;  // Return address is PC+4
    } else {
        ex_mem.alu_result = alu_result;
    }
    
    // Update EX/MEM register
    ex_mem.pc = id_ex.pc;
    ex_mem.rs2_val = reg_b_val;  // For store instructions
    ex_mem.rd = id_ex.rd;
    ex_mem.ctrl = id_ex.ctrl;
    ex_mem.is_valid = true;
    ex_mem.instr_number = id_ex.instr_number;
    
    if (!knobs.enable_pipelining) {
        cout << "ALU Result: " << alu_result << " (0x" << hex << alu_result << dec << ")" << endl;
        if (id_ex.ctrl.branch) {
            cout << "Branch " << (branch_taken ? "taken" : "not taken") << endl;
        }
    }
}

// Memory stage
void memory() {
    if (!ex_mem.is_valid) return;
    
    if (!knobs.enable_pipelining) {
        cout << "\n--- Memory Stage ---\n";
    }
    
    int32_t mem_result = ex_mem.alu_result;
    
    // Memory operations
    if (ex_mem.ctrl.mem_read) {
        uint32_t addr = ex_mem.alu_result;
        
        // Find data in memory
        bool found = false;
        for (const auto& mem_entry : data_memory) {
            if (mem_entry.first == addr) {
                mem_result = mem_entry.second;
                found = true;
                
                // Handle different sizes
                if (ex_mem.ctrl.mem_size == "BYTE") {
                    mem_result = (mem_result & 0xFF);
                    if (mem_result & 0x80) mem_result |= 0xFFFFFF00;  // Sign extend
                } 
                else if (ex_mem.ctrl.mem_size == "HALF") {
                    mem_result = (mem_result & 0xFFFF);
                    if (mem_result & 0x8000) mem_result |= 0xFFFF0000;  // Sign extend
                }
                else if (ex_mem.ctrl.mem_size == "BYTE_U") {
                    mem_result = (mem_result & 0xFF);  // Zero extend
                }
                else if (ex_mem.ctrl.mem_size == "HALF_U") {
                    mem_result = (mem_result & 0xFFFF);  // Zero extend
                }
                // WORD uses the full 32-bit value
                
                break;
            }
        }
        
        if (!found) {
            cout << "Warning: Memory read at address " << to_hex(addr) << " found no data, returning 0" << endl;
            mem_result = 0;
        }
        
        if (!knobs.enable_pipelining) {
            cout << "Memory Read: Address=" << to_hex(addr) << " Data=" << to_hex(mem_result) << endl;
        }
    }
    else if (ex_mem.ctrl.mem_write) {
        uint32_t addr = ex_mem.alu_result;
        int32_t value = ex_mem.rs2_val;
        
        // Handle different sizes
        if (ex_mem.ctrl.mem_size == "BYTE") {
            // Find existing memory location
            bool found = false;
            for (auto& mem_entry : data_memory) {
                if (mem_entry.first == addr) {
                    // Update only the lowest byte
                    mem_entry.second = (mem_entry.second & ~0xFF) | (value & 0xFF);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Create new memory entry with only the byte set
                data_memory.emplace_back(addr, value & 0xFF);
            }
        }
        else if (ex_mem.ctrl.mem_size == "HALF") {
            // Find existing memory location
            bool found = false;
            for (auto& mem_entry : data_memory) {
                if (mem_entry.first == addr) {
                    // Update only the lowest half-word
                    mem_entry.second = (mem_entry.second & ~0xFFFF) | (value & 0xFFFF);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Create new memory entry with only the half-word set
                data_memory.emplace_back(addr, value & 0xFFFF);
            }
        }
        else { // WORD - full 32-bit
            // Find existing memory location
            bool found = false;
            for (auto& mem_entry : data_memory) {
                if (mem_entry.first == addr) {
                    mem_entry.second = value;
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Create new memory entry
                data_memory.emplace_back(addr, value);
            }
        }
        
        if (!knobs.enable_pipelining) {
            cout << "Memory Write: Address=" << to_hex(addr) << " Data=" << to_hex(value) << endl;
        }
    }
    
    // Update MEM/WB register
    mem_wb.pc = ex_mem.pc;
    mem_wb.write_data = ex_mem.ctrl.output_sel == 1 ? mem_result : ex_mem.alu_result;
    mem_wb.rd = ex_mem.rd;
    mem_wb.ctrl = ex_mem.ctrl;
    mem_wb.is_valid = true;
    mem_wb.instr_number = ex_mem.instr_number;
}

// Writeback stage
void writeback() {
    if (!mem_wb.is_valid) return;
    
    if (!knobs.enable_pipelining) {
        cout << "\n--- Writeback Stage ---\n";
    }
    
    // Register write
    if (mem_wb.ctrl.reg_write && mem_wb.rd != 0) {
        if (mem_wb.ctrl.output_sel == 2) {  // PC+4 for JAL/JALR
            reg_file[mem_wb.rd] = mem_wb.pc + 4;
            
            if (!knobs.enable_pipelining) {
                cout << "Register Write: x" << mem_wb.rd << " = " << to_hex(mem_wb.pc + 4) << " (PC+4)" << endl;
            }
        } else {
            reg_file[mem_wb.rd] = mem_wb.write_data;
            
            if (!knobs.enable_pipelining) {
                cout << "Register Write: x" << mem_wb.rd << " = " << mem_wb.write_data 
                     << " (0x" << hex << mem_wb.write_data << dec << ")" << endl;
            }
        }
    }
    
    // Update statistics
    stats.total_instructions++;
}

// Print the state of all registers (for debugging)
void print_register_file() {
    cout << "\n--- Register File ---\n";
    for (int i = 0; i < 32; i++) {
        if (i % 4 == 0 && i > 0) cout << endl;
        cout << "x" << setw(2) << setfill('0') << i << ": " << setw(10) << setfill(' ') 
             << reg_file[i] << " (0x" << hex << setw(8) << setfill('0') << reg_file[i] << dec << ")  ";
    }
    cout << endl;
}

// Print the state of pipeline registers
void print_pipeline_registers() {
    cout << "\n--- Pipeline Registers ---\n";
    
    cout << "IF/ID: ";
    if (if_id.is_valid) {
        cout << "PC=" << to_hex(if_id.pc) << ", IR=" << to_hex(if_id.ir) << ", INSTR#=" << if_id.instr_number << endl;
    } else {
        cout << "INVALID" << endl;
    }
    
    cout << "ID/EX: ";
    if (id_ex.is_valid) {
        cout << "PC=" << to_hex(id_ex.pc) << ", rs1=" << id_ex.rs1 << "(" << id_ex.reg_a_val << "), "
             << "rs2=" << id_ex.rs2 << "(" << id_ex.reg_b_val << "), "
             << "rd=" << id_ex.rd << ", imm=" << id_ex.imm << ", "
             << "ctrl=" << id_ex.ctrl.alu_op << ", INSTR#=" << id_ex.instr_number << endl;
    } else {
        cout << "INVALID" << endl;
    }
    
    cout << "EX/MEM: ";
    if (ex_mem.is_valid) {
        cout << "PC=" << to_hex(ex_mem.pc) << ", ALU=" << ex_mem.alu_result << ", "
             << "rs2_val=" << ex_mem.rs2_val << ", rd=" << ex_mem.rd << ", "
             << "ctrl=" << (ex_mem.ctrl.mem_read ? "MemRead " : "") 
             << (ex_mem.ctrl.mem_write ? "MemWrite " : "") 
             << (ex_mem.ctrl.reg_write ? "RegWrite " : "")
             << "INSTR#=" << ex_mem.instr_number << endl;
    } else {
        cout << "INVALID" << endl;
    }
    
    cout << "MEM/WB: ";
    if (mem_wb.is_valid) {
        cout << "PC=" << to_hex(mem_wb.pc) << ", WData=" << mem_wb.write_data << ", "
             << "rd=" << mem_wb.rd << ", "
             << "ctrl=" << (mem_wb.ctrl.reg_write ? "RegWrite " : "")
             << "INSTR#=" << mem_wb.instr_number << endl;
    } else {
        cout << "INVALID" << endl;
    }
}

// Trace specific instruction through the pipeline
void trace_instruction(int instr_number) {
    if (if_id.is_valid && if_id.instr_number == instr_number) {
        cout << "Instruction #" << instr_number << " is in IF/ID stage: PC=" << to_hex(if_id.pc) << endl;
    }
    
    if (id_ex.is_valid && id_ex.instr_number == instr_number) {
        cout << "Instruction #" << instr_number << " is in ID/EX stage: PC=" << to_hex(id_ex.pc) 
             << ", ALU=" << id_ex.ctrl.alu_op << endl;
    }
    
    if (ex_mem.is_valid && ex_mem.instr_number == instr_number) {
        cout << "Instruction #" << instr_number << " is in EX/MEM stage: PC=" << to_hex(ex_mem.pc)
             << ", ALU Result=" << ex_mem.alu_result << endl;
    }
    
    if (mem_wb.is_valid && mem_wb.instr_number == instr_number) {
        cout << "Instruction #" << instr_number << " is in MEM/WB stage: PC=" << to_hex(mem_wb.pc)
             << ", Data=" << mem_wb.write_data << endl;
    }
}

// Print statistics
void print_statistics() {
    cout << "\n--- Simulation Statistics ---\n";
    cout << "Total Cycles: " << stats.total_cycles << endl;
    cout << "Total Instructions: " << stats.total_instructions << endl;
    cout << "CPI: " << fixed << setprecision(3) << stats.get_cpi() << endl;
    cout << "Data Transfer Instructions: " << stats.data_transfer_instructions << endl;
    cout << "ALU Instructions: " << stats.alu_instructions << endl;
    cout << "Control Instructions: " << stats.control_instructions << endl;
    cout << "Total Stalls/Bubbles: " << stats.stall_count << endl;
    cout << "Data Hazards: " << stats.data_hazards << endl;
    cout << "Control Hazards: " << stats.control_hazards << endl;
    cout << "Branch Mispredictions: " << stats.branch_mispredictions << endl;
    cout << "Stalls Due to Data Hazards: " << stats.stalls_data_hazards << endl;
    cout << "Stalls Due to Control Hazards: " << stats.stalls_control_hazards << endl;
}

// Parse and process command-line options
void process_options(int argc, char* argv[], string& text_file, string& data_file) {
    text_file = "text.mc";  // Default text file
    data_file = "data.mc";  // Default data file
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "-t" || arg == "--text") {
            if (i + 1 < argc) {
                text_file = argv[++i];
            } else {
                cerr << "Error: Text file path missing after " << arg << endl;
                exit(1);
            }
        } else if (arg == "-d" || arg == "--data") {
            if (i + 1 < argc) {
                data_file = argv[++i];
            } else {
                cerr << "Error: Data file path missing after " << arg << endl;
                exit(1);
            }
        } else if (arg == "-p" || arg == "--pipelining") {
            knobs.enable_pipelining = true;
            cout << "Pipelining enabled" << endl;
        } else if (arg == "-f" || arg == "--forwarding") {
            knobs.enable_data_forwarding = true;
            cout << "Data forwarding enabled" << endl;
        } else if (arg == "-v" || arg == "--verbose") {
            knobs.enable_verbose = true;
            cout << "Verbose output enabled" << endl;
        } else if (arg == "-h" || arg == "--help") {
            cout << "Usage: " << argv[0] << " [options]" << endl;
            cout << "Options:" << endl;
            cout << "  -t, --text FILE      Specify text segment file (default: text.mc)" << endl;
            cout << "  -d, --data FILE      Specify data segment file (default: data.mc)" << endl;
            cout << "  -p, --pipelining     Enable pipelining" << endl;
            cout << "  -f, --forwarding     Enable data forwarding" << endl;
            cout << "  -v, --verbose        Show detailed execution trace" << endl;
            cout << "  -h, --help           Display this help message" << endl;
            exit(0);
        } else {
            cerr << "Unknown option: " << arg << endl;
            cerr << "Use --help for usage information" << endl;
            exit(1);
        }
    }
    
    cout << "Using text file: " << text_file << endl;
    cout << "Using data file: " << data_file << endl;
}

// Main simulation cycle
void run_simulation() {
    // Initialize simulation
    stats.total_cycles = 0;
    stats.total_instructions = 0;
    program_done = false;
    stall_pipeline = false;
    flush_pipeline = false;
    
    cout << "Starting simulation..." << endl;
    
    // Main simulation loop
    while (!program_done || id_ex.is_valid || ex_mem.is_valid || mem_wb.is_valid) {
        if (knobs.enable_verbose) {
            cout << "\n==== Cycle " << stats.total_cycles + 1 << " ====" << endl;
            print_pipeline_registers();
        }
        
        // Check for data hazards
        stall_pipeline = detect_data_hazard();
        
        // Pipeline stages in reverse order to avoid structural hazards
        writeback();  // WB stage
        memory();     // MEM stage
        execute();    // EX stage
        decode();     // ID stage
        fetch();      // IF stage
        
        // Update cycle count
        stats.total_cycles++;
        
        // Handle pipeline flushes for control instructions
        if (flush_pipeline) {
            if_id.is_valid = false;
            id_ex.is_valid = false;
        }
        
        // Shift pipeline for single-cycle execution
        if (!knobs.enable_pipelining) {
            if_id.is_valid = false;
            id_ex.is_valid = false;
            ex_mem.is_valid = false;
            mem_wb.is_valid = false;
        }
        
        if (knobs.enable_verbose) {
            print_register_file();
        }
    }
    
    // Final stats
    print_statistics();
}

// Branch Predictor implementation
class BranchPredictor {
private:
    // Simple 2-bit saturating counter branch predictor
    struct BranchInfo {
        uint8_t counter;       // 2-bit counter: 0-1 predict not taken, 2-3 predict taken
        uint32_t target;       // Branch target address
    };
    
    unordered_map<uint32_t, BranchInfo> branch_table;

public:
    BranchPredictor() {}
    
    // Predict whether branch will be taken
    bool predict(uint32_t branch_pc) {
        if (branch_table.find(branch_pc) == branch_table.end()) {
            // First encounter: Default predict not taken
            return false;
        }
        
        return branch_table[branch_pc].counter >= 2;
    }
    
    // Get the predicted target
    uint32_t get_target(uint32_t branch_pc) {
        if (branch_table.find(branch_pc) == branch_table.end()) {
            return branch_pc + 4;  // Default next PC
        }
        
        return branch_table[branch_pc].target;
    }
    
    // Update predictor based on actual outcome
    void update(uint32_t branch_pc, bool taken, uint32_t target) {
        if (branch_table.find(branch_pc) == branch_table.end()) {
            // First time seeing this branch
            branch_table[branch_pc] = {static_cast<unsigned char>(taken ? 2U : 0U), target};
        } else {
            // Update counter using 2-bit saturation
            if (taken) {
                if (branch_table[branch_pc].counter < 3) {
                    branch_table[branch_pc].counter++;
                }
            } else {
                if (branch_table[branch_pc].counter > 0) {
                    branch_table[branch_pc].counter--;
                }
            }
            
            // Update target
            branch_table[branch_pc].target = target;
        }
    }
};

// Initialize the simulator
void initialize_simulator() {
    // Clear memory
    text_memory.clear();
    data_memory.clear();
    
    // Clear registers
    for (int i = 0; i < 32; i++) {
        reg_file[i] = 0;
    }
    
    // Initialize PC and pipeline registers
    pc = 0;
    if_id.is_valid = false;
    id_ex.is_valid = false;
    ex_mem.is_valid = false;
    mem_wb.is_valid = false;
    
    // Reset statistics
    stats = Stats();
    instruction_count = 0;
    
    // Initialize control flags
    program_done = false;
    stall_pipeline = false;
    flush_pipeline = false;
    branch_target = 0;
}

// Load memory content from files
bool load_memory(const string& text_file, const string& data_file) {
    // Load text segment
    ifstream text_in(text_file);
    if (!text_in) {
        cerr << "Error: Cannot open text file: " << text_file << endl;
        return false;
    }
    
    uint32_t addr, instr;
    while (text_in >> hex >> addr >> instr) {
        text_memory.emplace_back(addr, instr);
        if (knobs.enable_verbose) {
            cout << "Loaded instruction: " << to_hex(addr) << " = " << to_hex(instr) << endl;
        }
    }
    text_in.close();
    
    // Load data segment if provided
    ifstream data_in(data_file);
    if (data_in) {
        while (data_in >> hex >> addr >> instr) {
            data_memory.emplace_back(addr, instr);
            if (knobs.enable_verbose) {
                cout << "Loaded data: " << to_hex(addr) << " = " << to_hex(instr) << endl;
            }
        }
        data_in.close();
    } else {
        cout << "No data file found or could not open: " << data_file << endl;
    }
    
    return true;
}

// Utility function to convert to hex string
string to_hex(uint32_t value) {
    stringstream ss;
    ss << "0x" << hex << setw(8) << setfill('0') << value;
    return ss.str();
}

// Main function
int main(int argc, char* argv[]) {
    // Process command line options
    string text_file, data_file;
    process_options(argc, argv, text_file, data_file);
    
    // Initialize simulator state
    initialize_simulator();
    
    // Load memory from files
    if (!load_memory(text_file, data_file)) {
        return 1;
    }
    
    // Run simulation
    run_simulation();
    
    return 0;
}

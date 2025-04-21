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


// Function to sign-extend a value
int32_t sign_extend(uint32_t value, int bits) {
    int32_t mask = 1 << (bits - 1);
    return (value ^ mask) - mask;
}

using namespace std;
string to_hex(uint32_t val) {
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << uppercase << val;
    return ss.str();
}

// Configuration knobs
struct Knobs {
    bool enable_pipelining = true;
    bool enable_data_forwarding = true;
    bool print_reg_file = false;
    bool print_pipeline_regs = false;
    int trace_instruction = -1;
    bool print_branch_predictor = false;
    bool enable_structural_hazard = false;
} knobs;

// Statistics
struct Stats {
    int total_cycles = 0;
    int total_instructions = 0;
    int data_transfer_instructions = 0;
    int alu_instructions = 0;
    int control_instructions = 0;
    int stall_count = 0;
    int data_hazards = 0;
    int control_hazards = 0;
    int branch_mispredictions = 0;
    int stalls_data_hazards = 0;
    int stalls_control_hazards = 0;

    double get_cpi() const {
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
    int instr_number = 0;
};

struct ID_EX_Register {
    uint32_t pc = 0;
    uint32_t ir = 0;
    int32_t reg_a_val = 0;
    int32_t reg_b_val = 0;
    int32_t imm = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;
};

struct EX_MEM_Register {
    uint32_t pc = 0;
    int32_t alu_result = 0;
    int32_t rs2_val = 0;
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;
};

struct MEM_WB_Register {
    uint32_t pc = 0;
    int32_t write_data = 0;
    uint32_t rd = 0;
    Control ctrl;
    bool is_valid = false;
    int instr_number = 0;
};

// Global simulation state
uint32_t pc = 0;
array<int32_t, 32> reg_file = {0};
vector<pair<uint32_t, uint32_t>> text_memory;
vector<pair<uint32_t, int32_t>> data_memory;
const int MAX_CYCLES = 10000;
int instruction_count = 0;
bool program_done = false;

// Pipeline registers
IF_ID_Register if_id;
ID_EX_Register id_ex;
EX_MEM_Register ex_mem;
MEM_WB_Register mem_wb;

// Hazard/stall controls
bool stall_pipeline = false;
class BranchPredictor {
private:
    struct BTBEntry {
        uint32_t pc;
        uint32_t target;
        bool valid;
        bool prediction; // 1-bit predictor: 0 (not taken), 1 (taken)
    };
    vector<BTBEntry> btb; // No fixed size, grows dynamically

public:
    BranchPredictor(uint32_t btb_s) {
        btb.clear(); // Initialize empty
        cout << "Branch Predictor initialized with dynamic size\n";
    }

    bool predict(uint32_t pc) {
        for (const auto& entry : btb) {
            if (entry.valid && entry.pc == pc) {
                bool predicted_taken = entry.prediction;
                cout << "Predict: PC=" << to_hex(pc) << ", BTB hit, Prediction=" << predicted_taken << "\n";
                return predicted_taken;
            }
        }
        cout << "Predict: PC=" << to_hex(pc) << ", No BTB entry, predict not taken\n";
        return false; // Default: predict not taken
    }

    uint32_t get_target(uint32_t pc) {
        for (const auto& entry : btb) {
            if (entry.valid && entry.pc == pc) {
                return entry.target;
            }
        }
        return pc + 4; // Default: next instruction
    }

    void update(uint32_t pc, bool taken, uint32_t target) {
        // Check if entry exists
        for (auto& entry : btb) {
            if (entry.valid && entry.pc == pc) {
                // Skip if no change
                if (entry.target == target && entry.prediction == taken) {
                    cout << "BTB Update: PC=" << to_hex(pc) << ", No change (Target=" << to_hex(target)
                         << ", Prediction=" << taken << ")\n";
                    return;
                }
                // Update existing entry
                entry.target = target;
                entry.prediction = taken;
                cout << "BTB Update: PC=" << to_hex(pc) << ", Taken=" << taken
                     << ", Target=" << to_hex(target) << ", Prediction=" << taken << "\n";
                return;
            }
        }
        // Add new entry
        btb.push_back({pc, target, true, taken});
        cout << "BTB Update: New entry PC=" << to_hex(pc) << ", Taken=" << taken
             << ", Target=" << to_hex(target) << ", Prediction=" << taken << "\n";
    }

    void print_state() {
        if (!knobs.print_branch_predictor) return;
        cout << "Branch Predictor State:\n";
        if (btb.empty()) {
            cout << "BTB is empty\n";
            return;
        }
        for (size_t i = 0; i < btb.size(); ++i) {
            if (btb[i].valid) {
                cout << "BTB[" << i << "]: PC=" << to_hex(btb[i].pc)
                     << ", Target=" << to_hex(btb[i].target)
                     << ", Prediction=" << btb[i].prediction << "\n";
            }
        }
    }
};

BranchPredictor* branch_predictor = nullptr;

bool is_valid_hex(const string& str) {
    if (str.empty() || str.length() < 3 || str.substr(0, 2) != "0x") return false;
    for (size_t i = 2; i < str.length(); ++i) {
        if (!isxdigit(str[i])) return false;
    }
    return true;
}

bool load_memory(const string& text_file, const string& data_file) {
    // Load text segment
    ifstream text_in(text_file);
    if (!text_in) {
        cerr << "Error: Cannot open text file: " << text_file << endl;
        return false;
    }

    string line;
    int line_num = 0;
    int valid_lines = 0;
    while (getline(text_in, line)) {
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
            cout << "Warning: Skipping malformed line " << line_num << " in " << text_file << ": " << line << endl;
            continue;
        }

        if (!is_valid_hex(addr_str) || !is_valid_hex(instr_str)) {
            cout << "Warning: Invalid hex at line " << line_num << " in " << text_file
                 << ": " << addr_str << " " << instr_str << endl;
            continue;
        }

        try {
            uint32_t addr = stoul(addr_str, nullptr, 16);
            uint32_t instr = stoul(instr_str, nullptr, 16);
            text_memory.emplace_back(addr, instr);
            ++valid_lines;
            cout << "Loaded text: Addr=" << to_hex(addr) << ", Instr=" << to_hex(instr) << "\n";
        } catch (const exception& e) {
            cout << "Warning: Invalid number format at line " << line_num << " in " << text_file
                 << ": " << addr_str << " " << instr_str << endl;
            continue;
        }
    }
    text_in.close();

    if (valid_lines == 0) {
        cerr << "Error: No valid instructions loaded from " << text_file << endl;
        return false;
    }
    cout << "Total valid text instructions loaded: " << valid_lines << "\n";

    // Load data segment if provided
    ifstream data_in(data_file);
    if (data_in) {
        line_num = 0;
        valid_lines = 0;
        while (getline(data_in, line)) {
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
                cout << "Warning: Skipping malformed line " << line_num << " in " << data_file
                     << ": " << line << endl;
                continue;
            }

            if (!is_valid_hex(addr_str) || !is_valid_hex(value_str)) {
                cout << "Warning: Invalid hex at line " << line_num << " in " << data_file
                     << ": " << addr_str << " " << value_str << endl;
                continue;
            }

            try {
                uint32_t addr = stoul(addr_str, nullptr, 16);
                int32_t value = static_cast<int32_t>(stoul(value_str, nullptr, 16));
                data_memory.emplace_back(addr, value);
                ++valid_lines;
                cout << "Loaded data: Addr=" << to_hex(addr) << ", Value=" << to_hex(value) << "\n";
            } catch (const exception& e) {
                cout << "Warning: Invalid number format at line " << line_num << " in " << data_file
                     << ": " << addr_str << " " << value_str << endl;
                continue;
            }
        }
        data_in.close();
        cout << "Total valid data entries loaded: " << valid_lines << "\n";
    } else {
        cout << "No data file found or could not open: " << data_file << endl;
    }

    return true;
}

void write_data_mc(const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Cannot write to " << filename << endl;
        return;
    }

    for (const auto& mem_pair : data_memory) {
        file << to_hex(mem_pair.first) << " " << to_hex(mem_pair.second) << "\n";
    }
    file.close();
}
bool detect_data_hazard() {
    if (!knobs.enable_pipelining || !if_id.is_valid) return false;

    uint32_t ir = if_id.ir;
    if (ir == 0) return false;

    uint32_t opcode = ir & 0x7F;
    uint32_t rs1 = (ir >> 15) & 0x1F;
    uint32_t rs2 = (ir >> 20) & 0x1F;

    bool uses_rs1 = (opcode == stoul(R_opcode_map["add"], nullptr, 2) ||
                     opcode == stoul(I_opcode_map["addi"], nullptr, 2) ||
                     opcode == stoul(I_opcode_map["lw"], nullptr, 2) ||
                     opcode == stoul(I_opcode_map["jalr"], nullptr, 2) ||
                     opcode == stoul(S_opcode_map["sw"], nullptr, 2) ||
                     opcode == stoul(SB_opcode_map["beq"], nullptr, 2));

    bool uses_rs2 = (opcode == stoul(R_opcode_map["add"], nullptr, 2) ||
                     opcode == stoul(S_opcode_map["sw"], nullptr, 2) ||
                     opcode == stoul(SB_opcode_map["beq"], nullptr, 2));

    // Check hazard with ID/EX
    if (id_ex.is_valid && id_ex.ctrl.reg_write && id_ex.rd != 0) {
        if ((uses_rs1 && rs1 == id_ex.rd) || (uses_rs2 && rs2 == id_ex.rd)) {
            if (!knobs.enable_data_forwarding || id_ex.ctrl.mem_read) {
                stats.data_hazards++;
                stats.stalls_data_hazards++;
                return true;
            }
        }
    }

    // Check hazard with EX/MEM
    if (!knobs.enable_data_forwarding && ex_mem.is_valid && ex_mem.ctrl.reg_write && ex_mem.rd != 0) {
        if ((uses_rs1 && rs1 == ex_mem.rd) || (uses_rs2 && rs2 == ex_mem.rd)) {
            stats.data_hazards++;
            stats.stalls_data_hazards++;
            return true;
        }
    }

    // Check hazard with MEM/WB (skipped if structural hazard handling is enabled)
    if (!knobs.enable_data_forwarding && !knobs.enable_structural_hazard &&
        mem_wb.is_valid && mem_wb.ctrl.reg_write && mem_wb.rd != 0) {
        if ((uses_rs1 && rs1 == mem_wb.rd) || (uses_rs2 && rs2 == mem_wb.rd)) {
            stats.data_hazards++;
            stats.stalls_data_hazards++;
            return true;
        }
    }

    return false;
}
void determine_forwarding(uint32_t& reg_a_val, uint32_t& reg_b_val, uint32_t rs1, uint32_t rs2) {
    if (!knobs.enable_data_forwarding) return;

    // Forward from EX/MEM
    if (ex_mem.is_valid && ex_mem.ctrl.reg_write && ex_mem.rd != 0) {
        if (ex_mem.rd == rs1) {
            reg_a_val = ex_mem.alu_result;
            cout << "Forwarding EX/MEM to rs1 (x" << rs1 << "): " << reg_a_val << "\n";
        }
        if (ex_mem.rd == rs2) {
            reg_b_val = ex_mem.alu_result;
            cout << "Forwarding EX/MEM to rs2 (x" << rs2 << "): " << reg_b_val << "\n";
        }
    }

    // Forward from MEM/WB
    if (mem_wb.is_valid && mem_wb.ctrl.reg_write && mem_wb.rd != 0) {
        if (mem_wb.rd == rs1 && !(ex_mem.is_valid && ex_mem.ctrl.reg_write && ex_mem.rd == rs1)) {
            reg_a_val = mem_wb.write_data;
            cout << "Forwarding MEM/WB to rs1 (x" << rs1 << "): " << reg_a_val << "\n";
        }
        if (mem_wb.rd == rs2 && !(ex_mem.is_valid && ex_mem.ctrl.reg_write && ex_mem.rd == rs2)) {
            reg_b_val = mem_wb.write_data;
            cout << "Forwarding MEM/WB to rs2 (x" << rs2 << "): " << reg_b_val << "\n";
        }
    }
}
void fetch() {
    if (program_done) {
        if (!if_id.is_valid && !id_ex.is_valid && !ex_mem.is_valid && !mem_wb.is_valid) {
            cout << "Fetch: Pipeline empty and program done\n";
            return;
        }
    }

    if (stall_pipeline && knobs.enable_pipelining) {
        stats.stall_count++;
        cout << "Fetch: Stalled, keeping IF/ID unchanged\n";
        return; // Keep if_id as is, do not advance PC
    }

    uint32_t ir = 0;
    bool found = false;
    for (const auto& instr : text_memory) {
        if (instr.first == pc) {
            ir = instr.second;
            found = true;
            break;
        }
    }

    if (!found || ir == 0) {
        cout << "Fetch: No instruction at PC=" << to_hex(pc) << ", marking IF/ID invalid\n";
        if_id = IF_ID_Register();
        pc += 4;
        if (!if_id.is_valid && !id_ex.is_valid && !ex_mem.is_valid && !mem_wb.is_valid) {
            cout << "Fetch: Pipeline empty and no more instructions, setting program_done\n";
            program_done = true;
        }
        return;
    }

    if_id.pc = pc;
    if_id.ir = ir;
    if_id.is_valid = true;
    if_id.instr_number = ++instruction_count;

    uint32_t next_pc = pc + 4;
    if (knobs.enable_pipelining) {
        bool predicted_taken = branch_predictor->predict(pc);
        if (predicted_taken) {
            next_pc = branch_predictor->get_target(pc);
        }
    } // In non-pipelined mode, always assume next_pc = pc + 4

    pc = next_pc;
    cout << "Fetch: PC=" << to_hex(if_id.pc) << ", IR=" << to_hex(ir) << ", Instr#=" << instruction_count
         << ", NextPC=" << to_hex(pc) << "\n";
}
int32_t extract_immediate(uint32_t ir, char type) {
    switch (type) {
        case 'I': return sign_extend((ir >> 20) & 0xFFF, 12); // imm[11:0]
        case 'S': {
            int32_t imm_s = ((ir >> 25) << 5) | ((ir >> 7) & 0x1F);
            return sign_extend(imm_s, 12);
        }
        case 'B': {
            int32_t imm_b = ((ir >> 31) << 12) | (((ir >> 7) & 0x1) << 11) |
                            (((ir >> 25) & 0x3F) << 5) | (((ir >> 8) & 0xF) << 1);
            return sign_extend(imm_b, 13);
        }
        case 'U': return ir & 0xFFFFF000; // imm[31:12] << 12
        case 'J': {
            int32_t imm_j = ((ir >> 31) << 20) | // imm[20]
                            (((ir >> 21) & 0x3FF) << 1) | // imm[10:1]
                            (((ir >> 20) & 0x1) << 11) | // imm[11]
                            (((ir >> 12) & 0xFF) << 12); // imm[19:12]
            return sign_extend(imm_j, 21);
        }
        default: return 0;
    }
}void decode() {
    if (!if_id.is_valid) {
        id_ex = ID_EX_Register();
        cout << "Decode: IF/ID invalid, inserting bubble\n";
        return;
    }

    if (stall_pipeline) {
        cout << "Decode: Pipeline stalled, keeping ID/EX unchanged\n";
        return; // Keep id_ex as is
    }

    // ... existing decode logic ...
    uint32_t ir = if_id.ir;
    id_ex.ir = ir;
    id_ex.pc = if_id.pc;
    id_ex.instr_number = if_id.instr_number;
    id_ex.is_valid = true;

    Control& ctrl = id_ex.ctrl;
    ctrl = Control();

    if (ir == 0) {
        ctrl.is_nop = true;
        cout << "Decode: Invalid instruction (IR=0)\n";
        return;
    }

    uint32_t opcode = ir & 0x7F;
    uint32_t rd = (ir >> 7) & 0x1F;
    uint32_t rs1 = (ir >> 15) & 0x1F;
    uint32_t rs2 = (ir >> 20) & 0x1F;
    uint32_t func3 = (ir >> 12) & 0x7;
    uint32_t func7 = (ir >> 25) & 0x7F;
    int32_t imm = 0;

    int32_t reg_a_val = reg_file[rs1];
    int32_t reg_b_val = reg_file[rs2];

    determine_forwarding(reinterpret_cast<uint32_t&>(reg_a_val), reinterpret_cast<uint32_t&>(reg_b_val), rs1, rs2);

    id_ex.rs1 = rs1;
    id_ex.rs2 = rs2;
    id_ex.rd = rd;
    id_ex.reg_a_val = reg_a_val;
    id_ex.reg_b_val = reg_b_val;

    cout << "Decode: PC=" << to_hex(if_id.pc) << ", IR=" << to_hex(ir) << ", rs1=x" << rs1
         << "(" << reg_a_val << "), rs2=x" << rs2 << "(" << reg_b_val << "), rd=x" << rd << "\n";

    bool is_control = false;
    bool branch_taken = false;
    uint32_t branch_target = if_id.pc + 4;

    if (opcode == stoul(R_opcode_map["add"], nullptr, 2)) {
        ctrl.reg_write = true;
        stats.alu_instructions++;
        if (func3 == stoul(func3_map["add"], nullptr, 2) && func7 == stoul(func7_map["add"], nullptr, 2)) {
            ctrl.alu_op = "ADD";
        } else if (func3 == stoul(func3_map["sub"], nullptr, 2) && func7 == stoul(func7_map["sub"], nullptr, 2)) {
            ctrl.alu_op = "SUB";
        } else if (func3 == stoul(func3_map["mul"], nullptr, 2) && func7 == stoul(func7_map["mul"], nullptr, 2)) {
            ctrl.alu_op = "MUL";
        } else if (func3 == stoul(func3_map["sll"], nullptr, 2) && func7 == stoul(func7_map["sll"], nullptr, 2)) {
            ctrl.alu_op = "SLL";
        } else if (func3 == stoul(func3_map["slt"], nullptr, 2) && func7 == stoul(func7_map["slt"], nullptr, 2)) {
            ctrl.alu_op = "SLT";
        } else if (func3 == stoul(func3_map["sltu"], nullptr, 2) && func7 == stoul(func7_map["sltu"], nullptr, 2)) {
            ctrl.alu_op = "SLTU";
        } else if (func3 == stoul(func3_map["xor"], nullptr, 2) && func7 == stoul(func7_map["xor"], nullptr, 2)) {
            ctrl.alu_op = "XOR";
        } else if (func3 == stoul(func3_map["srl"], nullptr, 2) && func7 == stoul(func7_map["srl"], nullptr, 2)) {
            ctrl.alu_op = "SRL";
        } else if (func3 == stoul(func3_map["sra"], nullptr, 2) && func7 == stoul(func7_map["sra"], nullptr, 2)) {
            ctrl.alu_op = "SRA";
        } else if (func3 == stoul(func3_map["or"], nullptr, 2) && func7 == stoul(func7_map["or"], nullptr, 2)) {
            ctrl.alu_op = "OR";
        } else if (func3 == stoul(func3_map["and"], nullptr, 2) && func7 == stoul(func7_map["and"], nullptr, 2)) {
            ctrl.alu_op = "AND";
        } else {
            ctrl.is_nop = true;
            cout << "Decode: Unknown R-type instruction, func3=0x" << hex << func3 << ", func7=0x" << func7 << dec << "\n";
        }
    } else if (opcode == stoul(I_opcode_map["addi"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.use_imm = true;
        imm = sign_extend((ir >> 20) & 0xFFF, 12);
        stats.alu_instructions++;
        if (func3 == stoul(func3_map["addi"], nullptr, 2)) {
            ctrl.alu_op = "ADDI";
        } else if (func3 == stoul(func3_map["slti"], nullptr, 2)) {
            ctrl.alu_op = "SLTI";
        } else if (func3 == stoul(func3_map["sltiu"], nullptr, 2)) {
            ctrl.alu_op = "SLTIU";
        } else if (func3 == stoul(func3_map["xori"], nullptr, 2)) {
            ctrl.alu_op = "XORI";
        } else if (func3 == stoul(func3_map["ori"], nullptr, 2)) {
            ctrl.alu_op = "ORI";
        } else if (func3 == stoul(func3_map["andi"], nullptr, 2)) {
            ctrl.alu_op = "ANDI";
        } else if (func3 == stoul(func3_map["slli"], nullptr, 2) && func7 == stoul(func7_map["slli"], nullptr, 2)) {
            ctrl.alu_op = "SLLI";
            imm = (ir >> 20) & 0x1F;
        } else if (func3 == stoul(func3_map["srli"], nullptr, 2) && func7 == stoul(func7_map["srli"], nullptr, 2)) {
            ctrl.alu_op = "SRLI";
            imm = (ir >> 20) & 0x1F;
        } else if (func3 == stoul(func3_map["srai"], nullptr, 2) && func7 == stoul(func7_map["srai"], nullptr, 2)) {
            ctrl.alu_op = "SRAI";
            imm = (ir >> 20) & 0x1F;
        } else {
            ctrl.is_nop = true;
            cout << "Decode: Unknown I-type arithmetic instruction, func3=0x" << hex << func3 << ", func7=0x" << func7 << dec << "\n";
        }
    } else if (opcode == stoul(I_opcode_map["lw"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.mem_read = true;
        ctrl.use_imm = true;
        ctrl.output_sel = 1;
        imm = sign_extend((ir >> 20) & 0xFFF, 12);
        stats.data_transfer_instructions++;
        if (func3 == stoul(func3_map["lb"], nullptr, 2)) {
            ctrl.alu_op = "LB";
            ctrl.mem_size = "BYTE";
        } else if (func3 == stoul(func3_map["lh"], nullptr, 2)) {
            ctrl.alu_op = "LH";
            ctrl.mem_size = "HALF";
        } else if (func3 == stoul(func3_map["lw"], nullptr, 2)) {
            ctrl.alu_op = "LW";
            ctrl.mem_size = "WORD";
        } else if (func3 == stoul(func3_map["lbu"], nullptr, 2)) {
            ctrl.alu_op = "LBU";
            ctrl.mem_size = "BYTE_U";
        } else if (func3 == stoul(func3_map["lhu"], nullptr, 2)) {
            ctrl.alu_op = "LHU";
            ctrl.mem_size = "HALF_U";
        } else {
            ctrl.is_nop = true;
            cout << "Decode: Unknown load instruction, func3=0x" << hex << func3 << dec << "\n";
        }
    } else if (opcode == stoul(S_opcode_map["sw"], nullptr, 2)) {
        ctrl.mem_write = true;
        ctrl.reg_write = false;
        imm = extract_immediate(ir, 'S');
        stats.data_transfer_instructions++;
        if (func3 == stoul(func3_map["sb"], nullptr, 2)) {
            ctrl.alu_op = "SB";
            ctrl.mem_size = "BYTE";
        } else if (func3 == stoul(func3_map["sh"], nullptr, 2)) {
            ctrl.alu_op = "SH";
            ctrl.mem_size = "HALF";
        } else if (func3 == stoul(func3_map["sw"], nullptr, 2)) {
            ctrl.alu_op = "SW";
            ctrl.mem_size = "WORD";
        } else {
            ctrl.is_nop = true;
            cout << "Decode: Unknown store instruction, func3=0x" << hex << func3 << dec << "\n";
        }
    } else if (opcode == stoul(SB_opcode_map["beq"], nullptr, 2)) {
        ctrl.branch = true;
        ctrl.reg_write = false;
        id_ex.rd = 0;
        is_control = true;
        stats.control_instructions++;
        imm = extract_immediate(ir, 'B');
        if (func3 == stoul(func3_map["beq"], nullptr, 2)) {
            ctrl.alu_op = "BEQ";
            branch_taken = (reg_a_val == reg_b_val);
        } else if (func3 == stoul(func3_map["bne"], nullptr, 2)) {
            ctrl.alu_op = "BNE";
            branch_taken = (reg_a_val != reg_b_val);
        } else if (func3 == stoul(func3_map["blt"], nullptr, 2)) {
            ctrl.alu_op = "BLT";
            branch_taken = (static_cast<int32_t>(reg_a_val) < static_cast<int32_t>(reg_b_val));
        } else if (func3 == stoul(func3_map["bge"], nullptr, 2)) {
            ctrl.alu_op = "BGE";
            branch_taken = (static_cast<int32_t>(reg_a_val) >= static_cast<int32_t>(reg_b_val));
        } else if (func3 == stoul(func3_map["bltu"], nullptr, 2)) {
            ctrl.alu_op = "BLTU";
            branch_taken = (static_cast<uint32_t>(reg_a_val) < static_cast<uint32_t>(reg_b_val));
        } else if (func3 == stoul(func3_map["bgeu"], nullptr, 2)) {
            ctrl.alu_op = "BGEU";
            branch_taken = (static_cast<uint32_t>(reg_a_val) >= static_cast<uint32_t>(reg_b_val));
        } else {
            ctrl.is_nop = true;
            cout << "Decode: Unknown SB-type instruction, func3=0x" << hex << func3 << dec << "\n";
            return;
        }
        branch_target = branch_taken ? if_id.pc + imm : if_id.pc + 4;
        cout << "Decode " << ctrl.alu_op << ": rs1=x" << rs1 << "(" << reg_a_val << "), rs2=x" << rs2
             << "(" << reg_b_val << "), Taken=" << branch_taken << ", Target=" << to_hex(branch_target) << "\n";
    } else if (opcode == stoul(UJ_opcode_map["jal"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.alu_op = "JAL";
        ctrl.output_sel = 2;
        is_control = true;
        stats.control_instructions++;
        imm = extract_immediate(ir, 'J');
        branch_taken = true;
        branch_target = if_id.pc + imm;
        cout << "Decode JAL: rd=x" << rd << ", Target=" << to_hex(branch_target) << "\n";
    } else if (opcode == stoul(I_opcode_map["jalr"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.alu_op = "JALR";
        ctrl.use_imm = true;
        ctrl.output_sel = 2;
        is_control = true;
        stats.control_instructions++;
        imm = sign_extend((ir >> 20) & 0xFFF, 12);
        branch_taken = true;
        branch_target = (reg_a_val + imm) & ~1;
        cout << "Decode JALR: rs1=x" << rs1 << "(" << reg_a_val << "), imm=" << imm
             << ", Target=" << to_hex(branch_target) << "\n";
    } else if (opcode == stoul(U_opcode_map["lui"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.alu_op = "LUI";
        imm = extract_immediate(ir, 'U');
        stats.alu_instructions++;
        cout << "Decode LUI: rd=x" << rd << ", imm=" << to_hex(imm) << "\n";
    } else if (opcode == stoul(U_opcode_map["auipc"], nullptr, 2)) {
        ctrl.reg_write = true;
        ctrl.alu_op = "AUIPC";
        imm = extract_immediate(ir, 'U');
        stats.alu_instructions++;
        cout << "Decode AUIPC: rd=x" << rd << ", imm=" << to_hex(imm) << "\n";
    } else {
        ctrl.is_nop = true;
        cout << "Decode: Unknown opcode: 0x" << hex << opcode << dec << "\n";
    }

    id_ex.imm = imm;

    if (is_control) {
        uint32_t current_pc = if_id.pc;
        if(knobs.enable_pipelining){
        bool predicted_taken = branch_predictor->predict(if_id.pc);
        uint32_t predicted_target = predicted_taken ? branch_predictor->get_target(if_id.pc) : if_id.pc + 4;

        if (branch_taken != predicted_taken || (branch_taken && branch_target != predicted_target)) {
            cout << "Misprediction: Flushing pipeline, ActualTarget=" << to_hex(branch_target)
                 << ", PredictedTarget=" << to_hex(predicted_target) << "\n";
            pc = branch_target;
            if_id = IF_ID_Register();
            stall_pipeline = true;
            stats.branch_mispredictions++;
            stats.control_hazards++;
            stats.stalls_control_hazards++;
        } else {
            cout << "Prediction correct: Taken=" << branch_taken << ", Target=" << to_hex(branch_target) << "\n";
        }

        branch_predictor->update(current_pc, branch_taken, branch_target);
    }
    else{
        // Non pipelined mode
        pc = branch_target;
        if_id = IF_ID_Register();
        branch_predictor->update(current_pc , branch_taken , branch_target);
        cout << "NON pipelined : Setting PC to " << to_hex(branch_target) << ", Taken" << branch_taken << endl;
    }
}}void execute() {
    if (!id_ex.is_valid) {
        ex_mem = EX_MEM_Register();
        cout << "Execute: ID/EX invalid, skipping\n";
        return;
    }

    cout << "Execute: PC=" << to_hex(id_ex.pc) << ", IR=" << to_hex(id_ex.ir) << ", ALU=" << id_ex.ctrl.alu_op << "\n";

    int32_t reg_a_val = id_ex.reg_a_val;
    int32_t reg_b_val = id_ex.reg_b_val;
    determine_forwarding(reinterpret_cast<uint32_t&>(reg_a_val), reinterpret_cast<uint32_t&>(reg_b_val), id_ex.rs1, id_ex.rs2);

    int32_t alu_result = 0;

    if (id_ex.ctrl.is_nop) {
        cout << "Execute: NOP\n";
        goto alu_done;
    }

    if (id_ex.ctrl.alu_op == "ADD") {
        alu_result = reg_a_val + reg_b_val;
    } else if (id_ex.ctrl.alu_op == "SUB") {
        alu_result = reg_a_val - reg_b_val;
    } else if (id_ex.ctrl.alu_op == "MUL") {
        alu_result = reg_a_val * reg_b_val;
    } else if (id_ex.ctrl.alu_op == "SLL") {
        alu_result = reg_a_val << (reg_b_val & 0x1F);
    } else if (id_ex.ctrl.alu_op == "SLT") {
        alu_result = (reg_a_val < reg_b_val) ? 1 : 0;
    } else if (id_ex.ctrl.alu_op == "SLTU") {
        alu_result = (static_cast<uint32_t>(reg_a_val) < static_cast<uint32_t>(reg_b_val)) ? 1 : 0;
    } else if (id_ex.ctrl.alu_op == "XOR") {
        alu_result = reg_a_val ^ reg_b_val;
    } else if (id_ex.ctrl.alu_op == "SRL") {
        alu_result = static_cast<uint32_t>(reg_a_val) >> (reg_b_val & 0x1F);
    } else if (id_ex.ctrl.alu_op == "SRA") {
        alu_result = reg_a_val >> (reg_b_val & 0x1F);
    } else if (id_ex.ctrl.alu_op == "OR") {
        alu_result = reg_a_val | reg_b_val;
    } else if (id_ex.ctrl.alu_op == "AND") {
        alu_result = reg_a_val & reg_b_val;
    } else if (id_ex.ctrl.alu_op == "ADDI" || id_ex.ctrl.alu_op == "LW" || id_ex.ctrl.alu_op == "LH" ||
               id_ex.ctrl.alu_op == "LB" || id_ex.ctrl.alu_op == "LHU" || id_ex.ctrl.alu_op == "LBU" ||
               id_ex.ctrl.alu_op == "SW" || id_ex.ctrl.alu_op == "SH" || id_ex.ctrl.alu_op == "SB" ||
               id_ex.ctrl.alu_op == "JALR") {
        alu_result = reg_a_val + id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "SLTI") {
        alu_result = (reg_a_val < id_ex.imm) ? 1 : 0;
    } else if (id_ex.ctrl.alu_op == "SLTIU") {
        alu_result = (static_cast<uint32_t>(reg_a_val) < static_cast<uint32_t>(id_ex.imm)) ? 1 : 0;
    } else if (id_ex.ctrl.alu_op == "XORI") {
        alu_result = reg_a_val ^ id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "ORI") {
        alu_result = reg_a_val | id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "ANDI") {
        alu_result = reg_a_val & id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "SLLI") {
        alu_result = reg_a_val << (id_ex.imm & 0x1F);
    } else if (id_ex.ctrl.alu_op == "SRLI") {
        alu_result = static_cast<uint32_t>(reg_a_val) >> (id_ex.imm & 0x1F);
    } else if (id_ex.ctrl.alu_op == "SRAI") {
        alu_result = reg_a_val >> (id_ex.imm & 0x1F);
    } else if (id_ex.ctrl.alu_op == "BEQ" || id_ex.ctrl.alu_op == "BNE" || id_ex.ctrl.alu_op == "BLT" ||
               id_ex.ctrl.alu_op == "BGE" || id_ex.ctrl.alu_op == "BLTU" || id_ex.ctrl.alu_op == "BGEU") {
        alu_result = id_ex.pc + id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "JAL" || id_ex.ctrl.alu_op == "JALR") {
        alu_result = id_ex.pc + 4;
    } else if (id_ex.ctrl.alu_op == "LUI") {
        alu_result = id_ex.imm;
    } else if (id_ex.ctrl.alu_op == "AUIPC") {
        alu_result = id_ex.pc + id_ex.imm;
    } else {
        cout << "Execute: Unknown ALU op: " << id_ex.ctrl.alu_op << "\n";
    }
    alu_done:
    ex_mem.pc = id_ex.pc;
    ex_mem.alu_result = alu_result;
    ex_mem.rs2_val = reg_b_val;
    ex_mem.rd = id_ex.rd;
    ex_mem.ctrl = id_ex.ctrl;
    ex_mem.is_valid = true;
    ex_mem.instr_number = id_ex.instr_number;

    id_ex = ID_EX_Register();
}
void memory() {
    if (!ex_mem.is_valid) {
        mem_wb = MEM_WB_Register();
        cout << "Memory: EX/MEM invalid, skipping\n";
        return;
    }

    int32_t mem_result = ex_mem.alu_result;

    if (ex_mem.ctrl.mem_read) {
        uint32_t addr = ex_mem.alu_result;
        // Alignment checks
        if (ex_mem.ctrl.mem_size == "WORD" && (addr % 4 != 0)) {
            cout << "Warning: Misaligned WORD read at address " << to_hex(addr) << "\n";
        } else if (ex_mem.ctrl.mem_size == "HALF" || ex_mem.ctrl.mem_size == "HALF_U") {
            if (addr % 2 != 0) {
                cout << "Warning: Misaligned HALF read at address " << to_hex(addr) << "\n";
            }
        }
        bool found = false;
        for (const auto& mem_entry : data_memory) {
            if (mem_entry.first == addr) {
                mem_result = mem_entry.second;
                found = true;
                if (ex_mem.ctrl.mem_size == "BYTE") {
                    mem_result = sign_extend(mem_result & 0xFF, 8);
                } else if (ex_mem.ctrl.mem_size == "HALF") {
                    mem_result = sign_extend(mem_result & 0xFFFF, 16);
                } else if (ex_mem.ctrl.mem_size == "BYTE_U") {
                    mem_result = mem_result & 0xFF;
                } else if (ex_mem.ctrl.mem_size == "HALF_U") {
                    mem_result = mem_result & 0xFFFF;
                }
                break;
            }
        }
        if (!found) {
            cout << "Warning: Memory read at address " << to_hex(addr) << " found no data, returning 0\n";
            mem_result = 0;
        }
    } else if (ex_mem.ctrl.mem_write) {
        uint32_t addr = ex_mem.alu_result;
        int32_t value = ex_mem.rs2_val;
        // Alignment checks
        if (ex_mem.ctrl.mem_size == "WORD" && (addr % 4 != 0)) {
            cout << "Warning: Misaligned WORD write at address " << to_hex(addr) << "\n";
        } else if (ex_mem.ctrl.mem_size == "HALF" && (addr % 2 != 0)) {
            cout << "Warning: Misaligned HALF write at address " << to_hex(addr) << "\n";
        }
        bool found = false;
        for (auto& mem_entry : data_memory) {
            if (mem_entry.first == addr) {
                if (ex_mem.ctrl.mem_size == "BYTE") {
                    mem_entry.second = (mem_entry.second & ~0xFF) | (value & 0xFF);
                } else if (ex_mem.ctrl.mem_size == "HALF") {
                    mem_entry.second = (mem_entry.second & ~0xFFFF) | (value & 0xFFFF);
                } else {
                    mem_entry.second = value;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            if (ex_mem.ctrl.mem_size == "BYTE") {
                data_memory.emplace_back(addr, value & 0xFF);
            } else if (ex_mem.ctrl.mem_size == "HALF") {
                data_memory.emplace_back(addr, value & 0xFFFF);
            } else {
                data_memory.emplace_back(addr, value);
            }
        }
    }

    mem_wb.pc = ex_mem.pc;
    mem_wb.write_data = ex_mem.ctrl.output_sel == 1 ? mem_result : ex_mem.alu_result;
    mem_wb.rd = ex_mem.rd;
    mem_wb.ctrl = ex_mem.ctrl;
    mem_wb.is_valid = true;
    mem_wb.instr_number = ex_mem.instr_number;

    ex_mem = EX_MEM_Register();
    cout << "Memory: PC=" << to_hex(mem_wb.pc) << ", Instr#=" << mem_wb.instr_number << "\n";
}
void writeback() {
    if (!mem_wb.is_valid) {
        cout << "Writeback: MEM/WB invalid, skipping\n";
        return;
    }

    if (!mem_wb.ctrl.is_nop) {
        stats.total_instructions++; // Count only non-NOP instructions
        cout << "Total Instructions are :  " << stats.total_instructions << endl;
    }

    if (mem_wb.ctrl.reg_write && mem_wb.rd != 0) {
        if (mem_wb.ctrl.output_sel == 2) {
            reg_file[mem_wb.rd] = mem_wb.pc + 4;
        } else {
            reg_file[mem_wb.rd] = mem_wb.write_data;
        }
        cout << "Writeback: x" << mem_wb.rd << " = " << to_hex(mem_wb.write_data) << "\n";
    }

    cout << "Writeback: PC=" << to_hex(mem_wb.pc) << ", Instr#=" << mem_wb.instr_number << "\n";
    mem_wb = MEM_WB_Register();
}

void print_register_file() {
    if (!knobs.print_reg_file) return;
    cout << "\nRegister File:\n";
    for (int i = 0; i < 32; i++) {
        if (i % 4 == 0 && i > 0) cout << "\n";
        cout << "x" << setw(2) << setfill('0') << i << ": " << setw(10) << setfill(' ')
             << reg_file[i] << " (0x" << hex << setw(8) << setfill('0') << reg_file[i] << dec << ")  ";
    }
    cout << "\n";
}

void print_pipeline_registers() {
    if (!knobs.print_pipeline_regs) return;
    cout << "\nPipeline Registers:\n";
    cout << "IF/ID: " << (if_id.is_valid ? "PC=" + to_hex(if_id.pc) + ", IR=" + to_hex(if_id.ir) +
                                  ", Instr#=" + to_string(if_id.instr_number) : "INVALID") << "\n";
    cout << "ID/EX: ";
    if (id_ex.is_valid) {
        cout << "PC=" << to_hex(id_ex.pc) << ", rs1=x" << id_ex.rs1 << "(" << id_ex.reg_a_val
             << "), rs2=x" << id_ex.rs2 << "(" << id_ex.reg_b_val << "), rd=x" << id_ex.rd
             << ", imm=" << id_ex.imm << ", ALU=" << id_ex.ctrl.alu_op << ", Instr#=" << id_ex.instr_number;
    } else {
        cout << "INVALID";
    }
    cout << "\n";
    cout << "EX/MEM: ";
    if (ex_mem.is_valid) {
        cout << "PC=" << to_hex(ex_mem.pc) << ", ALU=" << ex_mem.alu_result << ", rs2_val=" << ex_mem.rs2_val
             << ", rd=x" << ex_mem.rd << ", Ctrl=" << (ex_mem.ctrl.mem_read ? "MemRead " : "")
             << (ex_mem.ctrl.mem_write ? "MemWrite " : "") << (ex_mem.ctrl.reg_write ? "RegWrite " : "")
             << "Instr#=" << ex_mem.instr_number;
    } else {
        cout << "INVALID";
    }
    cout << "\n";
    cout << "MEM/WB: ";
    if (mem_wb.is_valid) {
        cout << "PC=" << to_hex(mem_wb.pc) << ", WData=" << mem_wb.write_data << ", rd=x" << mem_wb.rd
             << ", Ctrl=" << (mem_wb.ctrl.reg_write ? "RegWrite " : "") << "Instr#=" << mem_wb.instr_number;
    } else {
        cout << "INVALID";
    }
    cout << "\n";
}

void trace_instruction(int instr_number) {
    if (instr_number == -1) return; // Tracing disabled

    cout << "\nTracing Instruction #" << instr_number << ":\n";

    // IF/ID Stage
    if (if_id.is_valid && if_id.instr_number == instr_number) {
        cout << "IF/ID: PC=" << to_hex(if_id.pc) << ", IR=" << to_hex(if_id.ir)
             << ", Instr#=" << if_id.instr_number << "\n";
    } else if (if_id.instr_number == instr_number) {
        cout << "IF/ID: INVALID (Instruction #" << instr_number << " was here but is now invalid)\n";
    }

    // ID/EX Stage
    if (id_ex.is_valid && id_ex.instr_number == instr_number) {
        cout << "ID/EX: PC=" << to_hex(id_ex.pc) << ", IR=" << to_hex(id_ex.ir)
             << ", rs1=x" << id_ex.rs1 << "(" << id_ex.reg_a_val << ")"
             << ", rs2=x" << id_ex.rs2 << "(" << id_ex.reg_b_val << ")"
             << ", rd=x" << id_ex.rd << ", imm=" << id_ex.imm
             << ", ALU=" << id_ex.ctrl.alu_op
             << ", Ctrl=" << (id_ex.ctrl.reg_write ? "RegWrite " : "")
             << (id_ex.ctrl.mem_read ? "MemRead " : "")
             << (id_ex.ctrl.mem_write ? "MemWrite " : "")
             << (id_ex.ctrl.branch ? "Branch " : "")
             << (id_ex.ctrl.use_imm ? "UseImm " : "")
             << ", Instr#=" << id_ex.instr_number << "\n";
    } else if (id_ex.instr_number == instr_number) {
        cout << "ID/EX: INVALID (Instruction #" << instr_number << " was here but is now invalid)\n";
    }

    // EX/MEM Stage
    if (ex_mem.is_valid && ex_mem.instr_number == instr_number) {
        cout << "EX/MEM: PC=" << to_hex(ex_mem.pc) << ", IR=" << to_hex(id_ex.ir) // Note: IR not stored in EX/MEM, using ID/EX if needed
             << ", ALU Result=" << ex_mem.alu_result << ", rs2_val=" << ex_mem.rs2_val
             << ", rd=x" << ex_mem.rd
             << ", Ctrl=" << (ex_mem.ctrl.reg_write ? "RegWrite " : "")
             << (ex_mem.ctrl.mem_read ? "MemRead " : "")
             << (ex_mem.ctrl.mem_write ? "MemWrite " : "")
             << (ex_mem.ctrl.branch ? "Branch " : "")
             << (ex_mem.ctrl.use_imm ? "UseImm " : "")
             << ", Instr#=" << ex_mem.instr_number << "\n";
    } else if (ex_mem.instr_number == instr_number) {
        cout << "EX/MEM: INVALID (Instruction #" << instr_number << " was here but is now invalid)\n";
    }

    // MEM/WB Stage
    if (mem_wb.is_valid && mem_wb.instr_number == instr_number) {
        cout << "MEM/WB: PC=" << to_hex(mem_wb.pc) << ", IR=" << to_hex(id_ex.ir) // Note: IR not stored in MEM/WB
             << ", Write Data=" << mem_wb.write_data << ", rd=x" << mem_wb.rd
             << ", Ctrl=" << (mem_wb.ctrl.reg_write ? "RegWrite " : "")
             << (mem_wb.ctrl.mem_read ? "MemRead " : "")
             << (mem_wb.ctrl.mem_write ? "MemWrite " : "")
             << (mem_wb.ctrl.branch ? "Branch " : "")
             << (mem_wb.ctrl.use_imm ? "UseImm " : "")
             << ", Instr#=" << mem_wb.instr_number << "\n";
    } else if (mem_wb.instr_number == instr_number) {
        cout << "MEM/WB: INVALID (Instruction #" << instr_number << " was here but is now invalid)\n";
    }
}
void print_statistics() {
    cout << "\nSimulation Statistics:\n";
    cout << "Total Cycles: " << stats.total_cycles << "\n";
    cout << "Total Instructions: " << stats.total_instructions << "\n";
    cout << "CPI: " << fixed << setprecision(3) << stats.get_cpi() << "\n";
    cout << "Data Transfer Instructions: " << stats.data_transfer_instructions << "\n";
    cout << "ALU Instructions: " << stats.alu_instructions << "\n";
    cout << "Control Instructions: " << stats.control_instructions << "\n";
    cout << "Total Stalls/Bubbles: " << stats.stall_count << "\n";
    cout << "Data Hazards: " << stats.data_hazards << "\n";
    cout << "Control Hazards: " << stats.control_hazards << "\n";
    cout << "Branch Mispredictions: " << stats.branch_mispredictions << "\n";
    cout << "Stalls Due to Data Hazards: " << stats.stalls_data_hazards << "\n";
    cout << "Stalls Due to Control Hazards: " << stats.stalls_control_hazards << "\n";
}
void configure_knobs() {
    // Set knob values here (change these to control the simulator)
    knobs.enable_pipelining = true;          // Knob1: Enable pipelining
    knobs.enable_data_forwarding = false;     // Knob2: Enable data forwarding
    knobs.print_reg_file = true;             // Knob3: Print register file each cycle
    knobs.print_pipeline_regs = true;        // Knob4: Print pipeline registers each cycle
    knobs.enable_structural_hazard = false;   

    knobs.trace_instruction = -1;            // Knob5: Trace specific instruction (-1 to disable, e.g., 10 for 10th instruction)
    knobs.print_branch_predictor = true;     // Knob6: Print branch predictor state each cycle

    // Print knob settings for clarity
    cout << "Knob settings:\n";
    cout << "  Pipelining: " << (knobs.enable_pipelining ? "Enabled" : "Disabled") << "\n";
    cout << "  Data Forwarding: " << (knobs.enable_data_forwarding ? "Enabled" : "Disabled") << "\n";
    cout << "  Print Register File: " << (knobs.print_reg_file ? "Enabled" : "Disabled") << "\n";
    cout << "  Print Pipeline Registers: " << (knobs.print_pipeline_regs ? "Enabled" : "Disabled") << "\n";
     cout << "  Structural Hazard Handling: " << (knobs.enable_structural_hazard ? "Enabled" : "Disabled") << "\n";
    cout << "  Trace Instruction: " << (knobs.trace_instruction >= 0 ? to_string(knobs.trace_instruction) : "Disabled") << "\n";
    cout << "  Print Branch Predictor: " << (knobs.print_branch_predictor ? "Enabled" : "Disabled") << "\n";
}
void initialize_simulator() {
    text_memory.clear();
    data_memory.clear();
    reg_file.fill(0);
    reg_file[2] = 0x7FFFFFE4;
    reg_file[3] = 0x10000000;
    reg_file[10] = 0x00000001;
    reg_file[11] = 0x07FFFFE4;

    pc = 0;
    if_id = IF_ID_Register();
    id_ex = ID_EX_Register();
    ex_mem = EX_MEM_Register();
    mem_wb = MEM_WB_Register();

    stall_pipeline = false;

    stats = Stats();
    instruction_count = 0;
    program_done = false;

    // Initialize branch predictor
    if (branch_predictor != nullptr) {
        delete branch_predictor;
    }
    branch_predictor = new BranchPredictor(16); // BTB size = 16
    cout << "Simulator initialized, BTB cleared\n";
}void run_simulation() {
    stats.total_cycles = 0;
    stats.total_instructions = 0;
    program_done = false;
    stall_pipeline = false;

    cout << "Starting simulation...\n";
    cout << "Pipelining: " << (knobs.enable_pipelining ? "Enabled" : "Disabled") << "\n";

    if (knobs.enable_pipelining) {
        while (stats.total_cycles < MAX_CYCLES &&
               (!program_done || if_id.is_valid || id_ex.is_valid || ex_mem.is_valid || mem_wb.is_valid)) {
            cout << "\n=== Cycle " << stats.total_cycles + 1 << " ===\n";

            // Check for data hazards if not already stalling
            if (!stall_pipeline) {
                stall_pipeline = detect_data_hazard();
            }

            writeback();
            memory();
            execute();
            decode();
            fetch();

            // Clear stall if no hazards remain
            if (stall_pipeline && !detect_data_hazard()) {
                stall_pipeline = false;
            }

            stats.total_cycles++;

            print_pipeline_registers();
            //print_register_file();
            trace_instruction(knobs.trace_instruction);
            branch_predictor->print_state();
        }
    } else {
        // Non-pipelined execution
        while (stats.total_cycles < MAX_CYCLES && !program_done) {
            cout << "\n=== Cycle " << stats.total_cycles + 1 << " ===\n";

            fetch();
            if (!if_id.is_valid) {
                stats.total_cycles++;
                continue;
            }

            decode();
            if (!id_ex.is_valid) {
                stats.total_cycles++;
                continue;
            }

            execute();
            if (!ex_mem.is_valid) {
                stats.total_cycles++;
                continue;
            }

            memory();
            if (!mem_wb.is_valid) {
                stats.total_cycles++;
                continue;
            }

            writeback();

            stats.total_cycles++;

            print_pipeline_registers();
            print_register_file();
            trace_instruction(knobs.trace_instruction);
            branch_predictor->print_state();
        }
    }

    print_statistics();
    write_data_mc("data.mc");
}
int main() {
    string text_file = "text.mc";
    string data_file = "data.mc";

    // Configure knobs
    configure_knobs();

    initialize_simulator();

    if (!load_memory(text_file, data_file)) {
        cerr << "Failed to load memory\n";
        return 1;
    }

    run_simulation();

    // Clean up branch predictor
    delete branch_predictor;
    branch_predictor = nullptr;

    return 0;
}

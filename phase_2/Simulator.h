#ifndef SIMULATOR
#define SIMULATOR

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>

#include "memory.h"

using namespace std;

struct I_DATA
{
    string i_type;
    string opperation;
    int rd;
    int rs1;
    int rs2;
    int immi;
};

struct vector_hash
{
    template <typename T>
    size_t operator()(const vector<T> &v) const
    {
        size_t seed = v.size();
        for (const auto &x : v)
        {
            seed ^= hash<T>{}(x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};
class Simulator
{
private:
    /* data */
    int PC = 0x0;
    int EX;
    int limit_pc;
    unordered_map<int, string> PC_MAP;
    // unordered_map<int, int> Memory;
    unordered_map<int, long long int> Register{
        {0, 0},
        {1, 0},
        {2, 2147483612},
        {3, 0},
        {4, 0},
        {5, 0},
        {6, 0},
        {7, 0},
        {8, 0},
        {9, 0},
        {10, 0},
        {11, 0},
        {12, 0},
        {13, 0},
        {14, 0},
        {15, 0},
        {16, 0},
        {17, 0},
        {18, 0},
        {19, 0},
        {20, 0},
        {21, 0},
        {22, 0},
        {23, 0},
        {24, 0},
        {25, 0},
        {26, 0},
        {27, 0},
        {28, 0},
        {29, 0},
        {30, 0},
        {31, 0},
    };
    unordered_map<vector<string>, string, vector_hash> operation = {
        {{"0110011", "000", "0000000"}, "add"},
        {{"0110011", "000", "0100000"}, "sub"},
        {{"0110011", "001", "0000000"}, "sll"},
        {{"0110011", "010", "0000000"}, "slt"},
        {{"0110011", "011", "0000000"}, "sltu"},
        {{"0110011", "100", "0000000"}, "xor"},
        {{"0110011", "101", "0000000"}, "srl"},
        {{"0110011", "101", "0100000"}, "sra"},
        {{"0110011", "110", "0000000"}, "or"},
        {{"0110011", "111", "0000000"}, "and"},

        {{"0010011", "000", "-1"}, "addi"},
        {{"0010011", "010", "-1"}, "slti"},
        {{"0010011", "011", "-1"}, "sltiu"},
        {{"0010011", "100", "-1"}, "xori"},
        {{"0010011", "110", "-1"}, "ori"},
        {{"0010011", "111", "-1"}, "andi"},
        {{"0010011", "001", "0000000"}, "slli"},
        {{"0010011", "101", "0000000"}, "srli"},
        {{"0010011", "101", "0100000"}, "srai"},

        {{"1100011", "000", "-1"}, "beq"},
        {{"1100011", "001", "-1"}, "bne"},
        {{"1100011", "100", "-1"}, "blt"},
        {{"1100011", "101", "-1"}, "bge"},
        {{"1100011", "110", "-1"}, "bltu"},
        {{"1100011", "111", "-1"}, "bgeu"},

        {{"0000011", "000", "-1"}, "lb"},
        {{"0000011", "001", "-1"}, "lh"},
        {{"0000011", "010", "-1"}, "lw"},
        {{"0000011", "100", "-1"}, "lbu"},
        {{"0000011", "101", "-1"}, "lhu"},

        {{"0100011", "000", "-1"}, "sb"},
        {{"0100011", "001", "-1"}, "sh"},
        {{"0100011", "010", "-1"}, "sw"},

        {{"1101111", "-1", "-1"}, "jal"},
        {{"1100111", "000", "-1"}, "jalr"},

        {{"0110111", "-1", "-1"}, "lui"},
        {{"0010111", "-1", "-1"}, "auipc"},

        // RV32M Extension (Multiplication and Division)
        {{"0110011", "000", "0000001"}, "mul"},
        {{"0110011", "001", "0000001"}, "mulh"},
        {{"0110011", "010", "0000001"}, "mulhsu"},
        {{"0110011", "011", "0000001"}, "mulhu"},
        {{"0110011", "100", "0000001"}, "div"},
        {{"0110011", "101", "0000001"}, "divu"},
        {{"0110011", "110", "0000001"}, "rem"},
        {{"0110011", "111", "0000001"}, "remu"},

        // System Instructions
        {{"0001111", "-1", "-1"}, "fence"},
        {{"1110011", "-1", "-1"}, "ecall"},
        {{"1110011", "-1", "-1"}, "ebreak"},
    };
    unordered_map<string, bool> opp_fun3 = {
        {"0110011", true},
        {"0010011", true},
        {"1100011", true},
        {"0000011", true},
        {"0100011", true},
        {"1101111", false},
        {"1100111", true},
        {"0110111", false},
        {"0010111", false}};

    unordered_map<string, bool> opp_fun7 = {
        {"0110011", true},
        {"0010011", false},
        {"1100011", false},
        {"0000011", false},
        {"0100011", false},
        {"1101111", false},
        {"1100111", false},
        {"0110111", false},
        {"0010111", false}};

public:
    void store_PC(const string &filename)
    {
        string Line;
        ifstream PCFile(filename);
        while (getline(PCFile, Line))
        {
            PC_MAP[PC] = Line;
            PC += 4;
        }
        limit_pc = PC;
        PC = 0x0;
    }

    string Fetch(int PC)
    {
        string IS = PC_MAP[PC];
        return IS;
    }

    I_DATA decode(string Instruction)
    {
        string opcode = Instruction.substr(25, 7);
        string fun3 = "-1";
        string fun7 = "-1";
        string rd = Instruction.substr(20, 5);
        string rs1 = Instruction.substr(12, 5);
        string rs2 = Instruction.substr(7, 5);

        string immi;
        if (opcode == "0010011" || opcode == "0000011" || opcode == "1100111")
        {                                     // I-type
            immi = Instruction.substr(0, 12); // First 12 bits [11:0]
        }
        else if (opcode == "0100011")
        {                                                                // S-type
            immi = Instruction.substr(0, 7) + Instruction.substr(20, 5); // imxm[4:0] + imm[11:5]
        }
        else if (opcode == "1100011")
        { // B-type
            immi = Instruction.substr(0, 1) + Instruction.substr(24, 1) + Instruction.substr(1, 6) + Instruction.substr(20, 4) + "0";
            // imm[12] + imm[11] + imm[10:5] + imm[4:1] (shifted left)
        }
        else if (opcode == "0110111" || opcode == "0010111")
        {                                     // U-type
            immi = Instruction.substr(0, 20); // First 20 bits [19:0]
        }
        else if (opcode == "1101111")
        { // J-type
            immi = Instruction.substr(0, 1) + Instruction.substr(12, 8) + Instruction.substr(11, 1) + Instruction.substr(1, 10) + "0";
            // imm[20] + imm[19:12] + imm[11] + imm[10:1] (shifted left)
        }
        else
        {
            immi = "000"; // No immediate for R-type
        }
        if (opp_fun3[opcode])
        {
            fun3 = Instruction.substr(17, 3);
        }
        if (opp_fun7[opcode] || (opcode == "0010011" && (fun3 == "001" || fun3 == "101")))
        {
            fun7 = Instruction.substr(0, 7);
        }

        I_DATA data;
        data.i_type = opcode;
        data.opperation = operation[{opcode, fun3, fun7}];

        data.rd = stoi(rd, 0, 2);
        data.rs1 = stoi(rs1, 0, 2);
        data.rs2 = stoi(rs2, 0, 2);
        int temp_immi = stoi(immi, 0, 2);
        int bits = immi.size();
        if (temp_immi & (1 << (bits - 1)))
        {
            data.immi = temp_immi - (1 << bits);
        }
        else
        {
            data.immi = temp_immi;
        }

        return data;
    };

    void execute(I_DATA data)
    {
        string operation = data.opperation;
        if (operation == "add")
        {
            EX = Register[data.rs1] + Register[data.rs2];
        }
        else if (operation == "sub")
        {
            EX = Register[data.rs1] - Register[data.rs2];
        }
        else if (operation == "sll")
        {
            EX = Register[data.rs1] << Register[data.rs2];
        }
        else if (operation == "slt")
        {
            EX = Register[data.rs1] < Register[data.rs2];
        }
        else if (operation == "sltu")
        {
            EX = (unsigned int)Register[data.rs1] < (unsigned int)Register[data.rs2] ? 1 : 0;
        }
        else if (operation == "xor")
        {
            EX = Register[data.rs1] ^ Register[data.rs2];
        }
        else if (operation == "srl")
        {
            EX = Register[data.rs1] >> Register[data.rs2];
        }
        else if (operation == "sra")
        {
            EX = Register[data.rs1] >> Register[data.rs2];
        }
        else if (operation == "or")
        {
            EX = Register[data.rs1] | Register[data.rs2];
        }
        else if (operation == "and")
        {
            EX = Register[data.rs1] & Register[data.rs2];
        }
        else if (operation == "addi")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "slti")
        {
            EX = Register[data.rs1] < data.immi;
        }
        else if (operation == "sltiu")
        {
            EX = (unsigned int)Register[data.rs1] < (unsigned int)data.immi;
        }
        else if (operation == "xori")
        {
            EX = Register[data.rs1] ^ data.immi;
        }
        else if (operation == "ori")
        {
            EX = Register[data.rs1] | data.immi;
        }
        else if (operation == "andi")
        {
            EX = Register[data.rs1] & data.immi;
        }
        else if (operation == "slli")
        {
            EX = Register[data.rs1] << data.immi;
        }
        else if (operation == "srli")
        {
            EX = Register[data.rs1] >> data.immi;
        }
        else if (operation == "srai")
        {
            EX = Register[data.rs1] >> data.immi;
        }
        // Branch instruction
        else if (operation == "beq")
        {

            if (Register[data.rs1] == Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC = PC + 4;
        }
        else if (operation == "bne")
        {
            if (Register[data.rs1] != Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC += 4;
        }
        else if (operation == "blt")
        {
            if (Register[data.rs1] < Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC += 4;
        }
        else if (operation == "bge")
        {
            if (Register[data.rs1] >= Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC += 4;
        }
        else if (operation == "bltu")
        {
            if (Register[data.rs1] < Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC += 4;
        }
        else if (operation == "bgeu")
        {
            if (Register[data.rs1] >= Register[data.rs2])
            {
                PC = PC + data.immi;
            }
            else
                PC += 4;
        }
        // store instruction
        else if (operation == "lb")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "lh")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "lw")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "lbu")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "lhu")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "sb")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "sh")
        {
            EX = Register[data.rs1] + data.immi;
        }
        else if (operation == "sw")
        {
            EX = Register[data.rs1] + data.immi;
        }

        // differnt
        else if (operation == "jal")
        {
            EX = PC + 4;
            PC = PC + data.immi;
        }
        else if (operation == "jalr")
        {
            EX = PC + 4;
            PC = Register[data.rs1] + data.immi;
        }

        else if (operation == "lui")
        {
            EX = data.immi << 12;
        }
        else if (operation == "auipc")
        {
            EX = PC + data.immi;
        }
        else if (operation == "mul")
        {
            cout << Register[data.rs1] << " " << Register[data.rs2] << endl;
            EX = Register[data.rs1] * Register[data.rs2];
        }
        else if (operation == "div")
        {
            EX = Register[data.rs1] / Register[data.rs2];
        }

        if (data.i_type != "1100011" && data.opperation != "jal" && data.opperation != "jalr")
        {
            PC += 4;
        }
    }

    void write_back(I_DATA data)
    {
        if (data.i_type == "0110011" || data.i_type == "0010011" || data.i_type == "1101111" || data.i_type == "1100111" || data.i_type == "0110111" || data.i_type == "0010111")
        {
            Register[data.rd] = EX;
        }
    }

    void memory(I_DATA data)
    {
        if (data.i_type == "0100011")
        {
            int val = Register[data.rs2];
            string curr = data.opperation;

            if (curr == "sb")
            {
                writeMemory(EX, val % (1 << 8));
            }
            else if (curr == "sh")
            {
                writeMemory(EX, val % 256);
                val >>= 8;
                writeMemory(EX + 1, val % 256);
            }
            else
            {
                writeMemory(EX, val % 256);
                val >>= 8;
                writeMemory(EX + 1, val % 256);
                val >>= 8;
                writeMemory(EX + 2, val % 256);
                val >>= 8;
                writeMemory(EX + 3, val % 256);
            }
        }

        else if (data.i_type == "0000011")
        {

            string curr = data.opperation;
            if (curr == "lb")
            {
                int d = readMemory(EX);
                if (d & (1 << 7))
                {
                    Register[data.rd] = d - (1 << 8);
                }
                else
                {
                    Register[data.rd] = d;
                }
            }

            else if (curr == "lw")
            {
                int d = (readMemory(EX + 3) << 24) + (readMemory(EX + 2) << 16) + (readMemory(EX + 1) << 8) + readMemory(EX);
                if (d & (1 << 31))
                {
                    Register[data.rd] = d - (1 << 32);
                }
                else
                {
                    Register[data.rd] = d;
                }
            }
            else if (curr == "lbu")
            {
                Register[data.rd] = readMemory(EX);
            }
            else if (curr == "lhu")
            {
                Register[data.rd] = readMemory(EX) + (readMemory(EX + 1) << 8);
            }
        }
    }

    void saveRegisterToFile(const string &filename)
    {
        ofstream outFile(filename);
        if (!outFile)
        {
            cerr << "Error: Could not open file for writing!\n";
            return;
        }

        for (const auto &pair : Register)
        {
            outFile << pair.first << " " << pair.second << endl;
        }

        outFile.close();
    }

    Simulator(const string &filename, const string &outputFile, const string &memory_file, const string &register_file)
    {
        store_PC(filename);
        ofstream outFile(outputFile);
        int cycles = 0;
        while (PC < limit_pc)
        {
            cycles++;
            Register[0] = 0;
            string IS = Fetch(PC);
            outFile << "Fetch   :" << IS << " " << PC << endl;
            I_DATA data = decode(IS);
            outFile << "Decode  :" << data.opperation << " ";
            if (!(data.i_type == "0100011" || data.i_type == "1100011"))
            {
                outFile << "rd:" << data.rd;
            }
            if (!(data.i_type == "0110111" || data.i_type == "0010111" || data.i_type == "1101111"))
            {
                outFile << " rs1:" << data.rs1;
            }
            if (!(data.i_type == "0010011" || data.i_type == "0000011" || data.i_type == "1101111" || data.i_type == "0110111" || data.i_type == "0010111"))
            {
                outFile << " rs2:" << data.rs2;
            }
            if (!(data.i_type == "0110011"))
            {
                outFile << " imm:" << data.immi;
            }
            outFile << endl;
            execute(data);
            outFile << "Execute :" << EX << endl;
            memory(data);
            if (data.i_type == "0000011" || data.i_type == "0100011")
            {
                outFile << "Memory  :" << Register[data.rd] << endl;
            }
            else
            {
                outFile << "Memory  :" << "Not used" << endl;
            }

            write_back(data);
            if (data.i_type == "0100011" || data.i_type == "1100011")
            {
                outFile << "WriteBack   :" << "Not used" << endl;
            }
            else
            {
                outFile << "WriteBack   :" << Register[data.rd] << endl;
            }
            outFile << "------------------------------------------------------------------------------------" << endl;
        }
        Register[0] = 0;
        outFile << "Cycles: " << cycles << endl;
        outFile.close();
        saveRegisterToFile(register_file);
        saveMemoryToFile(memory_file);
        cout << "Simulation Completed" << endl;
    }
};

#endif

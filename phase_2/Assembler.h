#ifndef ASSEMBLER
#define ASSEMBLER

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>
#include <bitset>
#include <iomanip>

#include "memory.h"

using namespace std;

class SymbolTable
{
private:
    unordered_map<string, int> table;

public:
    void insert(const string &label, int address)
    {
        table[label] = address;
    }

    int getAddress(const string &label) const
    {
        auto it = table.find(label);
        return (it != table.end()) ? it->second : -1;
    }
};

class Assembler
{
private:
    const string TERMINATION_CODE = "0x00000013"; // nop instruction
private:
    SymbolTable symTable;
    SymbolTable dataTable;
    int currentAddress = 0x00000000;
    int dataAddress = 0x10000000;
    int HeapAddress = 0x10008000;
    int StackAddress = 0x7FFFFFDC;
    vector<string> outputLines;
    vector<string> PCLines;
    vector<string> dataSegment;
    bool inTextSegment = true;
    string lastLabel = "";

    unordered_map<string, string> opcodeTable = {
        {"add", "0110011"}, {"sub", "0110011"}, {"sll", "0110011"}, {"slt", "0110011"}, {"sltu", "0110011"}, {"xor", "0110011"}, {"srl", "0110011"}, {"sra", "0110011"}, {"or", "0110011"}, {"and", "0110011"}, {"addi", "0010011"}, {"slti", "0010011"}, {"sltiu", "0010011"}, {"xori", "0010011"}, {"ori", "0010011"}, {"andi", "0010011"}, {"slli", "0010011"}, {"srli", "0010011"}, {"srai", "0010011"}, {"beq", "1100011"}, {"bne", "1100011"}, {"blt", "1100011"}, {"bge", "1100011"}, {"bltu", "1100011"}, {"bgeu", "1100011"}, {"lw", "0000011"}, {"sw", "0100011"}, {"jal", "1101111"}, {"lui", "0110111"}, {"auipc", "0010111"}, {"jalr", "1100111"}, {"lb", "0000011"}, {"lh", "0000011"}, {"lbu", "0000011"}, {"lhu", "0000011"}, {"sb", "0100011"}, {"sh", "0100011"}, {"addiw", "0011011"}, {"slliw", "0011011"}, {"srliw", "0011011"}, {"sraiw", "0011011"}, {"sd", "0100011"}, {"ld", "0000011"}, {"mul", "0110011"}, {"div", "0110011"}};

    unordered_map<string, string> funct3Table = {
        {"add", "000"}, {"sub", "000"}, {"sll", "001"}, {"slt", "010"}, {"sltu", "011"}, {"xor", "100"}, {"srl", "101"}, {"sra", "101"}, {"or", "110"}, {"and", "111"}, {"addi", "000"}, {"slti", "010"}, {"sltiu", "011"}, {"xori", "100"}, {"ori", "110"}, {"andi", "111"}, {"slli", "001"}, {"srli", "101"}, {"srai", "101"}, {"beq", "000"}, {"bne", "001"}, {"blt", "100"}, {"bge", "101"}, {"bltu", "110"}, {"bgeu", "111"}, {"lw", "010"}, {"sw", "010"}, {"jal", "000"}, {"lui", "011"}, {"auipc", "011"}, {"jalr", "000"}, {"lb", "000"}, {"lh", "001"}, {"lbu", "100"}, {"lhu", "101"}, {"sb", "000"}, {"sh", "001"}, {"addiw", "000"}, {"slliw", "001"}, {"srliw", "101"}, {"sraiw", "101"}, {"sd", "011"}, {"mul", "000"}, {"div", "100"}};

    unordered_map<string, string> funct7Table = {
        {"add", "0000000"}, {"sub", "0100000"}, {"sll", "0000000"}, {"slt", "0000000"}, {"sltu", "0000000"}, {"xor", "0000000"}, {"srl", "0000000"}, {"sra", "0100000"}, {"or", "0000000"}, {"and", "0000000"}, {"slli", "0000000"}, {"srli", "0000000"}, {"srai", "0100000"}, {"addiw", "0000000"}, {"slliw", "0000000"}, {"srliw", "0000000"}, {"sraiw", "0100000"}, {"sd", "0000000"}, {"ld", "0000000"}, {"mul", "0000001"}, {"div", "0000001"}};

    unordered_map<string, string> instructionType = {
        {"add", "R"}, {"sub", "R"}, {"sll", "R"}, {"slt", "R"}, {"sltu", "R"}, {"xor", "R"}, {"srl", "R"}, {"sra", "R"}, {"or", "R"}, {"and", "R"}, {"addi", "I"}, {"slti", "I"}, {"sltiu", "I"}, {"xori", "I"}, {"ori", "I"}, {"andi", "I"}, {"slli", "I"}, {"srli", "I"}, {"srai", "I"}, {"beq", "B"}, {"bne", "B"}, {"blt", "B"}, {"bge", "B"}, {"bltu", "B"}, {"bgeu", "B"}, {"lw", "I"}, {"sw", "S"}, {"jal", "J"}, {"lui", "U"}, {"auipc", "U"}, {"jalr", "I"}, {"lb", "I"}, {"lh", "I"}, {"lbu", "I"}, {"lhu", "I"}, {"sb", "S"}, {"sh", "S"}, {"addiw", "I"}, {"slliw", "I"}, {"srliw", "I"}, {"sraiw", "I"}, {"sd", "S"}, {"ld", "I"}, {"mul", "R"}, {"div", "R"}};
    string registerToBinary(const string &reg)
    {
        if (reg[0] != 'x' || reg.size() < 2)
        {
            cerr << "Error: Invalid register format - " << reg << endl;
            return "00000"; // Default to x0
        }

        return bitset<5>(stoi(reg.substr(1))).to_string();
    }

    string immediateToBinary(int imm, int bits)

    {
        int maxVal = (1 << (bits - 1)) - 1;
        int minVal = -(1 << (bits - 1));

        if (imm < minVal || imm > maxVal)
        {
            cerr << "Error: Immediate value out of range - " << imm << endl;
            return bitset<32>(0).to_string().substr(32 - bits, bits); // Default to 0
        }
        return bitset<32>(imm).to_string().substr(32 - bits, bits);
    }

    string binaryToHex(const string &binary)
    {
        if (binary.length() % 4 != 0)
        {
            throw invalid_argument("Binary string length must be a multiple of 4");
        }

        string hexResult = "0x";
        for (size_t i = 0; i < binary.length(); i += 4)
        {
            string fourBits = binary.substr(i, 4);
            int decimalValue = stoi(fourBits, nullptr, 2);
            hexResult += "0123456789ABCDEF"[decimalValue];
        }
        return hexResult;
    }
    void handleDataDirective(const vector<string> &tokens, ofstream &outputFile)
    {

        if (tokens.empty())
            return;
        string directive = tokens[0];
        if (tokens.size() < 2)
        {
            cerr << "Error: Missing operand for directive - " << directive << endl;
            return;
        }

        if (directive == ".word")
        {
            if (!lastLabel.empty())
            {

                dataTable.insert(lastLabel, dataAddress);

                lastLabel.clear();
            }
            outputFile << "0x" << hex << dataAddress << " ";
            for (size_t i = 1; i < tokens.size(); i++)
            {
                int value = stoi(tokens[i]);
                for (int j = 0; j < 4; j++)
                {
                    writeMemory(dataAddress++, (value >> (j * 8)) & 0xFF);
                    outputFile << "0x" << hex << ((value >> (j * 8)) & 0xFF) << " ";
                }
            }
            outputFile << endl;
        }
        else if (directive == ".dword")
        {
            if (!lastLabel.empty())
            {
                dataTable.insert(lastLabel, dataAddress);
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // data >>= 8;
                // dataAddress += 1;
                // writeMemory(dataAddress, data & bitMask);
                // dataAddress += 1;
                lastLabel.clear();
            }
            for (size_t i = 1; i < tokens.size(); i++)
            {
                int value = stoi(tokens[i]);
                outputFile << "0x" << hex << dataAddress << " ";
                for (int j = 0; j < 8; j++)
                {
                    writeMemory(dataAddress++, (value >> (j * 8)) & 0xFF);
                    outputFile << "0x" << hex << ((value >> (j * 8)) & 0xFF) << " ";
                }
            }
            outputFile << endl;
        }
        else if (directive == ".byte")
        {
            if (!lastLabel.empty())
            {
                dataTable.insert(lastLabel, dataAddress);
                lastLabel.clear();
            }
            outputFile << "0x" << hex << dataAddress << " ";
            for (size_t i = 1; i < tokens.size(); i++)
            {
                int value = stoi(tokens[i]);
                writeMemory(dataAddress++, value & 0xFF);
                outputFile << "0x" << hex << (value & 0xFF) << " ";
            }
            outputFile << endl;
        }
        else if (directive == ".half")
        {
            if (!lastLabel.empty())
            {
                dataTable.insert(lastLabel, dataAddress);
                lastLabel.clear();
            }
            outputFile << "0x" << hex << dataAddress << " ";
            for (size_t i = 1; i < tokens.size(); i++)
            {
                int value = stoi(tokens[i]);
                for (int j = 0; j < 2; j++)
                {
                    writeMemory(dataAddress++, (value >> (j * 8)) & 0xFF);
                    outputFile << "0x" << hex << ((value >> (j * 8)) & 0xFF) << " ";
                }
            }
            outputFile << endl;
        }
        else if (directive == ".string" || directive == ".asciz")
        {
            string str = tokens[1].substr(1, tokens[1].size() - 2);
            if (!lastLabel.empty())
            {
                dataTable.insert(lastLabel, dataAddress);

                lastLabel.clear();
            }
            outputFile << "0x" << hex << dataAddress << " ";
            for (char c : str)
            {
                writeMemory(dataAddress, c);
                outputFile << c << " ";
                dataAddress += 1;
            }
            outputFile << endl;
            writeMemory(dataAddress, '\0');
            dataAddress += 1;
        }
        else
        {
            cerr << "Error: Unsupported directive - " << directive << endl;
        }
    }

    void encodeInstruction(const vector<string> &tokens)
    {
        if (tokens.empty())
            return;
        string instr = tokens[0];
        if (instructionType.find(instr) == instructionType.end())
        {
            cerr << "Error: Invalid instruction - " << instr << endl;
            return;
        }

        string binaryEncoding, breakdown, assemblyFormat;
        if (instructionType[instr] == "R")
        {
            binaryEncoding = funct7Table[instr] + registerToBinary(tokens[3]) +
                             registerToBinary(tokens[2]) + funct3Table[instr] +
                             registerToBinary(tokens[1]) + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-" + funct3Table[instr] + "-" + funct7Table[instr] +
                        "-" + registerToBinary(tokens[1]) + "-" + registerToBinary(tokens[2]) +
                        "-" + registerToBinary(tokens[3]);

            assemblyFormat = instr + " " + tokens[1] + " " + tokens[2] + " " + tokens[3];
        }
        else if (instructionType[instr] == "I")
        {
            string rd, rs1, rs2;

            if (tokens.size() < 4)
            {
                rd = tokens[1];
                size_t pos = tokens[2].find('(');
                rs2 = tokens[2].substr(0, pos);
                rs1 = tokens[2].substr(pos + 1, tokens[2].size() - pos - 2);
            }
            else
            {
                rd = tokens[1];
                rs1 = tokens[2];
                rs2 = tokens[3];
            }

            string imm = immediateToBinary(stoi(rs2), 12);

            binaryEncoding = imm + registerToBinary(rs1) +
                             funct3Table[instr] + registerToBinary(tokens[1]) + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-" + funct3Table[instr] + "-" + imm +
                        "-" + registerToBinary(tokens[1]) + "-" + registerToBinary(rs1);

            assemblyFormat = instr + " " + tokens[1] + " " + rs1 + " " + rs2;
        }
        else if (instructionType[instr] == "S")
        {
            size_t pos = tokens[2].find('(');
            string imm = immediateToBinary(stoi(tokens[2].substr(0, pos)), 12);
            string reg = tokens[2].substr(pos + 1, tokens[2].size() - (pos + 2));

            binaryEncoding = imm.substr(0, 7) + registerToBinary(tokens[1]) +
                             registerToBinary(reg) + funct3Table[instr] +
                             imm.substr(7, 5) + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-" + funct3Table[instr] + "-" + imm +
                        "-" + registerToBinary(tokens[1]) + "-" + registerToBinary(reg);

            assemblyFormat = instr + " " + tokens[1] + " " + tokens[2].substr(0, pos) + "(" + reg + ")";
        }
        else if (instructionType[instr] == "B")
        {
            int labelAddress = symTable.getAddress(tokens[3]);
            if (labelAddress == -1)
            {
                cerr << "Error: Undefined label - " << tokens[3] << endl;
                return;
            }

            int offset = (labelAddress - currentAddress);

            string imm = immediateToBinary(offset, 13);

            binaryEncoding = imm[0] + imm.substr(2, 6) + registerToBinary(tokens[2]) +
                             registerToBinary(tokens[1]) + funct3Table[instr] +
                             imm.substr(8, 4) + imm[1] + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-" + funct3Table[instr] + "-XX-" +
                        registerToBinary(tokens[1]) + "-" + registerToBinary(tokens[2]) +
                        "-XX-" + imm;

            assemblyFormat = instr + " " + tokens[1] + " " + tokens[2] + " " + tokens[3];
        }
        else if (instructionType[instr] == "J")
        {
            int labelAddress = symTable.getAddress(tokens[2]);
            int offset = (labelAddress - currentAddress);
            string imm = immediateToBinary(offset, 21);

            binaryEncoding = imm[0] + imm.substr(10, 10) + imm[9] +
                             imm.substr(1, 8) + registerToBinary(tokens[1]) + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-XX-" + registerToBinary(tokens[1]) + "-XX-" + imm;

            assemblyFormat = instr + " " + tokens[1] + " " + tokens[2];
        }
        else if (instructionType[instr] == "U")
        {
            string imm = immediateToBinary(stoi(tokens[2], nullptr, 16), 20);
            binaryEncoding = imm + registerToBinary(tokens[1]) + opcodeTable[instr];

            breakdown = opcodeTable[instr] + "-XX-" + registerToBinary(tokens[1]) + "-XX-" + imm;

            assemblyFormat = instr + " " + tokens[1] + " " + tokens[2];
        }

        ostringstream formattedOutput;

        formattedOutput << "0x" << hex << currentAddress << " " << binaryToHex(binaryEncoding) << "  " << assemblyFormat
                        << " # " << breakdown;
        PCLines.push_back(binaryEncoding);
        outputLines.push_back(formattedOutput.str());
        currentAddress += 4;
    }

public:
    Assembler(const string &inputFilename, const string &outputFilename, const string &PC)
    {
        ifstream inputFile(inputFilename);
        ofstream outputFile(outputFilename);
        ofstream PCfile(PC);
        string line;

        if (!inputFile)
        {
            cerr << "Error opening input file!" << endl;
            return;
        }
        while (getline(inputFile, line))
        {
            size_t commentPos = line.find('#');
            if (commentPos != string::npos)
                line = line.substr(0, commentPos);
            commentPos = line.find("//");
            if (commentPos != string::npos)
                line = line.substr(0, commentPos);
            istringstream iss(line);
            vector<string> tokens;
            string token;

            while (iss >> token)
            {
                tokens.push_back(token);
            }

            if (tokens.empty())
                continue;
            if (tokens[0] == ".text")
            {
                // outputFile << "0x" << hex << currentAddress << " # Text segment start" << endl;
                inTextSegment = true;
                continue;
            }
            else if (tokens[0] == ".data")
            {
                // outputFile << "0x" << hex << dataAddress << " # Data segment start" << endl;
                inTextSegment = false;
                continue;
            }

            if (tokens[0].back() == ':')
            {
                string label = tokens[0].substr(0, tokens[0].size() - 1);
                if (inTextSegment)
                {
                    symTable.insert(label, currentAddress);
                    if (tokens.size() > 1)
                    {
                        currentAddress += 4;
                    }
                }
                else
                {
                    lastLabel = label;
                    handleDataDirective(vector<string>(tokens.begin() + 1, tokens.end()), outputFile);
                }
            }
            else
            {
                if (inTextSegment)
                {
                    currentAddress += 4;
                }
                else
                {
                    if (lastLabel.empty())
                    {
                        cerr << "Error: Data directive found without a preceding label!" << endl;
                        exit(1); // Stop execution since this is an error
                    }
                    handleDataDirective(tokens, outputFile);
                }
            }
        }
        inputFile.close();

        // second Pass
        inputFile.open(inputFilename);
        currentAddress = 0x00000000;

        if (!inputFile)
        {
            cerr << "Error reopening input file!" << endl;
            return;
        }

        inTextSegment = true;
        while (getline(inputFile, line))
        {
            size_t commentPos = line.find('#');
            if (commentPos != string::npos)
                line = line.substr(0, commentPos);
            commentPos = line.find("//");
            if (commentPos != string::npos)
                line = line.substr(0, commentPos);

            istringstream iss(line);
            vector<string> tokens;
            string token;
            while (iss >> token)
            {
                if (token[0] == '(')
                {
                    token = token.substr(1, token.size() - 2);
                }
                tokens.push_back(token);
            }

            if (tokens.empty())
                continue;
            if (tokens[0] == ".text")
            {
                outputFile << "0x" << hex << currentAddress << " # Text segment start" << endl;
                inTextSegment = true;
                continue;
            }
            else if (tokens[0] == ".data")
            {
                outputFile << "0x" << hex << dataAddress << " # Data segment start" << endl;
                inTextSegment = false;
                continue;
            }

            if (tokens[0].back() == ':')
            {
                if (inTextSegment)
                {
                    encodeInstruction(vector<string>(tokens.begin() + 1, tokens.end()));
                }
            }
            else
            {
                if (inTextSegment)
                {
                    encodeInstruction(tokens);
                }
            }
        }

        for (const auto &output : outputLines)
        {
            outputFile << output << endl;
        }
        for (const auto &P : PCLines)
        {
            PCfile << P << endl;
        }
        outputFile << "Vikash " << TERMINATION_CODE << "  # End of program" << endl;
        cout << "Assembly completed. Output written to " << outputFilename << endl;
        saveMemoryToFile("memory.mem");
        outputFile.close();
        inputFile.close();
    }
};

#endif

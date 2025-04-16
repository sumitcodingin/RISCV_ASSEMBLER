#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <string>
#include <math.h>
#include <sstream>
#include <iomanip>
#include "Riscv_Instructions.h"
#include "Instructions_Func.h"
#include "Auxiliary_Functions.h"

using namespace std;

// Object of instruction being executed
RISC_V_Instructions Instruction;

// For storing all labels
unordered_map<string, long long> labels;
vector<string> textDirectiveInst, dataDirectiveInst, textOutputCode, dataOutputCode;

long long pc = 0;

// For error output with a default of no error (Constructor)
Error output_error = Error(ERROR_NONE, "Code executed Successfully!!!");

// Binary to Hexadecimal conversion with 0x prefix
string binaryToHex(const string& binary, int bit_length) {
    // Convert binary to decimal
    unsigned long long decimal = 0;
    for (char bit : binary) {
        decimal = decimal * 2 + (bit - '0');
    }

    // Calculate the number of hex digits needed (1 hex digit = 4 bits)
    int hex_digits = (bit_length + 3) / 4;

    // Convert decimal to hex with 0x prefix
    stringstream ss;
    ss << "0x" << hex << uppercase << setfill('0') << setw(hex_digits) << decimal;
    return ss.str();
}
void dataDirectives(vector<string> dataInst) {
    long long memory_address = 268435456; // 0x10000000
    string data, temp_word, output_string;
    int size_of_data = 0;

    for (size_t i = 0; i < dataInst.size(); i++) {
        data = dataInst[i];
        stringstream ss(data);
        output_string = "";
        string label;

        // Handle labels
        size_t colonPos = data.find(':');
        if (colonPos != string::npos) {
            label = data.substr(0, colonPos);
            string afterColon = data.substr(colonPos + 1);
            afterColon.erase(0, afterColon.find_first_not_of(" \t"));
            data = afterColon;
            ss.str(data);
            ss.clear();
        }

        // Get data type
        ss >> temp_word;
      
        if (temp_word.empty()) {
            output_error.AlterError(INVALID_DATA, "Empty data directive");
            output_error.PrintError();
            return;
        }

        if (dataTypeSize.find(temp_word) == dataTypeSize.end()) {
            output_error.AlterError(INVALID_DATA, "Invalid data type: " + temp_word);
            output_error.PrintError();
            return;
        }
        size_of_data = dataTypeSize[temp_word];

        // Handle .word with comma-separated values
        if (temp_word == ".word") {
            string remaining_data;
            getline(ss, remaining_data);
            remaining_data = trim(remaining_data);
            
            // Split by commas
            stringstream values(remaining_data);
            string value;
            while (getline(values, value, ',')) {
                value = trim(value);
                try {
                    long long num_value = stoll(value);
                    long long max_val = (1LL << (size_of_data * 8 - 1)) - 1;
                    long long min_val = -(1LL << (size_of_data * 8 - 1));
                   
                    
                    if (num_value < min_val || num_value > max_val) {
                      
                        output_error.AlterError(INVALID_DATA, "Value out of range: " + value);
                        output_error.PrintError();
                        return;
                    }

                    string hex_address = binaryToHex(decToBinary_Len(memory_address, 32), 32);
                    string hex_data = binaryToHex(decToBinary_Len(num_value, 8 * size_of_data), 8 * size_of_data);
                    output_string = hex_address + " " + hex_data;
                    cout << "DATA: " << output_string << endl;
                    dataOutputCode.push_back(output_string);
                    memory_address += size_of_data;
                } catch (const invalid_argument&) {
                    output_error.AlterError(INVALID_DATA, "Invalid number: " + value);
                    continue;
                } catch (const out_of_range&) {
                    output_error.AlterError(INVALID_DATA, "Number out of range: " + value);
                    continue;
                }
            }
        }
        // Handle .asciiz or .string
        else if (temp_word == ".asciiz" || temp_word == ".string") {
            string remainder;
            getline(ss, remainder) ;
         
            remainder.erase(0, remainder.find_first_not_of(" \t"));

            if (remainder.empty() || remainder[0] != '"' || remainder.back() != '"') {
               
                output_error.AlterError(INVALID_DATA, "Invalid string format: " + remainder);
                output_error.PrintError();
                return;
            }

            string s = remainder.substr(1, remainder.length() - 2);
         
            for (char c : s) {
                int ascii_val = static_cast<unsigned char>(c); // convert into ascii value
                string hex_address = binaryToHex(decToBinary_Len(memory_address, 32), 32);
                string hex_data = binaryToHex(decToBinary_Len(ascii_val, 8), 8);
                output_string = hex_address + " " + hex_data;
                cout << "DATA: " << output_string << endl;
                dataOutputCode.push_back(output_string);
                memory_address += size_of_data;
            }
            if (temp_word == ".asciiz") {
                string hex_address = binaryToHex(decToBinary_Len(memory_address, 32), 32);
                string hex_data = binaryToHex(decToBinary_Len(0, 8), 8);
                output_string = hex_address + " " + hex_data;
                cout << "DATA (null): " << output_string << endl;
                dataOutputCode.push_back(output_string);
                memory_address += size_of_data;
            }
        }
        // Handle other data types (.half, .byte)
        else {
            while (ss >> temp_word) {
                try {
                    long long value = stoll(temp_word);
                    
                    long long max_val = (1LL << (size_of_data * 8 - 1)) - 1;
                    long long min_val = -(1LL << (size_of_data * 8 - 1));
                    if (value < min_val || value > max_val) {
                        output_error.AlterError(INVALID_DATA, "Value out of range: " + temp_word);
                        output_error.PrintError();
                        return;
                    }

                    string hex_address = binaryToHex(decToBinary_Len(memory_address, 32), 32);
                    string hex_data = binaryToHex(decToBinary_Len(value, 8 * size_of_data), 8 * size_of_data);
                    output_string = hex_address + " " + hex_data;
                    cout << "DATA: " << output_string << endl;
                    dataOutputCode.push_back(output_string);
                    memory_address += size_of_data;
                } catch (const exception& e) {
                    output_error.AlterError(INVALID_DATA, "Error processing value: " + temp_word);
                    continue;
                }
            }
        }
    }
}
int main()
{
    // Opening asm file
    ifstream input_file("main.asm");
    // Output machine code files
    ofstream text_file("text.mc", ios::out);
    ofstream data_file("data.mc", ios::out);

    // Reading file line by line (instruction by instruction)
    string line;
    bool isText = true, isData = true;
    if (input_file.is_open())
    {
        while (getline(input_file, line))
        {
            // Removing leading and trailing spaces an instruction
            line = trim(line);
           
            // Empty or comments
            if (line.empty() || line[0] == '#')
                continue; // Ignore Comments and empty lines
            
            // If directives
            if (line == ".data")
            {
                isText = false;
                isData = true;
                continue;
            }
            else if (line == ".text")
            {
                isText = true;
                isData = false;
                continue;
            }

            // To get entire block
            if (isData == true){
                dataDirectiveInst.push_back(line);
               
            }
            else if (isText == true)
            {
                /* Setting Program Counter for labels */
                try
                {
                    // Getting colon position if label
                    size_t colonPos = line.find(':');
                    if (colonPos != string::npos)
                    {
                        
                        
                        size_t comment_pos = line.find('#');
                        if (comment_pos != string::npos) {
                        line = line.substr(0, comment_pos); // Keep only the part before '#'
                        }
                        line = trim(line); 
                        line = trim(line.substr(0, line.length()-1));
                        labels[line] = pc;
                        pc -= 4;
                    }
                    else{
                       
                        textDirectiveInst.push_back(line);
                        
                    }
                }
                catch (exception ex)
                {
                   
                }
                pc += 4;
            }
        }
    }

     for(const auto& pair : labels){
         cout << pair.first << " " << pair.second << endl;
     }
    // Assembling Text Instructions
    line = "";
    pc = 0;
    string output_string = "";

    
    for (size_t i = 0; i < textDirectiveInst.size(); i++)
    {
        output_string = "";
        line = textDirectiveInst[i];
        
        Instruction = InitializeInstruction(labels, line, &output_error, pc);

        // Temporary string to hold the full 32-bit instruction in binary
        string binary_instruction = "";

        if (Instruction.type == 1) { // R-Type
            binary_instruction = Instruction.func7 + decToBinary_Len(Instruction.rs2, 5) +
                                decToBinary_Len(Instruction.rs1, 5) + Instruction.func3 +
                                decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode;
        }
        else if (Instruction.type == 2) { // I-Type
            bitset<12> imm_bits(Instruction.imm);
            binary_instruction = imm_bits.to_string() +
                                decToBinary_Len(Instruction.rs1, 5) + Instruction.func3 +
                                decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode;
        }
        else if (Instruction.type == 3) { // S-Type
            bitset<12> imm_bits(Instruction.imm);
            binary_instruction = imm_bits.to_string().substr(0, 7) +
                                decToBinary_Len(Instruction.rs2, 5) + decToBinary_Len(Instruction.rs1, 5) +
                                Instruction.func3 + imm_bits.to_string().substr(7, 5) + Instruction.OpCode;
        }
        else if (Instruction.type == 4) { // SB-Type
            bitset<13> imm_bits(Instruction.imm);
            binary_instruction = imm_bits.to_string()[0] + imm_bits.to_string().substr(2, 6) +
                                decToBinary_Len(Instruction.rs2, 5) + decToBinary_Len(Instruction.rs1, 5) +
                                Instruction.func3 + imm_bits.to_string().substr(8, 4) +
                                imm_bits.to_string()[1] + Instruction.OpCode;
        }
        else if (Instruction.type == 5) { // U-Type
            bitset<20> imm_bits(Instruction.imm);
            binary_instruction = imm_bits.to_string() + decToBinary_Len(Instruction.rd, 5) +
                                Instruction.OpCode;
        }
        else if (Instruction.type == 6) { // UJ-Type
            bitset<21> imm_bits(Instruction.imm);
            binary_instruction = imm_bits.to_string()[0] + imm_bits.to_string().substr(10, 10) +
                                imm_bits.to_string()[9] + imm_bits.to_string().substr(1, 8) +
                                decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode;
        }
        else if (Instruction.type == 7) { // Shift-Immediate Type
            bitset<5> shamt_bits(Instruction.imm); // 6-bit shift amount
            string imm_field = Instruction.func7 + shamt_bits.to_string(); // 7-bit func7 + 6-bit shamt
            binary_instruction = imm_field + // 12 bits total
                                 decToBinary_Len(Instruction.rs1, 5) +
                                 Instruction.func3 +
                                 decToBinary_Len(Instruction.rd, 5) +
                                 Instruction.OpCode;
        }

        // Convert PC and instruction to hexadecimal with 0x prefix
        string hex_pc = binaryToHex(decToBinary_Len(pc, 32), 32);
        string hex_instruction = binaryToHex(binary_instruction, 32);
        string binary_output = binary_instruction;

        // Format output as "0xPC_HEX 0xINSTRUCTION_HEX"
        output_string = hex_pc + " " + hex_instruction + "  # " + binary_output;
        textOutputCode.push_back(output_string); // Store in textOutputCode
        cout << "TEXT:" << output_string << endl;

        pc += 4;
    }

   
    dataDirectives(dataDirectiveInst);

    // Write text segment to text.mc
    if (text_file.is_open())
    {
        for (size_t i = 0; i < textOutputCode.size(); i++)
        {
            text_file << textOutputCode[i] << endl;
        }
        text_file.close();
    }

    // Write data segment to data.mc
    if (data_file.is_open())
    {
        for (size_t i = 0; i < dataOutputCode.size(); i++)
        {
            data_file << dataOutputCode[i] << endl;
        }
        data_file.close();
    }

    input_file.close();
    return 0;
}

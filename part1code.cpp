#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <string>
#include <math.h>
#include "Riscv_Instructions.h"
#include "Instructions_Func.h"
#include "Auxiliary_Functions.h"

using namespace std;

// Object of instruction being executed
RISC_V_Instructions Instruction;

// For storing all labels
unordered_map<string, long long> labels;
vector<string> textDirectiveInst, dataDirectiveInst, outputCode;

long long pc = 0;

// For error output with a default of no error (Constructor)
Error output_error = Error(ERROR_NONE, "Code executed Successfully!!!");

void dataDirectives(vector<string> dataInst)
{
    long long memory_address = 268435456; // Starting address in decimal
    string data = "", temp_word, output_string;
    int size_of_data = 0;
    // to get word by word
    stringstream ss;
    // For each data instruction
    for(size_t i = 0; i < dataDirectiveInst.size(); i++)
    {
        ss.clear();
        output_string = "";
        data = dataInst[i];
        ss << data;
        // getting type
        ss >> temp_word;
        if(dataTypeSize.find(temp_word) != dataTypeSize.end())
        {
            size_of_data = dataTypeSize[temp_word];   
        }
        else
        {
            // Altering error to invalid data
            output_error.AlterError(INVALID_DATA, "Data Error");
            output_error.PrintError();
            // Exiting with error code
            exit(INVALID_DATA);
        }

        // If asciiz
        if(temp_word == ".asciiz")
        {
            // getting ascii data
            ss >> temp_word;
            // If not in commas
            if (temp_word[0] != '\"' || temp_word[temp_word.length()-1] != '\"')
            {
                // Altering error to invalid data
                output_error.AlterError(INVALID_DATA, "Data Error");
                output_error.PrintError();
                // Exiting with error code
                exit(INVALID_DATA);
            }

            // getting data to store
            string s = temp_word.substr(1, temp_word.length()-2);
            // Ascii val for string letter
            int ascii_Val = 0;
            // Storing letter by letter using ascii and address
            for(size_t j = 0; j < s.length(); j++)
            {
                output_string = "";
                // Letter by letter getting ascii
                ascii_Val = s[j];
                // Storing in output (CHANGED TO BINARY)
                output_string = decToBinary_Len(memory_address, 32) + " " + decToBinary_Len(ascii_Val, 8 * size_of_data);
                cout << "DATA:" << output_string << endl;
                outputCode.push_back(output_string);
                // increasing address to store
                memory_address += size_of_data;
            }
        }
        // For other data types
        else
        {
            while(!ss.eof())
            {
                ss >> temp_word;
                // If out of range of data
                if(stoi(temp_word) < -pow(2, (size_of_data * 8) - 1) || stoi(temp_word) > (pow(2, (size_of_data * 8) - 1) -1))
                {
                    // Altering error to invalid data
                    output_error.AlterError(INVALID_DATA, "Data Error");
                    output_error.PrintError();
                    // Exiting with error code
                    exit(INVALID_DATA);
                }
                // Storing in output (CHANGED TO BINARY)
                output_string = decToBinary_Len(memory_address, 32) + " " + decToBinary_Len(stoi(temp_word), 8 * size_of_data);
                cout << "DATA:" << output_string << endl;
                outputCode.push_back(output_string);
                // Increasing memory location
                memory_address += size_of_data;
            }
        }
    }
}

int main()
{
    // Opening asm file
    ifstream input_file("main.asm");
    // Output machine code file
    ofstream output_file("main.mc", ios::out);

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
            
            // If derivatives
            if (line == ".data")
            {
                isText = false;
                isData = true;
                continue;
            }
            else if(line == ".text")
            {
                isText = true;
                isData = false;
                continue;
            }

            // To get entire block
            if(isData == true)
                dataDirectiveInst.push_back(line);
            else if(isText == true)
            {
                /* Setting Program Counter for labels */
                try
                {
                    // Getting colon position if label
                    size_t colonPos = line.find(':');
                    if(colonPos != string::npos)
                    {
                        line = trim(line.substr(0, line.length()-1));
                        labels[line] = pc;
                    }
                    else
                        textDirectiveInst.push_back(line);
                }
                catch(exception ex)
                {

                }
                pc++;
            }
        }
    }

    // Assembling Text Instructions
    line = "";
    pc = 0;
    string output_string = "";;
    for(size_t i = 0; i < textDirectiveInst.size(); i++)
    {
        output_string = "";
        line = textDirectiveInst[i];
        Instruction = InitializeInstruction(labels, line, &output_error, pc);
        // Storing Instructions Output (CHANGED TO BINARY)
        if(Instruction.type == 1) // R-Type
        {
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (Instruction.func7 + decToBinary_Len(Instruction.rs2, 5) +
                            decToBinary_Len(Instruction.rs1, 5) + Instruction.func3 +
                            decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        else if(Instruction.type == 2) // I-Type
        {
            // Converting 12 bits immediate value
            bitset<12> imm_bits(Instruction.imm);
            // Output String with concatenation
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (imm_bits.to_string() +
                            decToBinary_Len(Instruction.rs1, 5) + Instruction.func3 +
                            decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        else if(Instruction.type == 3) // S-Type
        {
            // Converting 12 bits immediate value
            bitset<12> imm_bits(Instruction.imm);
            // Output String with concatenation
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (imm_bits.to_string().substr(0, 7) +
                            decToBinary_Len(Instruction.rs2, 5) + decToBinary_Len(Instruction.rs1, 5) + 
                            Instruction.func3 + imm_bits.to_string().substr(7, 5) + Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        else if(Instruction.type == 4) // SB-Type
        {
            // Converting 13 bits immediate value
            bitset<13> imm_bits(Instruction.imm);
            // Output String with concatenation
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (imm_bits.to_string()[0] + imm_bits.to_string().substr(2, 6) +
                            decToBinary_Len(Instruction.rs2, 5) + decToBinary_Len(Instruction.rs1, 5) + 
                            Instruction.func3 + imm_bits.to_string().substr(8, 4) + 
                            imm_bits.to_string()[1] + Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        else if(Instruction.type == 5) // U-Type
        {
            // Converting 20 bits immediate value
            bitset<20> imm_bits(Instruction.imm);
            // Output String with concatenation
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (imm_bits.to_string() + decToBinary_Len(Instruction.rd, 5) + 
                            Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        else if(Instruction.type == 6) // UJ-Type
        {
            // Converting 21 bits immediate value
            bitset<21> imm_bits(Instruction.imm);
            // Output String with concatenation
            output_string = decToBinary_Len(pc, 32) + " " + 
                            (imm_bits.to_string()[0] + imm_bits.to_string().substr(10, 10) + 
                            imm_bits.to_string()[9] + imm_bits.to_string().substr(1, 8) +
                            decToBinary_Len(Instruction.rd, 5) + Instruction.OpCode);
            outputCode.push_back(output_string);
            cout << output_string << endl;
        }
        pc += 4;
    }

    dataDirectives(dataDirectiveInst);

    // Writing in File
    if(output_file.is_open())
    {
        for(size_t i = 0; i < outputCode.size(); i++)
        {
            output_file << outputCode[i] << endl;
        }
    }
}
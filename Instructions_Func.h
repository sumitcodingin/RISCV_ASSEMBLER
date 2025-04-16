#ifndef ERROR_H // This needs to be unique in each header
#define ERROR_H

#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <math.h>
#include "Riscv_Instructions.h"
#include "Auxiliary_Functions.h"

using namespace std;

// Enum to have error code
enum ErrorType
{
    ERROR_NONE = 0x00,              // No Error
    ERROR_SYNTAX = 0x01,            // Syntax is wrong
    INVALID_INSTRUCTION = 0x02,     // Instruction is not valid
    INVALID_REGISTER = 0x03,        // Register is invalid
    INVALID_IMMEDIATE_VALUE = 0x04, // Imm Value is invalid
    INVALID_DATA = 0x05,            // Invalid Data
    INVALID_LABEL = 0x06            // Branch Target is invalid
};

// Defining Structure of Return Errors
class Error
{
private:
    // Getting Type of error
    ErrorType err;
    // String error message
    string msg;

public:
    // A Constructor
    Error(ErrorType error, string message)
    {
        err = error;
        msg = message;
    }
    // To Alter error
    
    void PrintError()
    {
        cout << "Code is exited with error code " << err << ". \"" << msg << "\"";
    }

    void AlterError(ErrorType error, string message)
    {
        err = error;
        msg = message;
    }
};
int Calculate_Immediate(const string &hex, int bit_width, bool is_signed, Error *output_error)
{
    // Convert hex or decimal string to decimal value, checking against specified bit width
    string hexString = hex;
   
    int decimalValue = 0;

    // Determine range based on bit width and signed/unsigned
    long long max_value = (1LL << (bit_width - (is_signed ? 1 : 0))) - 1; // 2^(n-1) - 1 or 2^n - 1
    long long min_value = is_signed ? -(1LL << (bit_width - 1)) : 0;     // -2^(n-1) or 0

    if (hexString.substr(0, 2) != "0x")
    {
        // Assume decimal input
        try
        {
            decimalValue = stoi(hexString);
            if (decimalValue >= min_value && decimalValue <= max_value)
                return decimalValue;
            else
            {
                string errorMsg = "Typed IMMEDIATE_VALUE is invalid. Out of limit (" + 
                                  to_string(min_value) + ", " + to_string(max_value) + ")";
                (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, errorMsg);
                (*output_error).PrintError();
                exit(INVALID_IMMEDIATE_VALUE);
                return INT_MIN;
            }
        }
        catch (const exception &e)
        {
            (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Typed IMMEDIATE_VALUE is invalid. Invalid decimal format");
            (*output_error).PrintError();
            exit(INVALID_IMMEDIATE_VALUE);
            return INT_MIN;
        }
    }
    else if (hexString.substr(0, 2) == "0x")
    {
        hexString = hexString.substr(2);
        decimalValue = 0;
        int power = hexString.length() - 1;
        for (char c : hexString)
        {
            int digit;
            if (c >= '0' && c <= '9')
                digit = c - '0';
            else if (c >= 'a' && c <= 'f')
                digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                digit = c - 'A' + 10;
            else
            {
                (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Typed IMMEDIATE_VALUE is invalid. Invalid hex letters");
                (*output_error).PrintError();
                exit(INVALID_IMMEDIATE_VALUE);
                return INT_MIN;
            }
            decimalValue += digit * pow(16, power);
            power--;
        }
        // Check bit width (number of hex digits <= ceil(bit_width / 4))
        if (hexString.length() > (bit_width + 3) / 4)
        {
            (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Typed IMMEDIATE_VALUE exceeds " + 
                                       to_string(bit_width) + "-bit limit");
            (*output_error).PrintError();
            exit(INVALID_IMMEDIATE_VALUE);
            return INT_MIN;
        }
        if (decimalValue >= min_value && decimalValue <= max_value)
            return decimalValue;
        else
        {
            string errorMsg = "Typed IMMEDIATE_VALUE is invalid. Out of limit (" + 
                              to_string(min_value) + ", " + to_string(max_value) + ")";
            (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, errorMsg);
            (*output_error).PrintError();
            exit(INVALID_IMMEDIATE_VALUE);
            return INT_MIN;
        }
    }
    else
    {
        (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Typed IMMEDIATE_VALUE is invalid. Neither Hex nor Integer");
        (*output_error).PrintError();
        exit(INVALID_IMMEDIATE_VALUE);
        return INT_MIN;
    }
}
const int Normal_XNum_Parameter(const string &given_parameter, Error *output_error)
{
    int reg_number;
    if (given_parameter[0] == 'x')
    {
        // Try catch if string it not fully integer
        try
        {
            if (isNumber(given_parameter.substr(1, given_parameter.length() - 1)))
            {
                // Getting register number
                reg_number = stoi(given_parameter.substr(1, given_parameter.length() - 1));
                // Throwing exception if register number is > 31
                if (reg_number > 31)
                    throw invalid_argument("");
                // Reading if it is valid
                return reg_number;
            }
            else
                throw invalid_argument("");
        }
        catch (exception e)
        {
            // Altering error to invalid register
            (*output_error).AlterError(INVALID_REGISTER, "Typed Register is invalid");
            (*output_error).PrintError();
            // Exiting with error code
            exit(INVALID_REGISTER);
        }
    }
    else
    {
        // Altering error to invalid register
        (*output_error).AlterError(INVALID_REGISTER, "Typed Register is invalid");
        (*output_error).PrintError();
        // Exiting with error code
        exit(INVALID_REGISTER);
    }
}

void Bracketed_Immediate_Parameter(RISC_V_Instructions *Current_Instruction, const string &given_parameter, Error *output_error)
{
    string temp_word;
    int reg_number;
    stringstream temp(given_parameter);
    try
    {
        getline(temp, temp_word, '(');
        // Removing trailing and leading spaces
        temp_word = trim(temp_word);
        // Checking if number
        if (isNumber(temp_word) || temp_word.substr(0, 2) == "0x")
        {
            (*Current_Instruction).imm = Calculate_Immediate(temp_word,12 ,true, output_error);
        }
        else
            throw invalid_argument("");

        getline(temp, temp_word, '(');
        // Removing trailing and leading spaces
        if (temp_word[0] == 'x')
        {
            temp_word = trim(temp_word.substr(1, temp_word.length() - 2));
            reg_number = stoi(temp_word);
            // Throwing exception if register number is > 31
            if (reg_number > 31)
                throw invalid_argument("");
            // Storing
            (*Current_Instruction).rs1 = reg_number;
        }
        else
        {
            // Altering error to invalid register
            (*output_error).AlterError(INVALID_REGISTER, "Typed Register is invalid");
            (*output_error).PrintError();
            // Exiting with error code
            exit(INVALID_REGISTER);
        }
    }
    catch (exception e)
    {
        // Altering error to invalid register

        (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Immediate Value " + temp_word + " is invalid");
        (*output_error).PrintError();
        // Exiting with error code
        exit(INVALID_IMMEDIATE_VALUE);
    }
}

int Label_Offset_Parameter(unordered_map<string, long long> labels_PC, const string &label, long long program_counter, Error *output_error)
{
   
  
   

    // This function will return the number of instructions to skip
   

    if (labels_PC.find(label) != labels_PC.end())
    {
       
        return (labels_PC[label] - program_counter );
    }
    else
    {
        // Altering error to invalid register
       
        (*output_error).AlterError(INVALID_LABEL, "Typed Branch Target is invalid");
        (*output_error).PrintError();
        // Exiting with error code
        exit(INVALID_LABEL);
        return -1;
    }
}


// Function to check if it a valid instruction
const RISC_V_Instructions InitializeInstruction(const unordered_map<string, long long> labels_PC,  string &inst, Error *output_error, int program_counter )
{
  
    RISC_V_Instructions Current_Instruction;
    Current_Instruction.Instruction = inst;
    size_t comment_pos = inst.find('#');
    if (comment_pos != string::npos) {
        inst = inst.substr(0, comment_pos); // Keep only the part before '#'
    }
    inst = trim(inst); 

   
   
    // String stream to get word by word
    stringstream ss(inst);
    // Just some varibles for specific purpose
    string temp_word = "";
    int reg_number = -1, values = 0;
    // Character by Character
    // Getting first word
    ss >> temp_word;
    // This is for R-type instructions
    if (R_opcode_map.find(temp_word) != R_opcode_map.end())
    {
        // func3 and func7 Initialization
        Current_Instruction.func3 = func3_map[temp_word];
        Current_Instruction.func7 = func7_map[temp_word];
        Current_Instruction.type = 1;
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = R_opcode_map[temp_word];
        // Getting rest of the values from the instruction
        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);

            // Reading if it is valid
            if (values == 1)
                Current_Instruction.rd = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 2)
                Current_Instruction.rs1 = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 3)
                Current_Instruction.rs2 = Normal_XNum_Parameter(temp_word, output_error);
            else
            {
                // Altering error to invalid Syntax
            
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    // I Type
    else if (I_opcode_map.find(temp_word) != I_opcode_map.end())
    {
        // Initializing func3
        Current_Instruction.func3 = func3_map[temp_word];
        Current_Instruction.type = 2;
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = I_opcode_map[temp_word];
        // Getting rest of the values from the instruction
        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);
            // Reading register value
            // If lw, ld, lh, lb
            if (Current_Instruction.OpCode == "0000011" && values == 2)
                Bracketed_Immediate_Parameter(&Current_Instruction, temp_word, output_error);
            // addi, andi, ori, jalr
            else if (temp_word[0] == 'x' && values != 3)
            {
                // Reading if it is valid
                if (values == 1)
                    Current_Instruction.rd = Normal_XNum_Parameter(temp_word, output_error);
                else if (values == 2)
                    Current_Instruction.rs1 = Normal_XNum_Parameter(temp_word, output_error);
            }
            else if (values == 3)
            {
                // Storing Immediate value
                if(temp_word[temp_word.length() - 1] == 'u'){
                    Current_Instruction.imm = Calculate_Immediate(temp_word,12 , false ,output_error);

                }
              else{
                Current_Instruction.imm = Calculate_Immediate(temp_word,12 , true ,output_error);
              }
            }
            else
            {
                // Altering error to invalid Syntax
               
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    // S Type
    else if (S_opcode_map.find(temp_word) != S_opcode_map.end())
    {
        // Initializing func3
        Current_Instruction.func3 = func3_map[temp_word];
        Current_Instruction.type = 3;
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = S_opcode_map[temp_word];
        // Getting rest of the values from the instruction
        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);
            // Reading register value
            if (temp_word[0] == 'x' && values != 2)
            {
                // Reading if it is valid
                if (values == 1)
                    Current_Instruction.rs2 = Normal_XNum_Parameter(temp_word, output_error);
            }
            else if (values == 2)
                Bracketed_Immediate_Parameter(&Current_Instruction, temp_word, output_error);
            else
            {
                // Altering error to invalid Syntax
               
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    // SB Type
    else if (SB_opcode_map.find(temp_word) != SB_opcode_map.end())
    {
        // Initializing func3
        Current_Instruction.func3 = func3_map[temp_word];
        Current_Instruction.type = 4;
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = SB_opcode_map[temp_word];
        // Getting rest of the values from the instruction
        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);

            // Reading if it is valid
            if (values == 1)
                Current_Instruction.rs1 = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 2)
                Current_Instruction.rs2 = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 3){
                getline(ss, temp_word, '#');
                temp_word = trim(temp_word);
                
                Current_Instruction.imm = Label_Offset_Parameter(labels_PC, temp_word, program_counter, output_error);
             
            }
            else
            {
                // Altering error to invalid Syntax
                
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    // U Type
    else if (U_opcode_map.find(temp_word) != U_opcode_map.end())
    {
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = U_opcode_map[temp_word];
        Current_Instruction.type = 5;

        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);

            // Reading if it is valid
            if (values == 1)
                Current_Instruction.rd = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 2)
            {
                if (temp_word.substr(0, 2) == "0x")
                    Current_Instruction.imm = Calculate_Immediate(temp_word,20 , false, output_error);
                else if (stoi(temp_word) <= 1048576)
                    Current_Instruction.imm = stoi(temp_word);
                else
                {
                    // Altering error to invalid IMMEDIATE_VALUE
                   
                    (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Typed Immediate value is invalid");
                    (*output_error).PrintError();
                    // Exiting with error code
                    exit(INVALID_IMMEDIATE_VALUE);
                }
            }

            else
            {
                // Altering error to invalid Syntax
            
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    else if (UJ_opcode_map.find(temp_word) != UJ_opcode_map.end())
    {
        // Reading Op Code for the instruction
        Current_Instruction.OpCode = UJ_opcode_map[temp_word];
        Current_Instruction.type = 6;
        while (!ss.eof()) // Run file end of instruction
        {
            values++;
            // Spliting and reading word by delimiter as comma
            getline(ss, temp_word, ',');
            // Removing trailing and leading spaces
            temp_word = trim(temp_word);

            // Reading if it is valid
            if (values == 1)
                Current_Instruction.rd = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 2){
               
                Current_Instruction.imm = Label_Offset_Parameter(labels_PC, temp_word, program_counter, output_error);
            }
            else
            {
                // Altering error to invalid Syntax
               
                (*output_error).AlterError(ERROR_SYNTAX, "Typed Syntax is invalid");
                (*output_error).PrintError();
                // Exiting with error code
                exit(ERROR_SYNTAX);
            }
        }
    }
    else if (I_new_opcode.find(temp_word) != I_new_opcode.end()) { // Assuming I_new_opcode is for shift-immediate
        Current_Instruction.func3 = func3_map[temp_word];
        Current_Instruction.type = 7; // New type for shift-immediate
        Current_Instruction.OpCode = I_new_opcode[temp_word];
        
        // Set func7 based on instruction
        if (temp_word == "slli" || temp_word == "srli") {
            Current_Instruction.func7 = "0000000";
        } else if (temp_word == "srai") {
            Current_Instruction.func7 = "0100000";
        }

        while (!ss.eof()) {
            values++;
            getline(ss, temp_word, ',');
            temp_word = trim(temp_word);
            if (values == 1)
                Current_Instruction.rd = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 2)
                Current_Instruction.rs1 = Normal_XNum_Parameter(temp_word, output_error);
            else if (values == 3) {
                // shamt is 5 bits (0-31), unsigned
                Current_Instruction.imm = Calculate_Immediate(temp_word, 5, false, output_error);
                if (Current_Instruction.imm < 0 || Current_Instruction.imm > 31) {
                    (*output_error).AlterError(INVALID_IMMEDIATE_VALUE, "Shift amount '" + temp_word + "' must be 0-31");
                    (*output_error).PrintError();
                    exit(INVALID_IMMEDIATE_VALUE);
                }
            }
            }
    }
    // Not an instruction
    else
    {
        // Altering error to invalid instruction
        (*output_error).AlterError(INVALID_INSTRUCTION, "Typed Instruction " + temp_word + " is invalid");
        (*output_error).PrintError();
        // Exiting with error code
        exit(INVALID_INSTRUCTION);
    }
    return Current_Instruction;
}

#endif

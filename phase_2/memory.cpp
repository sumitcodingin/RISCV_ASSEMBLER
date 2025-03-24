#include "memory.h"
#include <iostream> // For input/output
#include <fstream>  // For file handling

using namespace std;
map<int, int> memory; // Our global memory map

void writeMemory(int address, int value)
{
    memory[address] = value;
}

int readMemory(int address)
{
    return memory[address];
}

// Function to save memory data to a binary file
void saveMemoryToFile(const string &filename)
{
    ofstream outFile(filename);
    if (!outFile)
    {
        cerr << "Error: Could not open file for writing!\n";
        return;
    }

    for (const auto &pair : memory)
    {
        outFile << "0x" << hex << pair.first << " " << pair.second << endl;
    }
    outFile.close();
}

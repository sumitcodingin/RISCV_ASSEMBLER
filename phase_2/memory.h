#ifndef MEMORY_H
#define MEMORY_H

#include <map>
#include <iostream>

extern std::map<int, int> memory;

void writeMemory(int address, int value);
int readMemory(int address);
void saveMemoryToFile(const std::string &filename);

#endif

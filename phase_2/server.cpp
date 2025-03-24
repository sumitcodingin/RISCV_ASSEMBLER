#include "httplib.h"         // Lightweight HTTP library
#include <fstream>           // File handling
#include <iostream>          // Console logging
#include <cstdlib>           // For system() to run assembler
#include <string>            // For string operations
#include <unordered_map>     // For storing key-value pairs
#include <nlohmann/json.hpp> // JSON library (Download from https://github.com/nlohmann/json)

using json = nlohmann::json;
using namespace httplib;
using namespace std;

Server svr;

// Function to read and parse register.txt (Key-Value format)
json readFile(const string &filename)
{
    unordered_map<string, string> keyValuePairs;
    ifstream file(filename);

    if (!file.is_open())
    {
        cerr << "Error: Cannot open " << filename << endl;
        return json{{"error", "File not found"}};
    }

    string key, value;
    while (file >> key >> value)
    { // Read key-value pairs (space-separated)
        keyValuePairs[key] = value;
    }

    file.close();
    return keyValuePairs;
}

// Handle POST request for Assembly to Machine Code conversion
void handleAssemblyRequest(const Request &req, Response &res)
{
    string assemblyCode = req.body; // Get Assembly code from frontend

    // Save received assembly code to a file
    ofstream outFile("input.asm");
    outFile << assemblyCode;
    outFile.close();

    // Run the assembler (which generates 'output.mc' machine code file)
    system("./main input.asm output.mc FinalOutput.txt memory.mem register.reg PC.pc");

    // Read the machine code output file
    ifstream inFile("FinalOutput.txt");
    if (!inFile)
    {
        cerr << "Error: Could not open output.mc file!" << endl;
        res.set_content("Error: Could not read output file", "text/plain");
        return;
    }

    string machineCode((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
    inFile.close();

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");

    // Send the machine code back to the frontend
    res.set_content(machineCode, "text/plain");
}

// Handle GET request to fetch register data
void handleRegisterRequest(const Request &req, Response &res)
{
    json registerData = readFile("register.reg"); // Fixed filename

    // cout << "called Me without calling me" << endl;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");

    res.set_content(registerData.dump(), "application/json");
}

// Handle POST request to update register values
void handleRegisterUpdate(const Request &req, Response &res)
{
    json updatedRegisters;

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    try
    {
        updatedRegisters = json::parse(req.body);
    }
    catch (const json::parse_error &e)
    {
        res.status = 400;
        res.set_content("Invalid JSON format", "text/plain");
        return;
    }

    // Save updated registers to file
    ofstream outFile("register.mem");
    if (!outFile.is_open())
    {
        res.set_content("Error: Could not open register.txt for writing", "text/plain");
        return;
    }

    for (auto &[key, value] : updatedRegisters.items())
    {
        outFile << key << " " << value << "\n";
    }

    outFile.close();
    res.set_content("Registers updated successfully", "text/plain");
}

// Handle GET request to fetch memory data
void handleMemoryRequest(const Request &req, Response &res)
{
    json memoryData = readFile("memory.mem");

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");

    res.set_content(memoryData.dump(), "application/json");
}

// Handle POST request to update memory values
void handleMemoryUpdate(const Request &req, Response &res)
{
    json updatedMemory;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    try
    {
        updatedMemory = json::parse(req.body);
    }
    catch (const json::parse_error &e)
    {
        res.status = 400;
        res.set_content("Invalid JSON format", "text/plain");
        return;
    }

    // Save updated memory to file
    ofstream outFile("memory.mem");
    if (!outFile.is_open())
    {
        res.set_content("Error: Could not open memory.txt for writing", "text/plain");
        return;
    }

    for (auto &[address, data] : updatedMemory.items())
    {
        outFile << address << " " << data << "\n";
    }

    outFile.close();
    res.set_content("Memory updated successfully", "text/plain");
}

int main()
{
    cout << "Server starting on http://localhost:3001" << endl;

    svr.Post("/assemble", handleAssemblyRequest);
    svr.Get("/registers", handleRegisterRequest);
    svr.Post("/update-registers", handleRegisterUpdate);
    svr.Get("/memory", handleMemoryRequest);
    svr.Post("/update-memory", handleMemoryUpdate);

    svr.listen("0.0.0.0", 3001);
}

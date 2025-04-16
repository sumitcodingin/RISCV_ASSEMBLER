#ifndef AUXILIARY_FUNCTIONS_H
#define AUXILIARY_FUNCTIONS_H

#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <bitset>
#include <unordered_map>
#include <cmath>

using namespace std;

// Function to remove leading and trailing spaces from a string
const string trim(const string &str) {
    try {
        auto start = find_if_not(str.begin(), str.end(), [](unsigned char c) { return isspace(c); });
        auto end = find_if_not(str.rbegin(), str.rend(), [](unsigned char c) { return isspace(c); }).base();
        return (start >= end) ? "" : string(start, end);
    } catch (const exception& e) {
        cerr << "Error in trim: " << e.what() << endl;
        return "";
    }
}

// Check if string is integer (including negative numbers)
const bool isNumber(string s) {
    if(s.empty()) return false;
    
    size_t start = 0;
    if(s[0] == '-') start = 1;
    
    for (size_t i = start; i < s.length(); i++) {
        if (!isdigit(s[i])) return false;
    }
    return true;
}

// function to convert decimal to hexadecimal
string decToHexa(long long n) {
    try {
        if (n == 0) return "0x0";
        
        bool negative = n < 0;
        if (negative) n = -n;
        
        string ans = "";
        while (n != 0) {
            int rem = n % 16;
            char ch = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
            ans = ch + ans;
            n /= 16;
        }
        
        return (negative ? "-0x" : "0x") + (ans.empty() ? "0" : ans);
    } catch (const exception& e) {
        cerr << "Error in decToHexa: " << e.what() << endl;
        return "0x0";
    }
}

// Function to create map between binary number and its equivalent hexadecimal 
void createMap(unordered_map<string, char> *um) {
    (*um)["0000"] = '0'; (*um)["0001"] = '1'; (*um)["0010"] = '2'; (*um)["0011"] = '3';
    (*um)["0100"] = '4'; (*um)["0101"] = '5'; (*um)["0110"] = '6'; (*um)["0111"] = '7';
    (*um)["1000"] = '8'; (*um)["1001"] = '9'; (*um)["1010"] = 'A'; (*um)["1011"] = 'B';
    (*um)["1100"] = 'C'; (*um)["1101"] = 'D'; (*um)["1110"] = 'E'; (*um)["1111"] = 'F';
}

// function to find hexadecimal equivalent of binary 
string binToHexa(string bin) {
    try {
        if(bin.empty()) return "0x0";
        
        // Pad with zeros to make length multiple of 4
        while(bin.length() % 4 != 0) {
            bin = '0' + bin;
        }
        
        unordered_map<string, char> bin_hex_map;
        createMap(&bin_hex_map);
        
        string hex = "";
        for(size_t i = 0; i < bin.length(); i += 4) {
            string chunk = bin.substr(i, 4);
            if(bin_hex_map.find(chunk) != bin_hex_map.end()) {
                hex += bin_hex_map[chunk];
            } else {
                throw runtime_error("Invalid binary digit sequence");
            }
        }
        
        return "0x" + hex;
    } catch (const exception& e) {
        cerr << "Error in binToHexa: " << e.what() << endl;
        return "0x0";
    }
}

// function to convert decimal to binary with specified length
string decToBinary_Len(long long n, int len) {
    try {
        string binary;
        bool isNegative = n < 0;
        
        if(isNegative) {
            // Handle two's complement for negative numbers
            unsigned long long maxVal = (1ULL << len);
            n = maxVal + n;
        }
        
        // Convert to binary
        for(int i = len - 1; i >= 0; i--) {
            binary = (n & 1 ? "1" : "0") + binary;
            n >>= 1;
        }
        
        if(binary.length() > len) {
            binary = binary.substr(binary.length() - len);
        }
        
        while(binary.length() < len) {
            binary = "0" + binary;
        }
        
        return binary;
    } catch (const exception& e) {
        cerr << "Error in decToBinary_Len: " << e.what() << endl;
        return string(len, '0');
    }
}

#endif

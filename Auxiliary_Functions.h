#ifndef AUXILIARY_FUNCTIONS_H
#define AUXILIARY_FUNCTIONS_H

#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <bitset>

using namespace std;

// Function to remove leading and trailing spaces from a string
const string trim(const string &str)
{
    // Find the first non-space character
    auto start = find_if_not(str.begin(), str.end(), [](unsigned char c)
                             { return isspace(c); });

    // Find the last non-space character
    auto end = find_if_not(str.rbegin(), str.rend(), [](unsigned char c)
                           { return isspace(c); })
                   .base();

    // If start and end are the same, the string is either all spaces or empty
    if (start == end)
        return "";

    // Return the substring without leading and trailing spaces
    return string(start, end);
}

// Check if string is integer
const bool isNumber(string s)
{
    for (int i = 0; i < s.length(); i++)
        if (isdigit(s[i]) == false)
            return false;
 
    return true;
}

// function to convert decimal to hexadecimal
string decToHexa(int n)
{
    // ans string to store hexadecimal number
    string ans = "";
   
    while (n != 0) {
        // remainder variable to store remainder
        int rem = 0;
         
        // ch variable to store each character
        char ch;
        // storing remainder in rem variable.
        rem = n % 16;
 
        // check if temp < 10
        if (rem < 10) {
            ch = rem + 48;
        }
        else {
            ch = rem + 55;
        }
         
        // updating the ans string with the character variable
        ans += ch;
        n = n / 16;
    }
     
    // reversing the ans string to get the final result
    int i = 0, j = ans.size() - 1;
    while(i <= j)
    {
      swap(ans[i], ans[j]);
      i++;
      j--;
    }
    if(ans == "")
        ans = "0";
    return ("0x"+ans);
}

// Function to create map between binary 
// number and its equivalent hexadecimal 
void createMap(unordered_map<string, char> *um) 
{ 
    (*um)["0000"] = '0'; 
    (*um)["0001"] = '1'; 
    (*um)["0010"] = '2'; 
    (*um)["0011"] = '3'; 
    (*um)["0100"] = '4'; 
    (*um)["0101"] = '5'; 
    (*um)["0110"] = '6'; 
    (*um)["0111"] = '7'; 
    (*um)["1000"] = '8'; 
    (*um)["1001"] = '9'; 
    (*um)["1010"] = 'A'; 
    (*um)["1011"] = 'B'; 
    (*um)["1100"] = 'C'; 
    (*um)["1101"] = 'D'; 
    (*um)["1110"] = 'E'; 
    (*um)["1111"] = 'F'; 
} 
  
// function to find hexadecimal  
// equivalent of binary 
string binToHexa(string bin) 
{ 
    int l = bin.size(); 
    int t = bin.find_first_of('.'); 
      
    // length of string before '.' 
    int len_left = t != -1 ? t : l; 
      
    // add min 0's in the beginning to make 
    // left substring length divisible by 4  
    for (int i = 1; i <= (4 - len_left % 4) % 4; i++) 
        bin = '0' + bin; 
      
    // if decimal point exists     
    if (t != -1)     
    { 
        // length of string after '.' 
        int len_right = l - len_left - 1; 
          
        // add min 0's in the end to make right 
        // substring length divisible by 4  
        for (int i = 1; i <= (4 - len_right % 4) % 4; i++) 
            bin = bin + '0'; 
    } 
      
    // create map between binary and its 
    // equivalent hex code 
    unordered_map<string, char> bin_hex_map; 
    createMap(&bin_hex_map); 
      
    int i = 0; 
    string hex = ""; 
      
    while (1) 
    { 
        // one by one extract from left, substring 
        // of size 4 and add its hex code 
        hex += bin_hex_map[bin.substr(i, 4)]; 
        i += 4; 
        if (i == bin.size()) 
            break; 
              
        // if '.' is encountered add it 
        // to result 
        if (bin.at(i) == '.')     
        { 
            hex += '.'; 
            i++; 
        } 
    } 
      
    // required hexadecimal number 
    return ("0x"+hex);     
} 

// function to convert decimal to binary 
string decToBinary_Len(int n, int len) 
{ 
    string Bin_Number = "";
    // counter for binary array 
    int i = 0; 
    while (n > 0) { 
  
        // storing remainder in binary array 
        Bin_Number += to_string(n % 2);
        n = n / 2; 
        i++; 
    }
    while(Bin_Number.length() < len)
    {
        Bin_Number += "0";
    }
    reverse(Bin_Number.begin(), Bin_Number.end());
    return Bin_Number;
}


#endif
#include "TextUtils.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cctype>

std::string EatWhite(std::string str, char delim = 0)
{
    std::string ret, tail = "";
    unsigned length = str.length();
    if (delim != 0) /// use delimiter
    {
        size_t pos = str.find_first_of(delim);
        if (pos != std::string::npos)
        {
            length = pos;
            tail = str.substr(pos);
        }
    }
    for (unsigned i = 0; i < length; ++i)
    {
        if (!isspace(str[i]))
            ret += str[i];
    }
    ret += tail;
    return ret;
}

std::string Trim(std::string str)
{
    std::string ret = str, wspc(" \t\f\v\n\r");
    size_t pos = ret.find_first_not_of(wspc);
    if (pos != std::string::npos)
    {
        ret = ret.substr(pos);
    }
    else
    {
        return "";
    }
    pos = ret.find_last_not_of(wspc);
    ret = ret.substr(0, pos + 1);
    return ret;
}

size_t SplitAt(char ch, std::string in, std::string& out1, std::string& out2)
{
    size_t pos = in.find(ch);
    if (pos != std::string::npos)
    {
        out1 = Trim(in.substr(0, pos));
        out2 = Trim(in.substr(pos + 1));
    }
    else
    {
        out1 = Trim(in);
        out2 = "";
    }
    return pos;
}

const char chLookup[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

std::string MakeTextBlock(char *data, size_t dataSize)
{
    std::string out = "";
    for (unsigned i = 0; i < dataSize; ++i)
    {
        if ((i%16 == 0) && (i != 0))
            out += '\n';
        uint8_t ch = data[i];
        uint8_t up = ((ch & 0xF0) >> 4);
        uint8_t lw =  (ch & 0x0F);
        out += chLookup[up];
        out += chLookup[lw];
        out += ' ';
    }
    out += '\n';
    return out;
}

std::string GetFilename(std::string str)
{
    unsigned found = str.find_last_of("/\\");
    return str.substr(found + 1);
}

std::string GetFilenameNoExt(std::string str)
{
    unsigned found = str.find_last_of("/\\");
    return str.substr(0, str.find_last_of(".")).substr(found + 1);
}

std::string GetStringValue(const std::string& TextBuffer)
{
    std::string str = TextBuffer;
    if (str.length() < 1)
        return "";
    str = str.substr(0, str.find_first_of("\n")); /// get first line
    str = Trim(str); /// remove leading and trailing white-spaces
    if (str.find('\"') != std::string::npos) /// remove ""
    {
        str = str.substr(str.find_first_not_of("\""));
        str = str.substr(0, str.find_last_not_of("\"") + 1);
    }
    return str;
}

std::vector<char> GetDataChunk(const std::string& TextBuffer)
{
    std::vector<char> data;
    if (TextBuffer.length() < 1)
        return data;
    std::istringstream ss(TextBuffer);
    while (ss.good())
    {
        int byte;
        ss >> std::hex >> byte;
        if (!ss.fail() && !ss.bad())
            data.push_back(byte);
    }
    return data;
}

int GetIntValue(const std::string& TextBuffer)
{
    int val = 0;
    std::string str = ::GetStringValue(TextBuffer);
    if (str.length() < 1)
        return 0;
    //val = std::stoi(str, nullptr, 0);
    std::istringstream ss(str);
    /*if (str.find("0x") != std::string::npos)
        ss >> std::hex >> val;
    else
        ss >> std::dec >> val;*/
    ss >> val;
    return val;
}

unsigned GetUnsignedValue(const std::string& TextBuffer)
{
    unsigned val = 0;
    std::string str = ::GetStringValue(TextBuffer);
    if (str.length() < 1)
        return 0;
    std::istringstream ss(str);
    if (str.find("0x") != std::string::npos)
        ss >> std::hex >> val;
    else
        ss >> std::dec >> val;
    return val;
}

float GetFloatValue(const std::string& TextBuffer)
{
    float val = 0;
    std::string str = ::GetStringValue(TextBuffer);
    if (str.length() < 1)
        return 0;
    std::istringstream ss(str);
    ss >> val;
    return val;
}

std::string FormatHEX(uint32_t val)
{
    char ch[255];
    sprintf(ch, "0x%08X", val);
    return std::string(ch);
}

std::string FormatHEX(uint16_t val)
{
    char ch[255];
    sprintf(ch, "0x%04X", val);
    return std::string(ch);
}

std::string FormatHEX(uint8_t val)
{
    char ch[255];
    sprintf(ch, "0x%02X", val);
    return std::string(ch);
}

std::string FormatHEX(float val)
{
    std::string ret = "0x";
    uint8_t *p = reinterpret_cast<uint8_t*>(&val);
    for (unsigned i = 0; i < 4; ++i)
    {
        char ch[255];
        sprintf(ch, "%02X", p[3 - i]);
        ret += ch;
    }
    return ret;
}

std::string FormatHEX(FGuid GUID)
{
    char ch[255];
    sprintf(ch, "%08X%08X%08X%08X", GUID.GUID_A, GUID.GUID_B, GUID.GUID_C, GUID.GUID_D);
    return std::string(ch);
}

std::string FormatHEX(UNameIndex NameIndex)
{
    char ch[255];
    sprintf(ch, "0x%08X (Index) 0x%08X (Numeric)", NameIndex.NameTableIdx, NameIndex.Numeric);
    return std::string(ch);
}

std::string FormatHEX(uint32_t L, uint32_t H)
{
    char ch[255];
    sprintf(ch, "0x%08X%08X", H, L);
    return std::string(ch);
}

std::string FormatHEX(std::vector<char> DataChunk)
{
    std::string ret;
    for (unsigned i = 0; i < DataChunk.size(); ++i)
    {
        char ch[255];
        sprintf(ch, "%02X", (uint8_t)DataChunk[i]);
        ret += std::string(ch) + " ";
    }
    return ret;
}

std::string FormatHEX(char* DataChunk, size_t size)
{
    std::string ret;
    for (unsigned i = 0; i < size; ++i)
    {
        char ch[255];
        sprintf(ch, "%02X", (uint8_t)DataChunk[i]);
        ret += std::string(ch) + " ";
    }
    return ret;
}

std::string FormatHEX(std::string DataString)
{
    std::string ret;
    for (unsigned i = 0; i < DataString.size(); ++i)
    {
        char ch[255];
        sprintf(ch, "%02X", (uint8_t)DataString[i]);
        ret += std::string(ch) + " ";
    }
    return ret;
}
